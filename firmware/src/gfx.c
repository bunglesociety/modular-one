/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Small-canvas renderer (no full framebuffer). Draw text/lines into
 * an off-screen strip, then blit it in one DMA transfer. Pixels are stored
 * byte-swapped (the panel takes RGB565 big-endian). */
typedef struct {               /* proportional, see tools/gen_font.py */
    uint8_t h;
    uint8_t pad;               /* bitmap starts this many pixels left of the pen */
    uint8_t first, last;
    const uint8_t *adv;        /* advance per glyph */
    const uint8_t *bw;         /* bitmap width per glyph (starts FONT_PAD left of the pen) */
    const uint16_t *off;       /* byte offset of each glyph */
    const uint8_t *data;
} felucca_font_t;
#include "felucca_font.h"

#define CV_MAX (240u * 124u)      /* the graph strip is 240 x 124 */
static uint16_t cv_px[CV_MAX] __attribute__((section(".pool")));
static uint32_t cv_w, cv_h;
static int32_t cv_oy;            /* y offset for graph drawing */

#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define C_BLACK 0x0000u
#define C_WHITE 0xFFFFu              /* accent only: what is being touched / where we are */
/* The screen is five steps of one colour, darkest to brightest, plus white.
 * Palettes are picked in the HOME-hold menu (COLOR). */
typedef struct {
    const char *name;
    uint16_t c[5];
} palette_t;
static const palette_t PALETTES[] = {
    {"GREEN", {RGB(0, 40, 12), RGB(0, 84, 30), RGB(16, 140, 54), RGB(56, 200, 92), RGB(120, 255, 146)}},
    {"AMBER", {RGB(60, 26, 0), RGB(110, 50, 0), RGB(170, 82, 0), RGB(225, 120, 8), RGB(255, 166, 40)}},
    {"CYAN", {RGB(0, 30, 50), RGB(0, 62, 96), RGB(16, 112, 160), RGB(56, 172, 222), RGB(140, 222, 255)}},
    {"RED", {RGB(52, 8, 8), RGB(100, 18, 14), RGB(170, 36, 26), RGB(226, 64, 48), RGB(255, 112, 92)}},
    {"MONO", {RGB(40, 40, 40), RGB(80, 80, 80), RGB(130, 130, 130), RGB(186, 186, 186), RGB(226, 226, 226)}},
};
#define NPALETTES (sizeof(PALETTES) / sizeof(PALETTES[0]))
static uint16_t pal[5];
#define C_LINE pal[0]                /* 1 rules, separators */
#define C_DIM pal[1]                 /* 2 inactive, empty steps, units */
#define C_GRAY pal[2]                /* 3 labels */
#define C_AMB pal[3]                 /* 4 secondary text */
#define C_HI pal[4]                  /* 5 values, curves */

/* Modular One: one screen, the green of the first handheld console. Four shades (lightest to darkest), the
 * pixel letters (Silkscreen: LAB at its design size, G at 2x, the pixel grid of the town), square boxes and
 * block gauges. The knobs take the two dark shades in turn. The five steps above stay the MONO ones for
 * the few legacy screens (calibration, crash, update). */
#define GB0 RGB(155, 188, 15)        /* lightest: the screen */
#define GB1 RGB(139, 172, 15)
#define GB2 RGB(48, 98, 48)
#define GB3 RGB(15, 56, 15)          /* darkest: the ink */
static uint8_t m1_bold;              /* bold letters: the small letters drawn twice, a pixel apart */

static const uint16_t KC[4] = {GB3, GB2, GB3, GB2};
static const uint16_t UIC[6] = {GB0, GB1, GB1, GB2, GB2, GB3};   /* ground, panel, rules, dim, grey, ink */
#define ui_light 1u
#define ui_style 3u
#define ui_drawn 0u
static const felucca_font_t *const FN_NAME = &FONT_G, *const FN_VAL = &FONT_G, *const FN_MID = &FONT_G,
                            *const FN_VALS = &FONT_LAB, *const FN_SM = &FONT_LAB, *const FN_LAB = &FONT_LAB;

static void palette_set(uint32_t i)
{
    uint32_t k;
    (void)i;
    for (k = 0; k < 5u; k++)
        pal[k] = PALETTES[4].c[k];
}

static inline uint16_t swap16(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }

static void cv_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    uint32_t i, n;
    if (w * h > CV_MAX)
        h = CV_MAX / w;
    lcd_sync();                     /* the last blit may still read cv_px */
    cv_w = w;
    cv_h = h;
    n = w * h;
    for (i = 0; i < n; i++)
        cv_px[i] = swap16(bg);
}

/* the handheld screen has four shades and nothing between them: a colour between two shades (a blend, an
 * anti-aliased edge, a mix) becomes the two, in a fine 4x4 ordered pattern on the 2x2 pixel grid of the
 * town (lo[] the brightness of each shade, darkest first: a colour darker than lo[0] is the darkest).
 * px byte-swapped, screen position (sx, sy) for the pattern */
static const uint8_t GB_BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static const uint16_t GB_SH[4] = {GB3, GB2, GB1, GB0};          /* darkest first */
static const uint8_t GB_LUMA_UI[5] = {39, 77, 144, 158, 16};
static inline uint32_t luma565(uint32_t c)
{
    return (((c >> 11) << 3) * 77u + (((c >> 5) & 63u) << 2) * 150u + ((c & 31u) << 3) * 29u) >> 8;
}
/* lo[4]: as above; lo[4] the width of the pattern band between two shades, 16 = all the way, 8 = only the middle
 * half (flat areas stay flat) */
static void gb_quantize(uint16_t *px, uint32_t w, uint32_t h, uint32_t sx, uint32_t sy, const uint8_t *lo)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint16_t *q = &px[j * w + i];
            uint32_t c = swap16(*q), y, k, t;
            if (c == GB_SH[0] || c == GB_SH[1] || c == GB_SH[2] || c == GB_SH[3])
                continue;
            y = luma565(c);
            for (k = 0; k < 2u && y >= lo[k + 1u]; k++)
                ;
            if (y <= lo[0]) {
                *q = swap16(GB_SH[0]);
                continue;
            }
            if (y >= lo[3]) {
                *q = swap16(GB_SH[3]);
                continue;
            }
            t = (y - lo[k]) * 16u / (lo[k + 1u] - lo[k]);           /* 0..15 between shade k and k + 1 */
            if (lo[4] < 16u) {
                int32_t u = ((int32_t)t - 8) * 16 / (int32_t)lo[4] + 8;
                t = u < 0 ? 0u : u > 16 ? 16u : (uint32_t)u;
            }
            *q = swap16(GB_SH[k + (t > GB_BAYER[(((sy + j) >> 1) & 3u) * 4u + (((sx + i) >> 1) & 3u)] ? 1u : 0u)]);
        }
}

static void cv_blit(uint32_t x, uint32_t y)
{
    gb_quantize(cv_px, cv_w, cv_h, x, y, GB_LUMA_UI);
    lcd_blit(x, y, cv_w, cv_h, cv_px);
}

/* canvas rows r0 .. cv_h-1 only, to screen row y + r0 */
static void cv_blit_from(uint32_t x, uint32_t y, uint32_t r0)
{
    if (r0 < cv_h) {
        gb_quantize(cv_px + r0 * cv_w, cv_w, cv_h - r0, x, y + r0, GB_LUMA_UI);
        lcd_blit(x, y + r0, cv_w, cv_h - r0, cv_px + r0 * cv_w);
    }
}

static inline void cv_pset(int32_t x, int32_t y, uint16_t c)
{
    y += cv_oy;
    if ((uint32_t)x < cv_w && (uint32_t)y < cv_h)
        cv_px[(uint32_t)y * cv_w + (uint32_t)x] = swap16(c);
}

static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w, y1 = y + h + cv_oy, i;
    uint16_t sc = swap16(c);
    y += cv_oy;                             /* clipped once, then filled row by row */
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x1 > (int32_t)cv_w)
        x1 = (int32_t)cv_w;
    if (y1 > (int32_t)cv_h)
        y1 = (int32_t)cv_h;
    for (; y < y1; y++)
        for (i = x; i < x1; i++)
            cv_px[(uint32_t)y * cv_w + (uint32_t)i] = sc;
}

static void cv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, guard = 2000;
    while (guard--) {                       /* bounded: a line is never longer than 480 px */
        int32_t e2 = 2 * err;               /* both tests use the same error value */
        cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* glyph index of a character: lower case folds to upper case when the font
 * has none, anything missing (and C1 controls) draws as '?' */
static uint32_t glyph(const felucca_font_t *f, uint32_t ch)
{
    if (ch >= 'a' && ch <= 'z' && f->last < 'a')
        ch -= 32u;
    if (ch < f->first || ch > f->last || (ch >= 127u && ch < 160u))
        ch = '?';
    return ch - f->first;
}

/* text, alpha-blended onto black with colour c; returns the end x */
static int32_t cv_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    uint16_t ramp[16];
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u, a;
    for (a = 0; a < 16u; a++)
        ramp[a] = (uint16_t)(((r * a / 15u) << 11) | ((g * a / 15u) << 5) | (b * a / 15u));
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w, bpr;
        const uint8_t *gd;
        w = f->bw[gi];
        bpr = (w + 1u) / 2u;
        gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[gy * bpr + gx / 2u];
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v)
                    cv_pset(x - f->pad + (int32_t)gx, y + (int32_t)gy, ramp[v]);
            }
        x += f->adv[gi];
    }
    return x;
}

static int32_t text_w(const felucca_font_t *f, const char *s)
{
    int32_t w = 0;
    for (; *s; s++)
        w += f->adv[glyph(f, (uint8_t)*s)];
    return w;
}

/* one-shot: text in a box, cleared to black, blitted */
static void draw_text_box(uint32_t x, uint32_t y, uint32_t w, const felucca_font_t *f, const char *s,
                          uint16_t c, int align)
{
    int32_t tw = text_w(f, s), tx = 0;
    cv_begin(w, f->h, C_BLACK);
    if (align == 1)
        tx = ((int32_t)w - tw) / 2;
    else if (align == 2)
        tx = (int32_t)w - tw;
    cv_text(tx, 0, f, s, c);
    cv_blit(x, y);
    lcd_sync();                     /* one-shots (boot, crash, UBOOT, update) finish here */
}
