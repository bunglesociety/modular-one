/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society); after the Felucca UI by Leo Kuroshita */
/* Modular One: the screen.
 *
 *   rows   0..19   the bar: track, page (and its siblings), transport, tempo, battery
 *   rows  20..189  the page: one picture per page, in two bands of 85 rows (the canvas holds
 *                  124 rows), each redrawn only when what it shows changed
 *   rows 190..239  the four knobs, always the same four colours (KC[], the COLOR theme): a dot,
 *                  the name, the value, a gauge. The picture above colours what each knob moves
 *                  with that knob's colour.
 *
 * Before the first frame: the 5 s opening (MODULAR ONE / BY BUNGLE SOCIETY, a sun rising over the
 * water). After a minute untouched: the window on Modular Town (mt_city.c) as a screen saver,
 * until the next touch (menu VEILLE). All in the main loop, never in the audio interrupt. */
static void draw_menu(void);
static uint32_t str_hash(uint32_t h, const char *s);

/* ---------------------------------------------------------------- colours --- */
#define M_BG UIC[0]                              /* the theme (gfx.c THEMES, menu THEME) */
#define M_PANEL UIC[1]
#define M_LINE UIC[2]
#define M_DIM UIC[3]
#define M_GREY UIC[4]
#define M_TEXT UIC[5]
#define M_REC UIC[5]
static const uint16_t FAMC[5] = {GB3, GB2, GB3, GB2, GB3};   /* the families: SOUFFLES, KEYS, LUMIERES, OMBRES, NEON */

#define Y_MAIN 20
#define H_BAND 85
#define Y_KNOB 190
#define SAVER_MS 60000u
#define SAVER_OFF (settings.zoom != 0)            /* menu SCREEN SAVER (the old ZOOM setting): 0 = on */
#define SPLASH_MS 5000u

static struct {
    uint32_t top, knob[4], band[2];
    uint32_t touch_ms, splash_t0, last_sig;
    uint8_t saver, splash, started, chimed, theme;
    uint8_t meter[NPART];
} m1;

/* ------------------------------------------------------------- primitives --- */
static inline uint16_t m1_mix(uint32_t a, uint32_t b, uint32_t t)          /* a + (b - a) t / 16 */
{
    int32_t ar = (int32_t)(a >> 11), ag = (int32_t)((a >> 5) & 63u), ab = (int32_t)(a & 31u);
    int32_t br = (int32_t)(b >> 11), bg = (int32_t)((b >> 5) & 63u), bb = (int32_t)(b & 31u);
    if (t > 16u)
        t = 16u;
    return (uint16_t)(((ar + (((br - ar) * (int32_t)t) >> 4)) << 11) | ((ag + (((bg - ag) * (int32_t)t) >> 4)) << 5) |
                      (ab + (((bb - ab) * (int32_t)t) >> 4)));
}

static inline void m1_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* a 0..16 */
{
    y += cv_oy;
    if ((uint32_t)x < cv_w && (uint32_t)y < cv_h) {
        uint16_t *q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
        *q = swap16(m1_mix(swap16(*q), c, a));
    }
}

/* a line; in the drawn themes, by hand: it wanders a pixel off its path and back, the same way at
 * every redraw (the wander comes from its end points) */
static void m1_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, guard = 2000, n = 0, j = 0, horiz = dx >= -dy;
    uint32_t seed = (uint32_t)(x0 * 7919 + y0 * 104729 + x1 * 31 + y1 * 131);
    if (!ui_drawn || (dx < 4 && -dy < 4)) {
        cv_line(x0, y0, x1, y1, c);
        return;
    }
    while (guard--) {
        int32_t e2 = 2 * err;
        if (!(n & 7)) {                                     /* every 8 px: the hand drifts by one, or back */
            uint32_t h = (seed + (uint32_t)n * 2654435761u) >> 29;
            j += h < 2u && j > -1 ? -1 : h > 5u && j < 1 ? 1 : 0;
        }
        n++;
        if (horiz)
            cv_pset(x0, y0 + j, c);
        else
            cv_pset(x0 + j, y0, c);
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

/* text, alpha-blended onto whatever is under it; returns the end x */
static int32_t m1_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        if (y + (int32_t)f->h + cv_oy > 0 && y + cv_oy < (int32_t)cv_h)
            for (gy = 0; gy < f->h; gy++)
                for (gx = 0; gx < w; gx++) {
                    uint32_t v = gd[gy * bpr + gx / 2u];
                    v = (gx & 1u) ? (v & 15u) : (v >> 4);
                    if (v) {
                        m1_blend(x - f->pad + (int32_t)gx, y + (int32_t)gy, c, v + (v >> 3));
                        if (m1_bold && f == &FONT_LAB)              /* bold: the stroke two pixels wide */
                            m1_blend(x - f->pad + (int32_t)gx + 1, y + (int32_t)gy, c, v + (v >> 3));
                    }
                }
        x += f->adv[gi];
    }
    return x;
}
static void m1_text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    m1_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void m1_text_r(int32_t rx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    m1_text(rx - text_w(f, s), y, f, s, c);
}

static int32_t isqrt(int32_t v)
{
    int32_t r = 0, b = 1 << 14;
    if (v <= 0)
        return 0;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

static void m1_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t w = isqrt(r * r - dy * dy + r);
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}

static void m1_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x = r, y = 0, e = 1 - r;
    if (ui_drawn && r > 4) {                                /* by hand: a little out of round, ends overlapping */
        int32_t k, px = 0, py = 0;
        for (k = 0; k <= 36; k++) {
            uint32_t a = (uint32_t)k * (0xFFFFFFFFu / 34u);   /* a bit more than one turn */
            int32_t rr = r + ((k * 5 + r) % 7 == 0 ? 1 : 0) - (k > 33 ? 1 : 0);
            int32_t qx = cx + sine_i(a + 0x40000000u) * rr / 32768, qy = cy - sine_i(a) * rr / 32768;
            if (k)
                cv_line(px, py, qx, qy, c);
            px = qx;
            py = qy;
        }
        return;
    }
    while (x >= y) {
        cv_pset(cx + x, cy + y, c), cv_pset(cx - x, cy + y, c), cv_pset(cx + x, cy - y, c), cv_pset(cx - x, cy - y, c);
        cv_pset(cx + y, cy + x, c), cv_pset(cx - y, cy + x, c), cv_pset(cx + y, cy - x, c), cv_pset(cx - y, cy - x, c);
        y++;
        if (e < 0) {
            e += 2 * y + 1;
        } else {
            x--;
            e += 2 * (y - x) + 1;
        }
    }
}

/* rounded rectangles, radius up to 4 */
static void m1_rrect_o(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c);
static const uint8_t RR[5][5] = {{0}, {1, 0}, {1, 0, 0}, {2, 1, 0, 0}, {2, 1, 0, 0, 0}};
static void m1_rrect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    int32_t j;
    if (ui_style == 3u)                                     /* 8 BIT: square corners */
        r = 0;
    if (r > 4)
        r = 4;
    for (j = 0; j < h; j++) {
        int32_t k = j < r ? RR[r][j] : j >= h - r ? RR[r][h - 1 - j] : 0;
        cv_rect(x + k, y + j, w - 2 * k, 1, c);
    }
    if (ui_drawn && w > 16 && h > 13 && c != M_PANEL && c != M_BG && c != M_LINE)
        m1_rrect_o(x, y, w, h, r, m1_mix(c, ui_light ? M_TEXT : M_BG, ui_light ? 8u : 6u));
}
static void m1_rrect_o(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    int32_t j;
    if (ui_drawn && w > 16 && h > 13) {                     /* by hand: four strokes running past the corners */
        m1_line(x - 1, y, x + w, y + 1, c);
        m1_line(x + w - 1, y - 1, x + w - 2, y + h, c);
        m1_line(x + w, y + h - 1, x - 1, y + h - 2, c);
        m1_line(x, y + h, x + 1, y - 1, c);
        return;
    }
    if (ui_style == 3u)
        r = 0;
    if (r > 4)
        r = 4;
    cv_rect(x + RR[r][0], y, w - 2 * RR[r][0], 1, c);
    cv_rect(x + RR[r][0], y + h - 1, w - 2 * RR[r][0], 1, c);
    for (j = 1; j < h - 1; j++) {
        int32_t k = j < r ? RR[r][j] : j >= h - r ? RR[r][h - 1 - j] : 0;
        cv_pset(x + k, y + j, c);
        cv_pset(x + w - 1 - k, y + j, c);
    }
}

static void m1_thick(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t t, uint16_t c)
{
    int32_t k, dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    for (k = -(t / 2); k < t - t / 2; k++) {
        if (dx > dy)
            m1_line(x0, y0 + k, x1, y1 + k, c);
        else
            m1_line(x0 + k, y0, x1 + k, y1, c);
    }
}

/* a curve with a soft fill down (or up) to its axis: y[] holds 1 value per column from x0 */
static void m1_curve(int32_t x0, int32_t n, const int16_t *y, int32_t axis, int32_t amp, uint16_t c)
{
    int32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = y[i] < axis ? y[i] : axis, b = y[i] < axis ? axis : y[i], yy;
        if (ui_drawn) {                                     /* by hand: hatching, one stroke in three */
            if (!((x0 + i) % 3))
                for (yy = a; yy <= b; yy++)
                    m1_blend(x0 + i, yy, c, 6u);
        } else {
            for (yy = a; yy <= b; yy++) {
                int32_t d = yy - axis;
                if (d < 0)
                    d = -d;
                m1_blend(x0 + i, yy, c, (uint32_t)(2 + d * 6 / (amp > 0 ? amp : 1)));
            }
        }
        if (i)
            m1_line(x0 + i - 1, y[i - 1], x0 + i, y[i], c);
    }
}

/* a canvas whose top left is screen (sx, sy), cleared to the ground; on paper, its grain: a few
 * darker fibres and specks, fixed to the screen position so redraws never move them */
static void m1_begin(uint32_t w, uint32_t h, int32_t sx, int32_t sy)
{
    uint32_t x, y;
    uint16_t f1, f2;
    cv_begin(w, h, M_BG);
    if (ui_style == 3u || (!ui_light && ui_style == 0u))
        return;
    if (ui_light) {                                         /* paper: fibres and specks */
        f1 = swap16(m1_mix(M_BG, M_PANEL, 7u));
        f2 = swap16(m1_mix(M_BG, M_LINE, 9u));
    } else if (ui_style == 1u) {                            /* FILM: a fine grain, light and dark */
        f1 = swap16(m1_mix(M_BG, M_PANEL, 10u));
        f2 = swap16(m1_mix(M_BG, M_DIM, 6u));
    } else {                                                /* PIRATE: a few stars in the night */
        f1 = swap16(M_BG);
        f2 = swap16(m1_mix(M_BG, M_GREY, 7u));
    }
    for (y = 0; y < cv_h; y++)
        for (x = 0; x < w; x++) {
            uint32_t hh = ((uint32_t)(sx + (int32_t)x) * 73856093u) ^ ((uint32_t)(sy + (int32_t)y) * 19349663u);
            hh ^= hh >> 13;
            hh *= 0x5bd1e995u;
            hh >>= 24;
            if (hh < (ui_light ? 22u : 12u))
                cv_px[y * w + x] = f1;
            else if (hh > (ui_style == 2u ? 254u : 252u) && (ui_style != 2u || ((sx + (int32_t)x) ^ (sy + (int32_t)y)) & 1))
                cv_px[y * w + x] = f2;
        }
}

static void m1_gauge_h(int32_t x, int32_t y, int32_t w, int32_t ratio, int bip, uint16_t c)
{
    int32_t fx = ratio * w / 1000;
    if (ui_style == 3u && !bip) {                           /* 8 BIT: a row of blocks */
        int32_t k;
        for (k = 0; k + 4 <= w; k += 5)
            cv_rect(x + k, y - 1, 4, 3, k < fx ? c : M_PANEL);
        return;
    }
    if (ui_drawn) {
        m1_line(x, y, x + w, y, M_LINE);
        if (bip)
            m1_line(fx < w / 2 ? x + fx : x + w / 2, y, fx < w / 2 ? x + w / 2 : x + fx, y, c);
        else
            m1_line(x, y, x + fx, y, c);
        m1_line(x + fx - 1, y - 3, x + fx, y + 3, c);
        return;
    }
    cv_rect(x, y, w, 1, M_LINE);
    if (bip)
        cv_rect(fx < w / 2 ? x + fx : x + w / 2, y, fx < w / 2 ? w / 2 - fx : fx - w / 2, 1, c);
    else
        cv_rect(x, y, fx, 1, c);
    cv_rect(x + fx - 1, y - 2, 2, 5, c);
}

/* -------------------------------------------------------------- the bar --- */
static int32_t batt_level(void)                           /* ADC ch3 thresholds */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
static int32_t batt_shown(void)                           /* charging (USB host): the bars fill in turn */
{
    if (usb.config && !usb.suspended)
        return 1 + (int32_t)((fm1_ms / 600u) % 3u);
    return batt_level();
}

/* one screen, the handheld green (gfx.c); the bold letters (menu): settings.palette 0x100 set = chosen,
 * 0x200 = bold (older values: bold); the legacy MONO steps */
static void m1_theme_apply(int force)
{
    uint8_t bold = (uint8_t)(settings.palette & 0x100u ? (settings.palette & 0x200u) != 0 : 1);
    if (bold != m1_bold) {
        m1_bold = bold;
        force = 1;
    }
    if (force || !m1.theme) {
        m1.theme = 1;
        palette_set(0);
        ui.force = 1;
    }
}

static uint16_t track_col(uint32_t k) { return KC[k == 2u ? 3u : k]; }   /* T1 blue, T2 green, T3 orange */

/* the page: its name and its rank in the family */
static void page_name(char *ti, uint32_t *k, uint32_t *n)
{
    const page_t *pg = cur_page();
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t i;
    *k = *n = 0;
    if (ui.home) {
        str_cpy(ti, "HOME", 12);
        return;
    }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam) {
            if (i == ui.page)
                *k = *n;
            (*n)++;
        }
    str_cpy(ti, pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : pg->title, 12);
}

static void draw_top(void)
{
    char b[24], ti[12];
    uint32_t pk, pn, i, rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u;
    uint32_t sig;
    page_name(ti, &pk, &pn);
    sig = str_hash(0x811C9DC5u, ti) + pk * 7u + pn * 77u + (uint32_t)song.playing * 3u + rec * 5u +
          (uint32_t)(song.octave + 8) * 11u + song.sel * 13131u + (ui.msg_t ? str_hash(7u, ui.msg) : 1u) +
          (uint32_t)song.g[G_BPM] * 101u + (ui.bpm_t != 0) * 31u + (uint32_t)batt_shown() * 7777u + KC[0];
    if (!ui.force && sig == m1.top)
        return;
    m1.top = sig;
    m1_begin(240, 20, 0, 0);
    m1_rrect(4, 4, 14, 12, 3, track_col(song.sel));
    b[0] = (char)('1' + song.sel);
    b[1] = 0;
    m1_text_c(11, 6, FN_LAB, b, M_BG);
    if (ui.msg_t) {
        m1_text(24, 2, FN_SM, ui.msg, M_TEXT);
    } else {
        int32_t x = m1_text(24, 6, FN_LAB, ti, M_TEXT) + 6;
        for (i = 0; pn > 1u && i < pn; i++)                   /* the pages of this button */
            cv_rect(x + (int32_t)i * 5, 9, 3, 3, i == pk ? M_TEXT : M_LINE);
    }
    {
        int32_t x = 166;
        if (song.octave) {                                    /* the keyboard octave */
            str_cpy(b, song.octave > 0 ? "+" : "", 4);
            fmt_int(b + str_len(b), song.octave);
            m1_text_r(x - 6, 4, FN_VALS, b, KC[2]);
        }
        if (rec) {
            m1_disc(x + 2, 9, 3, rec == 2u ? M_REC : M_DIM);
            x += 10;
        }
        if (song.playing) {
            for (i = 0; i < 5u; i++)
                cv_rect(x + (int32_t)i, 5 + (int32_t)i, 1, 9 - 2 * (int32_t)i, M_TEXT);
        } else {
            cv_rect(x, 6, 6, 7, M_DIM);
        }
        fmt_int(b, song.g[G_BPM]);
        m1_text_r(215, 3, FN_VALS, b, ui.bpm_t ? M_TEXT : M_GREY);
    }
    {
        int32_t lvl = batt_shown(), k;
        m1_rrect_o(219, 5, 15, 9, 1, M_DIM);
        cv_rect(234, 8, 2, 3, M_DIM);
        for (k = 0; k < lvl; k++)
            cv_rect(221 + k * 4, 7, 3, 5, lvl == 1 && batt_level() <= 1 ? M_REC : M_GREY);
    }
    m1_line(0, 19, 239, 19, M_LINE);
    cv_blit(0, 0);
}

/* ------------------------------------------------------------ the knobs --- */
typedef struct {
    char lab[8], val[10], unit[6];
    int32_t ratio;                 /* 0..1000, -1: no gauge */
    uint8_t bip, dim;
} knob_t;

static void knob_set(knob_t *k, const char *lab, const char *val, const char *unit, int32_t ratio)
{
    str_cpy(k->lab, lab, sizeof k->lab);
    str_cpy(k->val, val, sizeof k->val);
    str_cpy(k->unit, unit, sizeof k->unit);
    k->ratio = ratio;
}

static void knob_param(knob_t *k, const param_desc_t *d, int16_t v)
{
    char val[12];
    const char *unit = "";
    param_format(d, v, val, &unit);
    knob_set(k, d->label, val, unit, (d->fmt == F_ENUM || d->fmt == F_ONOFF) && d->max < 2 ? -1 : RATIO(d, v));
    k->bip = d->fmt == F_BIPCT;
}

static void knob_info(uint32_t c, knob_t *k)
{
    const page_t *pg = cur_page();
    int16_t *vp = 0;
    const param_desc_t *d;
    char v[12];
    k->lab[0] = k->val[0] = k->unit[0] = 0;
    k->ratio = -1;
    k->bip = k->dim = 0;
    if (ui.home) {
        d = home_param(c, &vp);
        if (d && vp)
            knob_param(k, d, *vp);
        return;
    }
    if (pg->scope == SC_TRK) {                                /* TRACK LEVEL LEN PAN of the selected track */
        const track_t *t = TSEL;
        if (c == 0u) {
            fmt_int(v, (int32_t)song.sel + 1);
            knob_set(k, "TRACK", v, "/3", (int32_t)song.sel * 500);
        } else if (c == 1u) {
            if (!t->p[P_LEVEL] || t->p[P_MUTE]) {
                knob_set(k, "LEVEL", "MUTE", "", 0);
                k->dim = 1;
            } else {
                knob_param(k, &TP[P_LEVEL], t->p[P_LEVEL]);
            }
        } else if (c == 2u) {
            knob_param(k, &TP[P_SLEN], t->p[P_SLEN]);
        } else {
            knob_param(k, &TP[P_PAN], t->p[P_PAN]);
        }
        return;
    }
    if (pg->graph == GR_BROWSE) {
        uint32_t total, cur = preset_pos(&total);
        if (c == 0u) {
            fmt_int(v, (int32_t)cur + 1);
            str_cpy(k->unit, "", 2);
            knob_set(k, "SOUND", v, "", total > 1u ? (int32_t)(cur * 1000u / (total - 1u)) : 0);
        } else if (c == 1u && !TSEL->user) {
            knob_set(k, "FAMILY", MT_FAM3[(TSEL->preset / 8u) % MT_NFAM], "", (int32_t)((TSEL->preset / 8u) * 1000u / (MT_NFAM - 1u)));
        }
        return;
    }
    if (pg->graph == GR_USER) {
        int used = up_used(ui.uslot);
        up_slot_label(v, ui.uslot);
        if (c == 0u)
            knob_set(k, "SLOT", v, "", (int32_t)ui.uslot * 1000 / (int32_t)(UP_SLOTS - 1u));
        else
            knob_set(k, c == 1u ? "LOAD" : c == 2u ? "ERASE" : "SAVE", ui.arm == 0xE0u + c ? "AGAIN" : "--", "", -1);
        k->dim = (c == 1u || c == 2u) && !used;
        return;
    }
    if (pg->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        static const char *const FLAG_N[4] = {"-", "ACC", "SLD", "A+S"};
        const step_t *st = &TSEL->step[ui.cursor];
        char u[6];
        if (c == 0u) {
            fmt_int(v, (int32_t)ui.cursor + 1);
            str_cpy(u, "/", 6);
            fmt_int(u + 1, TSEL->p[P_SLEN]);
            knob_set(k, "STEP", v, u, (int32_t)ui.cursor * 1000 / (TSEL->p[P_SLEN] > 1 ? TSEL->p[P_SLEN] - 1 : 1));
        } else if (c == 1u) {
            if (st->n) {
                note_name(v, st->note[0]);
                u[0] = 0;
                if (st->n > 1) {
                    str_cpy(u, "+", 6);
                    fmt_int(u + 1, st->n - 1);
                }
            } else {
                str_cpy(v, "--", 12);
                u[0] = 0;
            }
            knob_set(k, "NOTE", v, u, -1);
            k->dim = !step_on(st);
        } else if (c == 2u) {
            knob_set(k, "TIME", TIME_N[st->time % 3u], "", -1);
        } else {
            knob_set(k, "FLAG", FLAG_N[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], "", -1);
        }
        return;
    }
    d = page_desc(pg, c, &vp);
    if (!d || !vp || !d->label || d->label[0] == '-')
        return;
    if (pg->id[c] == G_MIDI && pg->scope == SC_GLOBAL) {
        knob_set(k, "USB", !usb.up ? "OFF" : usb.config ? "MIDI" : usb.setups ? "ENUM" : usb.sof_seen ? "BUS" : "WAIT", "", -1);
        return;
    }
    if (pg->id[c] == G_INFO && pg->scope == SC_GLOBAL) {
        fmt_int(v, (int32_t)(song.cpu_q8 * 100u / 256u));
        knob_set(k, "CPU", v, "%", (int32_t)(song.cpu_q8 * 1000u / 256u));
        return;
    }
    knob_param(k, d, *vp);
}

static void draw_knobs(void)
{
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        knob_t k;
        int hot = ui.hot_t && ui.hot_col == c;
        uint16_t col = KC[c], vc;
        uint32_t sig;
        int32_t x;
        knob_info(c, &k);
        sig = str_hash(str_hash(str_hash(0x9E3779B9u + c, k.lab), k.val), k.unit) + (uint32_t)(k.ratio + 1) * 31u +
              (uint32_t)hot * 7u + k.dim * 13u + k.bip * 17u + col;
        if (!ui.force && sig == m1.knob[c])
            continue;
        m1.knob[c] = sig;
        m1_begin(60, 50, (int32_t)c * 60, Y_KNOB);
        if (ui_style == 2u)                                     /* PIRATE: the verb panel */
            cv_rect(0, 0, 60, 50, M_PANEL);
        if (ui_style == 3u) {                                   /* 8 BIT: a framed box per knob */
            m1_rrect_o(2, 3, 56, 45, 0, M_DIM);
            m1_rrect_o(3, 4, 54, 43, 0, M_LINE);
        }
        m1_line(0, 0, 59, 0, M_LINE);
        if (c)
            m1_line(0, 7, 0, 43, M_LINE);
        if (k.lab[0]) {
            vc = k.dim ? M_DIM : hot ? M_TEXT : col;
            m1_disc(8, 9, 2, k.dim ? M_DIM : col);
            m1_text(14, 5, FN_LAB, k.lab, hot ? M_TEXT : M_GREY);
            if (text_w(FN_VAL, k.val) > 48) {                   /* too long for the big letters */
                m1_text(7, 22, FN_LAB, k.val, vc);
            } else {
                x = m1_text(6, 17, FN_VAL, k.val, vc);
                if (k.unit[0] && x + 1 + text_w(FN_VALS, k.unit) <= 54)
                    m1_text(x + 1, 19, FN_VALS, k.unit, M_DIM);
            }
            if (k.ratio >= 0)
                m1_gauge_h(6, 41, 48, k.ratio, k.bip, k.dim ? M_DIM : col);
        } else {
            m1_disc(8, 9, 2, M_LINE);
        }
        cv_blit(c * 60u, Y_KNOB);
    }
}

/* -------------------------------------------------------------- the pages --- */
static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &t->step[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) * 16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static int32_t pv(uint32_t id) { return TSEL->p[id]; }
static int32_t pr(uint32_t id, int32_t lo, int32_t hi) { return (TSEL->p[id] - lo) * 1000 / (hi - lo); }   /* 0..1000 */

/* HOME: the sound, its family, and what comes out */
static void pg_home(void)
{
    const track_t *t = TSEL;
    char nm[16], sub[40];
    uint32_t fam = (t->preset / 8u) % MT_NFAM;
    int user = user_of(t) < UP_SLOTS;
    static const char *const VN[4] = {"poly", "mono", "legato", "unison"};
    if (user) {
        up_name(user_of(t), nm);
        m1_rrect(6, 26, 31, 12, 3, M_GREY);
        m1_text_c(22, 28, FN_LAB, "USR", M_BG);
    } else {
        char k[8];
        str_cpy(nm, ENGINES[t->eng_req]->presets[t->preset % ENGINES[t->eng_req]->npresets].name, sizeof nm);
        m1_rrect(6, 26, 31, 12, 3, FAMC[fam]);
        m1_text_c(22, 28, FN_LAB, MT_FAM3[fam], M_BG);
        fmt_int(k, (int32_t)(t->preset % 8u) + 1);
        str_cpy(k + str_len(k), " / 8", 6);
        m1_text(43, 28, FN_LAB, k, M_GREY);
    }
    if (ui_style == 1u)                                     /* FILM: a title card */
        m1_text_c(120, 46, text_w(FN_NAME, nm) <= 228 ? FN_NAME : FN_MID, nm, M_TEXT);
    else
        m1_text(6, 44, text_w(FN_NAME, nm) <= 228 ? FN_NAME : FN_MID, nm, M_TEXT);
    str_cpy(sub, user ? "user" : MT_FAM[fam], sizeof sub);
    {   /* "Cloches  /  track 1  /  poly" */
        uint32_t i;
        for (i = 1; sub[i]; i++)
            if (sub[i] >= 'A' && sub[i] <= 'Z')
                sub[i] = (char)(sub[i] + 32);
    }
    str_cpy(sub + str_len(sub), "  /  track ", sizeof sub - str_len(sub));
    sub[str_len(sub) + 1u] = 0;
    sub[str_len(sub)] = (char)('1' + song.sel);
    str_cpy(sub + str_len(sub), "  /  ", sizeof sub - str_len(sub));
    str_cpy(sub + str_len(sub), VN[t->p[P_VOICE] & 3], sizeof sub - str_len(sub));
    if (ui_style == 1u)
        m1_text_c(120, 80, FN_SM, sub, M_GREY);
    else
        m1_text(7, 78, FN_SM, sub, M_GREY);
    /* band 2: the output, triggered on a rising zero crossing, auto-scaled */
    {
        static int16_t snap[SCOPE_N];
        int16_t y[240];
        uint32_t w = scope_w, i, trig = 0, nv = 0;
        int32_t peak = 1200, x;
        for (i = 0; i < SCOPE_N; i++) {
            snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
            if (snap[i] > peak)
                peak = snap[i];
            else if (-snap[i] > peak)
                peak = -snap[i];
        }
        for (i = 1; i < SCOPE_N - 240u; i++)
            if (snap[i - 1] < 0 && snap[i] >= 0) {
                trig = i;
                break;
            }
        for (x = 0; x < 240; x++)
            y[x] = (int16_t)(146 - snap[trig + (uint32_t)x] * 34 / peak);
        cv_rect(0, 146, 240, 1, M_LINE);
        m1_curve(0, 240, y, 146, 34, KC[0]);
        for (i = 0; i < NVOICE; i++)                            /* the voices sounding */
            nv += t->v[i].active && t->v[i].stage != 4u;
        for (i = 0; i < NVOICE; i++)
            cv_rect(186 + (int32_t)i * 6, 176, 4, 3, i < nv ? KC[0] : M_LINE);
    }
}

/* the FM page: one cycle of the voice, from the same formula as the engine (eng_mtfm.c) */
static void fm_cycle(int16_t *y, int32_t n, int32_t axis, int32_t amp)
{
    const int16_t *p = TSEL->p;
    int32_t cut = p[P_E0] * 258, res = p[P_E1] * 258, i, k;
    uint32_t tim = (uint32_t)p[P_E2] % MT_NTIMB;
    int32_t idx = mt_lerp(MT_IDX_Q12, cut, 128), idx2 = mulq15(idx, 14746), fb = (res * 14418) >> 15, fbk = 0;
    uint32_t inc = 0xFFFFFFFFu / (uint32_t)n, inc2 = (uint32_t)(((uint64_t)inc * MT_DET_Q16[clamp(p[P_E3] * 3 + 192, 0, 384)]) >> 16);
    uint32_t im1 = fm_ratio_inc(inc, MT_R1_Q8[tim]), im2 = fm_ratio_inc(inc2, MT_R2_Q8[tim]), ph = 0, p1 = 0, p2 = 0;
    for (k = 0; k < 2; k++)                                     /* twice: the feedback settles */
        for (i = 0; i < n; i++) {
            int32_t m1v = sine_i(p1 + ((uint32_t)(fb * fbk) << 1)), m2v, car;
            fbk = (fbk + m1v) >> 1;
            m2v = sine_i(p2);
            car = sine_i(ph + ((uint32_t)(idx * m1v) << 5) + ((uint32_t)(idx2 * m2v) << 5));
            if (k)
                y[i] = (int16_t)(axis - car * amp / 32768);
            ph += inc;
            p1 += im1;
            p2 += im2;
        }
}

static void op_circle(int32_t x, int32_t y, const char *name, const char *ratio, uint16_t rc, int carrier)
{
    m1_disc(x, y, 15, M_PANEL);
    m1_ring(x, y, 15, carrier ? M_TEXT : M_GREY);
    m1_text_c(x, y - 9, FN_LAB, name, M_GREY);
    m1_text_c(x, y - 1, FN_VALS, ratio, rc);
}

/* the console voice (TIMB PUL50 .. NOISE): its wave, the second pulse, the low-pass and the breath */
static void pg_fm(void)
{
    static const char *const R1[12] = {"1", "1", "1", "2", "2", "3", "1", ".5", "1", "2", "1", "1"};
    static const char *const R2[12] = {"1", "2", "3", "1", "3", "1", ".5", "1", "3.5", "5", "5", "7"};
    uint32_t tim = (uint32_t)pv(P_E2) % MT_NTIMB;
    int32_t bril = pr(P_E0, 0, 127), grain = pr(P_E1, 0, 127), det = pv(P_E3) * 12 / 64;
    int32_t wt = 1 + bril * 3 / 1000, k;
    char b[12];
    int16_t y[80];
    /* M2 -> C <- M1, and M1's feedback loop */
    m1_thick(48, 62, 116, 124, wt, m1_mix(M_BG, KC[0], 6u + (uint32_t)bril / 100u));
    m1_thick(124, 62, 124, 124, wt, m1_mix(M_BG, KC[0], 6u + (uint32_t)bril / 100u));
    for (k = 0; k <= 200; k++) {                                /* the loop: 3/4 of a turn above M1 */
        uint32_t a = (uint32_t)k * 0x00F00000u;
        int32_t cx = 140, cy = 46, px = cx + sine_i(0x40000000u + a) * 12 / 32768, py = cy - sine_i(a) * 12 / 32768;
        if (grain > 0 || (k & 3) == 0)
            cv_pset(px, py, grain ? KC[1] : M_LINE);
        if (grain > 500)
            cv_pset(px + 1, py, KC[1]);
    }
    op_circle(48, 62, "M2", R2[tim], KC[2], 0);
    op_circle(124, 62, "M1", R1[tim], KC[2], 0);
    op_circle(124, 128, "C", "1", M_TEXT, 1);
    cv_rect(124, 144, 1, 6, M_TEXT);
    b[0] = 0;
    if (det > 0)
        str_cpy(b, "+", 4);
    fmt_int(b + str_len(b), det);
    str_cpy(b + str_len(b), " st", 6);
    m1_text_c(48, 81, FN_VALS, b, pv(P_E3) ? KC[3] : M_DIM);
    /* the four readouts */
    {
        int32_t ly = FN_LAB->h + 1, y0 = (int32_t)(Y_MAIN + 2 * H_BAND) - 8 - 3 * ly;              /* three rows, above the letterbox */
        m1_text(6, y0, FN_LAB, "INDEX", M_GREY);
        m1_gauge_h(58, y0 + 4, 80, bril, 0, KC[0]);
        m1_text(6, y0 + ly, FN_LAB, "FEEDBACK", M_GREY);
        m1_gauge_h(58, y0 + ly + 4, 80, grain, 0, KC[1]);
        m1_text(6, y0 + 2 * ly, FN_LAB, "RATIO", M_GREY);
        str_cpy(b, R1[tim], 6);
        str_cpy(b + str_len(b), ":", 2);
        str_cpy(b + str_len(b), R2[tim], 6);
        m1_text(58, y0 + 2 * ly + FN_LAB->h - FN_VALS->h, FN_VALS, b, KC[2]);
    }
    /* one cycle */
    m1_rrect_o(152, 108, 84, 78, 3, M_LINE);
    m1_text(157, 112, FN_LAB, "1 CYCLE", M_DIM);
    fm_cycle(y, 80, 152, 26);
    m1_curve(154, 80, y, 152, 26, KC[0]);
}

/* ESPACE: the rise, the night, the island, the sub */
static void pg_espace(void)
{
    int32_t mont = pr(P_E4, 0, 127), night = pr(P_E5, 0, 127), ile = pv(P_E6), sub = pr(P_E7, 0, 127), k;
    for (k = 0; k < 6; k++) {                                   /* MONT: steps climbing */
        int32_t h = 10 + k * 11;
        cv_rect(10 + k * 7, 150 - h, 4, h, (k + 1) * 1000 / 6 <= mont + 80 && mont ? KC[0] : M_LINE);
    }
    {   /* NUIT: the moon fills with the night */
        int32_t cx = 90, cy = 96, off = 4 + night * 40 / 1000;
        m1_disc(cx, cy, 22, night ? KC[1] : M_LINE);
        m1_disc(cx - off, cy - 4, 22, M_BG);
        if (night > 900)
            m1_disc(cx, cy, 22, KC[1]);
    }
    {   /* ILE: the island, out of the haze when ON */
        uint16_t c = ile ? KC[2] : M_LINE;
        int32_t cx = 150, j;
        for (j = 0; j < 7; j++) {
            int32_t w = 24 - (j * j) / 2;
            cv_rect(cx - w, 140 - j, 2 * w, 1, c);
        }
        m1_thick(cx + 6, 134, cx + 9, 112, 2, c);
        m1_thick(cx + 9, 112, cx + 5, 92, 2, c);
        for (j = -2; j <= 2; j++)
            if (j)
                m1_thick(cx + 5, 92, cx + 5 + j * 8, 92 + (j < 0 ? -j : j) * 5, 2, c);
        cv_rect(122, 146, 56, 1, ile ? m1_mix(M_BG, KC[2], 6u) : M_LINE);
    }
    {   /* SUB: the note above, the octave below */
        int16_t y[50];
        for (k = 0; k < 50; k++)
            y[k] = (int16_t)(66 - sine_i((uint32_t)k * (0xFFFFFFFFu / 25u)) * 10 / 32768);
        for (k = 1; k < 50; k++)
            m1_line(186 + k - 1, y[k - 1], 186 + k, y[k], M_DIM);
        for (k = 0; k < 50; k++)
            y[k] = (int16_t)(118 - sine_i((uint32_t)k * (0xFFFFFFFFu / 50u)) * (2 + sub * 26 / 1000) / 32768);
        m1_curve(186, 50, y, 118, 28, sub ? KC[3] : M_LINE);
    }
}

/* ENV: attack, decay, sustain, release, each in its knob's colour */
static void pg_env(void)
{
    const int32_t top = 34, bot = 172;
    int32_t a = 4 + pv(P_ATK) * 44 / 127, d = 8 + pv(P_DEC) * 56 / 127, r = 8 + pv(P_REL) * 64 / 127;
    int32_t sus = pv(P_SUS) * 1000 / 127, x0 = 8, x1 = x0 + a, x2 = x1 + d, x3 = 226 - r, i;
    int16_t y[240];
    int32_t e;
#define EY(l) ((int16_t)(bot - (l) * (bot - top) / 1000))
    if (x3 < x2 + 16)
        x3 = x2 + 16;
    for (i = 0; i <= a; i++)
        y[i] = EY(i * 1000 / (a ? a : 1));
    e = 32768;
    for (i = 1; i <= d; i++) {
        e = (e * (32768 - 150733 / d)) >> 15;
        y[a + i] = EY(sus + ((1000 - sus) * e >> 15));
    }
    for (i = x2; i <= x3; i++)
        y[i - x0] = EY(sus);
    e = 32768;
    for (i = 1; i <= r && x3 + i < 236; i++) {
        e = (e * (32768 - 150733 / r)) >> 15;
        y[x3 - x0 + i] = EY(sus * e >> 15);
    }
    m1_curve(x0, a + 1, y, bot, bot - top, KC[0]);
    m1_curve(x1, d + 1, y + a, bot, bot - top, KC[1]);
    m1_curve(x2, x3 - x2 + 1, y + (x2 - x0), bot, bot - top, KC[2]);
    m1_curve(x3, r + 1 < 236 - x3 ? r + 1 : 236 - x3, y + (x3 - x0), bot, bot - top, KC[3]);
    cv_rect(4, bot + 1, 232, 1, M_LINE);
    m1_text(x0, 24, FN_LAB, "A", M_DIM);
    m1_text(x1 + 2, 24, FN_LAB, "D", M_DIM);
    m1_text(x2 + 2, 24, FN_LAB, "S", M_DIM);
    m1_text(x3 + 2, 24, FN_LAB, "R", M_DIM);
    cv_rect(x1, top - 2, 1, bot - top + 2, M_LINE);
    cv_rect(x2, top - 2, 1, bot - top + 2, M_LINE);
    cv_rect(x3, top - 2, 1, bot - top + 2, M_LINE);
#undef EY
}

/* LFO: RATE how many cycles, WAVE the shape, PHASE where it starts, FADE its slow arrival */
static void pg_lfo(void)
{
    const track_t *t = TSEL;
    int16_t y[232];
    int32_t x, cyc = 1 + pv(P_LRATE) * 5 / 127, fade = pv(P_LFADE) * 180 / 127;
    uint32_t ph = (uint32_t)pv(P_LPHASE) << 25;
    for (x = 0; x < 232; x++) {
        int32_t v = lfo_wave((track_t *)t, ph + (uint32_t)x * (uint32_t)cyc * (0xFFFFFFFFu / 232u));
        if (pv(P_LWAVE) == 4)
            v = (int32_t)((((uint32_t)(x * cyc / 29) + 1u) * 2654435761u) >> 16) - 32768;
        if (fade && x < fade)
            v = v * x / fade;
        y[x] = (int16_t)(104 - v * 54 / 32768);
    }
    cv_rect(4, 104, 232, 1, M_LINE);
    m1_curve(4, 232, y, 104, 54, KC[1]);
    for (x = 0; x < cyc; x++)                                   /* RATE: one tick per cycle */
        cv_rect(4 + x * 232 / cyc, 172, 2, 6, KC[0]);
    m1_disc(4, y[0], 3, KC[2]);                                 /* PHASE: the start */
    if (fade)                                                   /* FADE: the ramp */
        for (x = 0; x < fade; x += 3)
            cv_pset(4 + x, 104 - x * 54 / fade, KC[3]), cv_pset(4 + x, 104 + x * 54 / fade, KC[3]);
}

/* FX: the four sends as four small machines */
static void pg_fx(void)
{
    static const char *const N[4] = {"DIST", "CHORUS", "DELAY", "REVERB"};
    int32_t c, k;
    for (c = 0; c < 4; c++) {
        int32_t x0 = c * 60 + 5, cx = x0 + 25, v = pr(P_DIST + (uint32_t)c, 0, 127);
        uint16_t col = v ? KC[c] : M_DIM;
        m1_rrect(x0, 26, 50, 152, 4, M_PANEL);
        if (c == 0) {                                           /* the transfer curve, harder with the drive */
            int32_t prev = 0;
            for (k = 0; k <= 40; k++) {
                int32_t xx = (k - 20) * 1638, g = 1 + v / 100, yy = xx * g / (1 + ((xx < 0 ? -xx : xx) * (g - 1) >> 15));
                yy = 100 - yy * 30 / 32768;
                if (k)
                    m1_line(x0 + 4 + k, prev, x0 + 5 + k, yy, col);
                prev = yy;
            }
        } else if (c == 1) {                                    /* two voices drifting */
            for (k = 0; k < 2; k++) {
                int32_t i, prev = 0;
                for (i = 0; i <= 40; i++) {
                    int32_t yy = 100 - sine_i((uint32_t)i * 0x06000000u + (uint32_t)(k * v) * 0x00300000u) * 18 / 32768;
                    if (i)
                        m1_line(x0 + 4 + i, prev, x0 + 5 + i, yy, k ? m1_mix(M_BG, col, 8u) : col);
                    prev = yy;
                }
            }
        } else if (c == 2) {                                    /* the echoes going down with the feedback */
            int32_t h = 8 + v * 90 / 1000, fb = song.g[G_DFDBK] * 100 / 127;
            for (k = 0; k < 7 && h > 1; k++) {
                cv_rect(x0 + 6 + k * 6, 150 - h, 3, h, m1_mix(M_PANEL, col, (uint32_t)(16 - k * 2)));
                h = h * (30 + fb * 60 / 100) / 100;
            }
        } else {                                                /* the tail: a cloud, as tall as the room */
            int32_t n = 20 + v * 160 / 1000, hh = 30 + song.g[G_RSIZE] * 60 / 127;
            for (k = 0; k < n; k++) {
                uint32_t h = (uint32_t)k * 2654435761u;
                int32_t xx = x0 + 4 + (int32_t)((h >> 8) % 42u), yy = 150 - (int32_t)(((h >> 16) % 100u) * ((h >> 24) % 100u) * (uint32_t)hh / 10000u);
                cv_pset(xx, yy, m1_mix(M_PANEL, col, 6u + (h >> 4) % 10u));
            }
        }
        m1_text_c(cx, 162, FN_LAB, N[c], M_GREY);
    }
}

/* the global delay: echoes spaced by TIME, fading by FDBK, darker with COLOR, as loud as MIX */
static void pg_dly(void)
{
    int32_t gap = 8 + song.g[G_DTIME] * 30 / 127, fb = song.g[G_DFDBK], col = song.g[G_DCOLOR], mix = song.g[G_DMIX];
    int32_t h = 30 + mix * 100 / 127, x = 10, k;
    cv_rect(4, 160, 232, 1, M_LINE);
    for (k = 0; x < 232 && h > 1; k++) {
        uint16_t c = k ? m1_mix(KC[1], KC[2], (uint32_t)(col * 16 / 127)) : KC[3];
        m1_rrect(x, 160 - h, 6, h, 2, m1_mix(M_BG, c, (uint32_t)(16 - k > 4 ? 16 - k : 4)));
        if (k == 1)
            cv_rect(10 + 3, 166, x - 10, 1, KC[0]);                 /* TIME */
        x += gap;
        h = h * (20 + fb * 75 / 127) / 100;
    }
}

/* reverb and chorus: the room (SIZE, DAMP), the chorus (RATE, DEPTH) */
static void pg_rev(void)
{
    int32_t size = song.g[G_RSIZE], damp = song.g[G_RDAMP], rate = song.g[G_CRATE], depth = song.g[G_CDEPTH], k;
    int32_t r = 14 + size * 46 / 127;
    for (k = 0; k < 260; k++) {
        uint32_t h = (uint32_t)k * 2654435761u;
        int32_t a = (int32_t)(h >> 20), d = (int32_t)(((h >> 8) & 255u) * (uint32_t)r / 256u);
        int32_t x = 64 + sine_i((uint32_t)a << 20) * d / 32768, y = 104 + sine_i(((uint32_t)a << 20) + 0x40000000u) * d / 32768;
        cv_pset(x, y, m1_mix(KC[0], KC[1], (uint32_t)(damp * 16 / 127 * d / (r ? r : 1))));
    }
    m1_ring(64, 104, r, m1_mix(M_BG, KC[0], 6u));
    for (k = 0; k < 2; k++) {
        int32_t i, prev = 0, cyc = 1 + rate * 4 / 127;
        for (i = 0; i <= 100; i++) {
            int32_t yy = 104 - sine_i((uint32_t)(i * cyc) * 0x00A3D70Au + (uint32_t)k * 0x30000000u) * (4 + depth * 40 / 127) / 32768;
            if (i)
                m1_line(130 + i - 1, prev, 130 + i, yy, k ? KC[3] : KC[2]);
            prev = yy;
        }
    }
    m1_text_c(64, 172, FN_LAB, "REVERB", M_GREY);
    m1_text_c(180, 172, FN_LAB, "CHORUS", M_GREY);
}

/* SCL: one octave, the scale lit, the root in the first colour */
static void pg_scl(void)
{
    static const int8_t WK[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BX[5] = {0, 1, 3, 4, 5};
    const track_t *t = TSEL;
    uint32_t mask = scale_mask(t), i;
    char b[8];
    for (i = 0; i < 7u; i++) {
        uint32_t deg = ((uint32_t)WK[i] + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        uint16_t c = (mask >> deg) & 1u ? (deg == 0u ? KC[0] : KC[1]) : M_PANEL;
        m1_rrect(8 + (int32_t)i * 32, 34, 30, 132, 3, c);
    }
    for (i = 0; i < 5u; i++) {
        uint32_t deg = ((uint32_t)BK[i] + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        uint16_t c = (mask >> deg) & 1u ? (deg == 0u ? KC[0] : ui_light ? m1_mix(KC[1], M_TEXT, 6u) : m1_mix(M_BG, KC[1], 10u))
                                        : ui_light ? M_GREY : M_LINE;
        int32_t x = 8 + (BX[i] + 1) * 32 - 10;
        m1_rrect(x - 1, 33, 22, 82, 3, M_BG);
        m1_rrect(x, 34, 20, 80, 3, c);
    }
    if (t->p[P_TRANS]) {
        b[0] = 0;
        if (t->p[P_TRANS] > 0)
            str_cpy(b, "+", 4);
        fmt_int(b + str_len(b), t->p[P_TRANS]);
        m1_text_r(234, 172, FN_VALS, b, KC[3]);
    }
    m1_text(8, 172, FN_LAB, t->p[P_QUANT] ? "QUANTIZED" : "FREE", t->p[P_QUANT] ? KC[2] : M_DIM);
}

/* ARP: a chord of three notes through MODE, the beat grid of RATE, OCT bands, GATE lengths */
static void pg_arp(void)
{
    const track_t *t = TSEL;
    int32_t mode = pv(P_AMODE), oct = pv(P_AOCT), gate = pv(P_AGATE), rate = pv(P_ARATE), n = 3 * oct, i;
    static const uint8_t BEAT[6] = {1, 2, 4, 8, 3, 6};             /* notes per beat: 1/4 1/8 1/16 1/32 8T 16T */
    int32_t every = BEAT[rate % 6];
    static const int8_t CH[3] = {0, 4, 7};
    for (i = 0; i <= oct; i++)                                      /* octave bands */
        cv_rect(4, 170 - i * 138 / oct, 232, 1, i ? m1_mix(M_BG, KC[2], 4u) : M_LINE);
    for (i = 0; i < 16; i++) {
        int32_t k, x = 6 + i * 14, h, note;
        if (!(i % every))
            cv_rect(x - 2, 30, 1, 140, m1_mix(M_BG, KC[1], 5u));        /* the beats */
        switch (mode) {
        case 1: k = i % n; break;
        case 2: k = n - 1 - i % n; break;
        case 3: k = n > 1 ? (i % (2 * n - 2) < n ? i % (2 * n - 2) : 2 * n - 2 - i % (2 * n - 2)) : 0; break;
        case 4: k = (int32_t)((((uint32_t)i + 7u) * 2654435761u >> 24) % (uint32_t)n); break;
        default: k = i % n; break;
        }
        note = (k / 3) * 12 + CH[k % 3];
        h = 170 - (note + 1) * 138 / (12 * oct) - 4;
        m1_rrect(x, h, 2 + gate * 10 / 127, 6, 2, mode ? KC[0] : M_LINE);
        cv_rect(x, h + 8, 2 + gate * 10 / 127, 1, mode ? KC[3] : M_LINE);
    }
    (void)t;
}

/* STEP: the cursor's bank of 16 steps */
static void pg_step(void)
{
    const track_t *t = TSEL;
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = 0;
    for (i = 0; i < len; i++)
        if (step_on(&t->step[i])) {
            if (t->step[i].note[0] < lo)
                lo = t->step[i].note[0];
            if (t->step[i].note[0] > hi)
                hi = t->step[i].note[0];
        }
    if (lo > hi)
        lo = 48, hi = 72;
    if (hi - lo < 12)
        hi = lo + 12;
    for (i = 0; i < 16u; i++) {
        uint32_t si = base + i, j;
        const step_t *st = &t->step[si];
        int32_t x = 6 + (int32_t)(i % 8u) * 29, y = 26 + (int32_t)(i / 8u) * 80;
        int on = si < len && step_on(st);
        char b[4];
        if (si >= len) {
            m1_rrect_o(x, y, 25, 70, 4, M_PANEL);
            continue;
        }
        m1_rrect(x, y, 25, 70, 4, on ? m1_mix(M_BG, KC[1], 3u) : M_PANEL);
        if (si == ui.cursor)
            m1_rrect_o(x, y, 25, 70, 4, KC[0]);
        if (on)
            for (j = 0; j < st->n; j++) {
                int32_t hy = y + 58 - (st->note[j] - lo) * 50 / (hi - lo);
                m1_rrect(x + 4, hy, 17, 4, 2, j ? m1_mix(M_BG, KC[1], 10u) : KC[1]);
            }
        if (st->time == ST_TIE)
            cv_rect(x - 4, y + 34, 8, 2, KC[2]);
        if (st->flags & SF_ACCENT)
            m1_disc(x + 20, y + 5, 2, KC[3]);
        if (st->flags & SF_SLIDE)
            m1_thick(x + 15, y + 9, x + 21, y + 5, 1, KC[3]);
        if (song.playing && si == t->seq_idx)
            cv_rect(x, y + 72, 25, 3, M_TEXT);
        fmt_int(b, (int32_t)si + 1);
        m1_text_c(x + 13, y + 61, FN_LAB, b, M_DIM);
    }
}

/* PATTERN: the whole sequence (4 x 16), LEN, the swing of the off-steps, the GATE length */
static void pg_pattern(void)
{
    const track_t *t = TSEL;
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    int32_t sw = t->p[P_SSWING] * 5 / 127, gw = 2 + t->p[P_SGATE] * 9 / 127;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &t->step[i];
        int32_t x = 6 + (int32_t)(i % 16u) * 14 + (int32_t)(i % 16u) / 4 * 2 + ((i & 1u) ? sw : 0), y = 30 + (int32_t)(i / 16u) * 38;
        if (i >= len) {
            cv_rect(x, y + 26, 11, 1, M_PANEL);
            continue;
        }
        if (step_on(st))
            m1_rrect(x, y, gw, 26, 2, (st->flags & SF_ACCENT) ? M_TEXT : KC[3]);
        else if (st->time == ST_TIE)
            cv_rect(x, y + 12, 11, 2, m1_mix(M_BG, KC[3], 6u));
        else
            cv_rect(x, y + 26, 11, 1, M_LINE);
        if (song.playing && i == t->seq_idx)
            cv_rect(x, y + 29, 11, 2, M_TEXT);
    }
    if (len < NSTEP) {                                          /* LEN: where it loops */
        int32_t x = 4 + (int32_t)(len % 16u) * 14 + (int32_t)(len % 16u) / 4 * 2, y = 30 + (int32_t)(len / 16u) * 38;
        cv_rect(x, y - 2, 2, 32, KC[0]);
    }
}

/* TRACKS: three strips, name, level and meter, the pattern, pan */
static void pg_tracks(void)
{
    uint32_t c;
    for (c = 0; c < NPART; c++) {
        track_t *t = &trk[c];
        int32_t x0 = 4 + (int32_t)c * 79, sel = c == song.sel, lvl = t->p[P_LEVEL], mute = !lvl || t->p[P_MUTE];
        int32_t fy, m, pan = t->p[P_PAN];
        uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, i;
        char nm[16], b[4];
        const engine_t *e = ENGINES[t->eng_req % NENGINES];
        m1_rrect(x0, 24, 74, 160, 4, sel ? M_PANEL : M_BG);
        if (!sel)
            m1_rrect_o(x0, 24, 74, 160, 4, M_PANEL);
        m1_rrect(x0 + 5, 29, 14, 12, 3, mute ? M_DIM : track_col(c));
        b[0] = (char)('1' + c);
        b[1] = 0;
        m1_text_c(x0 + 12, 31, FN_LAB, b, M_BG);
        if ((song.rec >> c) & 1u)
            m1_disc(x0 + 27, 35, 3, song.playing ? M_REC : KC[3]);
        if (user_of(t) < UP_SLOTS)
            up_name(user_of(t), nm);
        else
            str_cpy(nm, e->presets[t->preset % e->npresets].name, sizeof nm);
        while (nm[0] && text_w(FN_SM, nm) > 66)
            nm[str_len(nm) - 1u] = 0;
        m1_text(x0 + 5, 45, FN_SM, nm, sel ? M_TEXT : M_GREY);
        /* level fader + meter */
        fy = 172 - lvl * 100 / 127;
        cv_rect(x0 + 10, 70, 2, 102, M_LINE);
        if (!mute)
            cv_rect(x0 + 10, fy, 2, 172 - fy, KC[1]);
        m1_rrect(x0 + 5, fy - 2, 12, 4, 2, mute ? M_DIM : sel ? M_TEXT : M_GREY);
        m = m1.meter[c];
        if (m)
            cv_rect(x0 + 21, 172 - m, 3, m, mute ? M_DIM : M_TEXT);
        /* the pattern over LEN */
        for (i = 0; i < len; i++) {
            const step_t *s = &t->step[i];
            int32_t r0 = 70 + (int32_t)(i * 102u / len), r1 = 70 + (int32_t)((i + 1u) * 102u / len), h = r1 - r0 > 2 ? r1 - r0 - 1 : 1;
            if (step_on(s))
                cv_rect(x0 + 32, r0, 4 + 5 * (int32_t)s->n, h, KC[2]);
            else if (s->time == ST_TIE)
                cv_rect(x0 + 32, r0, 2, r1 - r0, m1_mix(M_BG, KC[2], 6u));
            if (song.playing && i == t->seq_idx % len)
                cv_rect(x0 + 29, r0, 2, h, M_TEXT);
        }
        /* pan */
        cv_rect(x0 + 6, 178, 62, 1, M_LINE);
        cv_rect(x0 + 36 + pan * 30 / 64, 176, 2, 5, KC[3]);
    }
}

/* PRESETS: the eight families, the eight sounds of the one playing (user presets: their list) */
static void pg_presets(void)
{
    const track_t *t = TSEL;
    uint32_t fam = (t->preset / 8u) % MT_NFAM, i;
    int user = user_of(t) < UP_SLOTS;
    for (i = 0; i < MT_NFAM; i++) {                              /* the families, as tabs */
        int32_t cw = 232 / MT_NFAM, x = 4 + (int32_t)i * cw;
        if (!user && i == fam) {
            m1_rrect(x, 25, cw - 3, 13, 3, FAMC[i]);
            m1_text_c(x + (cw - 3) / 2, 28, FN_LAB, MT_FAM3[i], M_BG);
        } else {
            m1_rrect_o(x, 25, cw - 3, 13, 3, m1_mix(M_BG, FAMC[i], 9u));
            m1_text_c(x + (cw - 3) / 2, 28, FN_LAB, MT_FAM3[i], m1_mix(M_BG, FAMC[i], 13u));
        }
    }
    if (user) {
        int32_t row, first = clamp((int32_t)user_of(t) - 3, 0, UP_SLOTS - 8);
        m1_text(6, 44, FN_LAB, "USER", M_TEXT);
        for (row = 0; row < 8; row++) {
            uint32_t k = (uint32_t)(first + row);
            char tag[4], nm[13];
            int32_t y = 56 + row * 16;
            int sel = k == user_of(t);
            up_slot_label(tag, k);
            if (up_used(k))
                up_name(k, nm);
            else
                str_cpy(nm, "-", sizeof nm);
            if (sel) {
                cv_rect(0, y - 1, 240, 15, M_PANEL);
                cv_rect(0, y - 1, 3, 15, M_TEXT);
            }
            m1_text(8, y, FN_VALS, tag, sel ? M_TEXT : M_DIM);
            m1_text(40, y - 1, FN_SM, nm, sel ? M_TEXT : M_GREY);
        }
        return;
    }
    m1_text(6, 44, FN_LAB, MT_FAM[fam], FAMC[fam]);
    for (i = 0; i < 8u; i++) {
        uint32_t pi = fam * 8u + i;
        int32_t y = 56 + (int32_t)i * 16;
        int sel = pi == t->preset;
        char b[4];
        if (pi >= ENGINES[t->eng_req]->npresets)
            break;
        if (sel) {
            cv_rect(0, y - 1, 240, 15, M_PANEL);
            cv_rect(0, y - 1, 3, 15, FAMC[fam]);
        }
        fmt_int(b, (int32_t)i + 1);
        m1_text(9, y, FN_VALS, b, sel ? FAMC[fam] : M_DIM);
        m1_text(24, y - 1, FN_SM, ENGINES[t->eng_req]->presets[pi].name, sel ? M_TEXT : M_GREY);
    }
}

/* USER: the slots around the selected one */
static void pg_user(void)
{
    int32_t row, first = clamp((int32_t)ui.uslot - 4, 0, UP_SLOTS - 9);
    for (row = 0; row < 9; row++) {
        uint32_t k = (uint32_t)(first + row);
        int32_t y = 26 + row * 17;
        char tag[4], nm[13];
        int sel = k == ui.uslot, used = up_used(k);
        up_slot_label(tag, k);
        if (used)
            up_name(k, nm);
        else
            str_cpy(nm, "empty", sizeof nm);
        if (sel) {
            cv_rect(0, y - 1, 240, 16, M_PANEL);
            cv_rect(0, y - 1, 3, 16, KC[0]);
        }
        m1_text(8, y, FN_VALS, tag, sel ? KC[0] : M_DIM);
        m1_text(40, y - 1, FN_SM, nm, used ? (sel ? M_TEXT : M_GREY) : M_DIM);
    }
}

/* PROJECT: four slots */
static void pg_slots(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t x = 6 + (int32_t)(i % 2u) * 116, y = 26 + (int32_t)(i / 2u) * 80;
        int sel = (int32_t)i + 1 == song.g[G_SLOT], used = project_used(i);
        char b[4];
        m1_rrect(x, y, 112, 74, 4, used ? M_PANEL : M_BG);
        m1_rrect_o(x, y, 112, 74, 4, sel ? KC[0] : M_PANEL);
        b[0] = (char)('1' + i);
        b[1] = 0;
        m1_text(x + 10, y + 8, FN_NAME, b, sel ? M_TEXT : used ? M_GREY : M_DIM);
        m1_text(x + 10, y + 50, FN_LAB, used ? "SAVED" : "EMPTY", used ? KC[2] : M_DIM);
    }
}

/* SLICER: 16 steps of the gate pattern */
static void pg_slicer(void)
{
    const track_t *t = TSEL;
    uint32_t i, pat = sl_pattern(t), mode = (uint32_t)t->p[P_SLCR], cur = sl[t - trk].idx;
    int32_t open = 60 - t->p[P_SLDEPTH] * 60 / 127, y;
    uint16_t col = mode == SL_OFF ? M_DIM : KC[1];
    for (i = 0; i < 16u; i++) {
        int32_t x = 6 + (int32_t)i * 14 + (int32_t)(i / 4u) * 2;
        if ((pat >> i) & 1u) {
            m1_rrect(x, 50, 11, 100, 2, col);
        } else if (mode == SL_STUT) {
            for (y = 50; y < 150; y += 4)
                cv_rect(x, y, 11, 1, col);
        } else {
            cv_rect(x, 149, 11, 1, M_LINE);
            if (open)
                m1_rrect(x, 150 - open, 11, open, 2, m1_mix(M_BG, KC[3], 6u));
        }
        if (mode != SL_OFF && i == cur)
            cv_rect(x, 156, 11, 3, M_TEXT);
    }
}

/* every other page: four faders (an enum: its options stacked) */
static void pg_generic(void)
{
    const page_t *pg = cur_page();
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        knob_t k;
        int16_t *vp = 0;
        const param_desc_t *d = (pg->scope == SC_GLOBAL || pg->scope == SC_TRACK || pg->scope == SC_ENGINE)
                                    ? page_desc(pg, c, &vp) : 0;
        int32_t cx = (int32_t)c * 60 + 30;
        knob_info(c, &k);
        if (!k.lab[0])
            continue;
        if (d && vp && (d->fmt == F_ENUM || d->fmt == F_ONOFF) && d->names && d->max > d->min && d->max - d->min < 8 &&
            pg->id[c] != G_MIDI) {
            int32_t n = d->max - d->min + 1, i, hgt = 120 / n;
            for (i = 0; i < n; i++) {
                int32_t y = 160 - (i + 1) * hgt;
                int on = *vp - d->min == i;
                m1_rrect(cx - 18, y, 36, hgt - 4, 3, on ? KC[c] : M_PANEL);
                if (d->names && hgt >= 14)
                    m1_text_c(cx, y + (hgt - 4) / 2 - 4, FN_LAB, d->names[i], on ? M_BG : M_DIM);
            }
        } else if (k.ratio >= 0) {
            int32_t top = 36, bot = 160, fy = bot - k.ratio * (bot - top) / 1000;
            m1_rrect(cx - 5, top, 10, bot - top, 4, M_PANEL);
            if (k.bip) {
                int32_t mid = (top + bot) / 2;
                cv_rect(cx - 5, mid, 10, 1, M_LINE);
                m1_rrect(cx - 5, fy < mid ? fy : mid, 10, (fy < mid ? mid - fy : fy - mid) + 1, 2, k.dim ? M_DIM : KC[c]);
            } else {
                m1_rrect(cx - 5, fy, 10, bot - fy, 4, k.dim ? M_DIM : KC[c]);
            }
            cv_rect(cx - 9, fy - 1, 18, 2, M_TEXT);
        } else {
            m1_text_c(cx, 92, FN_MID, k.val, k.dim ? M_DIM : KC[c]);
        }
        m1_text_c(cx, 170, FN_LAB, k.lab, M_GREY);
    }
}

/* -------------------------------------------------------------- the main --- */
enum { PG_HOME, PG_FM, PG_ESPACE, PG_ENV, PG_LFO, PG_FX, PG_DLY, PG_REV, PG_SCL, PG_ARP, PG_STEP, PG_PAT,
       PG_TRK, PG_PRESETS, PG_USER, PG_SLOTS, PG_SLCR, PG_GEN };

static uint32_t page_kind(void)
{
    const page_t *pg = cur_page();
    if (ui.home)
        return PG_HOME;
    if (pg->scope == SC_ENGINE)
        return pg->id[0] == P_E0 ? PG_FM : PG_ESPACE;
    switch (pg->graph) {
    case GR_ADSR: return PG_ENV;
    case GR_LFO: return PG_LFO;
    case GR_FX: return PG_FX;
    case GR_SCALE: return PG_SCL;
    case GR_ARP: return PG_ARP;
    case GR_ROLL: return PG_STEP;
    case GR_STEPS: return PG_PAT;
    case GR_TRK: return PG_TRK;
    case GR_BROWSE: return PG_PRESETS;
    case GR_USER: return PG_USER;
    case GR_SLOTS: return PG_SLOTS;
    case GR_SLCR: return PG_SLCR;
    default: break;
    }
    if (pg->id[0] == G_DTIME && pg->scope == SC_GLOBAL)
        return PG_DLY;
    if (pg->id[0] == G_RSIZE && pg->scope == SC_GLOBAL)
        return PG_REV;
    return PG_GEN;
}

static void draw_page(uint32_t kind)
{
    switch (kind) {
    case PG_HOME: pg_home(); break;
    case PG_FM: pg_fm(); break;
    case PG_ESPACE: pg_espace(); break;
    case PG_ENV: pg_env(); break;
    case PG_LFO: pg_lfo(); break;
    case PG_FX: pg_fx(); break;
    case PG_DLY: pg_dly(); break;
    case PG_REV: pg_rev(); break;
    case PG_SCL: pg_scl(); break;
    case PG_ARP: pg_arp(); break;
    case PG_STEP: pg_step(); break;
    case PG_PAT: pg_pattern(); break;
    case PG_TRK: pg_tracks(); break;
    case PG_PRESETS: pg_presets(); break;
    case PG_USER: pg_user(); break;
    case PG_SLOTS: pg_slots(); break;
    case PG_SLCR: pg_slicer(); break;
    default: pg_generic(); break;
    }
}

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 6 dB = 10 px */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);
    return clamp((v - 48) * 100 / 80, 0, 100);
}

static void draw_main(void)
{
    uint32_t kind = page_kind(), i, b, sig, anim = 0, mask = 0;
    const track_t *t = TSEL;
    sig = kind * 2654435761u + song.sel * 7777u + TSEL->preset * 131u + TSEL->user * 257u + up_gen * 7919u +
          ui.uslot * 104729u + (uint32_t)song.g[G_SLOT] * 13u + KC[0] + ui.page * 31u;
    for (i = 0; i < P_COUNT; i++)
        sig = (sig ^ (uint32_t)t->p[i]) * 16777619u;
    for (i = G_DTIME; i <= G_CDEPTH; i++)
        sig = (sig ^ (uint32_t)song.g[i]) * 16777619u;
    switch (kind) {
    case PG_HOME:
        anim = ui.frame / 2u;                                       /* the scope: every other frame */
        mask = 2u;
        break;
    case PG_STEP:
    case PG_PAT:
        sig ^= steps_hash(t) + ui.cursor * 7919u + ui.bank * 3u + (uint32_t)song.playing * 5u;
        anim = song.playing ? t->seq_idx + 1u : 0u;
        mask = 3u;
        break;
    case PG_TRK:
        for (i = 0; i < NPART; i++) {                               /* the meters fall ~20 dB/s */
            int32_t m = trk[i].p[P_MUTE] ? 0 : meter_px(trk[i].peak);
            trk[i].peak = 0;
            if (m < m1.meter[i] - 3)
                m = m1.meter[i] - 3;
            m1.meter[i] = (uint8_t)(m < 0 ? 0 : m);
            anim = anim * 131u + m1.meter[i] + (song.playing ? trk[i].seq_idx * 7u : 0u);
            sig ^= steps_hash(&trk[i]) * (i + 1u) + (uint32_t)trk[i].preset * 31u + (uint32_t)((song.rec >> i) & 1u) * 977u;
        }
        mask = 3u;
        break;
    case PG_SLCR:
        anim = t->p[P_SLCR] ? sl[song.sel].idx + 1u : 0u;
        mask = 3u;
        break;
    case PG_SLOTS:
        for (i = 0; i < 4u; i++)
            sig ^= (uint32_t)project_used(i) << (20u + i);
        break;
    case PG_USER:
        sig ^= ui.arm * 3u;
        break;
    default:
        break;
    }
    for (b = 0; b < 2u; b++) {
        uint32_t s = sig + b * 0x9E3779B9u + ((mask >> b) & 1u ? anim * 2246822519u : 0u);
        if (!ui.force && s == m1.band[b])
            continue;
        m1.band[b] = s;
        m1_begin(240, H_BAND, 0, (int32_t)(Y_MAIN + b * H_BAND));
        cv_oy = -(int32_t)(Y_MAIN + b * H_BAND);
        draw_page(kind);
        if (ui_style == 1u) {                                       /* FILM: letterbox, sprockets */
            int32_t yb = b ? Y_MAIN + 2 * H_BAND - 7 : Y_MAIN, k;
            cv_rect(0, yb, 240, 7, RGB(0, 0, 0));
            for (k = 4; k < 240; k += 14)
                m1_rrect(k, yb + 2, 7, 3, 1, M_PANEL);
        }
        cv_oy = 0;
        cv_blit(0, Y_MAIN + b * H_BAND);
    }
}

/* ------------------------------------------------------- the opening, 5 s --- */
/* the opening, as the old handheld consoles did it: the name comes down from the top, stops in the
 * middle, the chime (fx.c m1_chime), then MODULAR ONE under it; 5 s, a note skips it */
#define SPL_STOP 1700                                               /* ms: the name reaches the middle */
#define SPL_Y 92
static const uint16_t GB_STEP[4] = {GB0, GB1, GB2, GB3};
static int32_t spl_y(int32_t t) { return t >= SPL_STOP ? SPL_Y : -34 + (SPL_Y + 34) * t / SPL_STOP; }

static void spl_region(int32_t y0, int32_t h, int32_t t)
{
    int32_t fade = clamp(t > 4500 ? (5000 - t) * 16 / 500 : 16, 0, 16), a;
    while (h > 0) {
        int32_t bh = h > 120 ? 120 : h;
        m1_begin(240, (uint32_t)bh, 0, y0);
        cv_oy = -y0;
        /* fades in the console's way: the four shades in steps, never a colour between them */
        m1_text_c(120, spl_y(t), FN_NAME, "BUNGLE SOCIETY", GB_STEP[(uint32_t)fade * 3u / 16u]);   /* the name */
        a = clamp((t - 2300) * 16 / 700, 0, 16) * fade / 16;
        if (a >= 6)
            m1_text_c(120, 150, FN_LAB, "MODULAR ONE", GB_STEP[(uint32_t)a * 2u / 16u + 1u > 2u ? 2u : (uint32_t)a * 2u / 16u + 1u]);
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
        y0 += bh;
        h -= bh;
    }
}

static void m1_splash(uint32_t ms)
{
    static int32_t last = -1000;
    int32_t t = (int32_t)ms, y = spl_y(t);
    if (last < -100 || t < last) {                                  /* the first frame: the whole paper */
        spl_region(0, 240, t);
        last = y;
    }
    if (t >= SPL_STOP && !m1.chimed) {
        m1.chimed = 1;
        m1_chime_req = 1;
    }
    {   /* only the rows the name crosses, then the line under it while it fades in or out */
        int32_t lo = (last < y ? last : y) - 2, hi = (last > y ? last : y) + 34;
        if (lo < 0)
            lo = 0;
        if (hi > lo)
            spl_region(lo, hi - lo, t);
        if ((t >= 2300 && t < 3100) || t >= 4450)
            spl_region(146, 14, t);
        if (t >= 4450)
            spl_region(SPL_Y - 2, 36, t);
        last = y;
    }
}

/* any touch, a note, a page: the screen saver waits a minute more */
static void m1_touch_check(void)
{
    uint32_t i, sig = ui.page * 3u + ui.home * 5u + song.sel * 7u + (uint32_t)song.g[G_BPM] * 11u + fm1_in.notes * 13u +
                      ui.menu * 17u + TSEL->preset * 19u + (uint32_t)song.octave * 23u + song.rec * 29u;
    for (i = 0; i < P_COUNT; i++)
        sig = (sig ^ (uint32_t)TSEL->p[i]) * 16777619u;
    if (sig != m1.last_sig || ui.hot_t || ui.msg_t || fm1_in.notes) {
        m1.last_sig = sig;
        m1.touch_ms = fm1_ms;
    }
}

static void ui_tick(void)
{
    if (ui.msg_t)
        ui.msg_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.arm_t && !--ui.arm_t)
        ui.arm = 0;
    if (ui.hot_t)
        ui.hot_t--;
}

/* ------------------------------------------------------------ the player --- */
/* While the player runs (seq.c): a tape recorder seen from above, nothing else. Two open reels
 * turn, the tape goes from the left one to the right one as the piece goes, down around two
 * rollers and under the head. Below: the piece's name, the sound, the time. KNOB 1 pops the list
 * up over the bottom half (ui_input.c: the choice plays when the knob rests, the list goes 5 s
 * later). Two strips drawn from the same scene: the reels at ~15 frames/s, the bottom when it
 * changes. */
#define PL_RY 74                                                    /* the reels' centres */
#define PL_RL 64
#define PL_RR 176
#define PL_R 50                                                     /* their flanges */
#define PL_GY 148                                                   /* the rollers */
#define PL_GL 20
#define PL_GR 220
#define PL_LIST_MS 5000u                                            /* the list stays up 5 s after the knob */

static void pl_time(char *b, uint32_t d10)
{
    uint32_t s = d10 / 100u, k = 0;
    if (s >= 600u)
        b[k++] = (char)('0' + s / 600u % 10u);
    b[k++] = (char)('0' + s / 60u % 10u);
    b[k++] = ':';
    b[k++] = (char)('0' + s % 60u / 10u);
    b[k++] = (char)('0' + s % 10u);
    b[k] = 0;
}

static void pl_reel(int32_t cx, int32_t cy, int32_t tape, uint32_t ang)
{
    uint32_t k;
    m1_disc(cx, cy, PL_R, M_TEXT);                                 /* the flange */
    m1_disc(cx, cy, PL_R - 2, M_BG);
    m1_disc(cx, cy, tape, M_DIM);                                  /* the tape wound on it */
    for (k = 0; k < 3u; k++) {                                     /* its three spokes, turning */
        uint32_t a = ang + k * 0x55555555u;
        int32_t c0 = sine_i(a + 0x40000000u), s0 = sine_i(a);
        int32_t x0 = cx + c0 * 11 / 32768, y0 = cy + s0 * 11 / 32768;
        int32_t x1 = cx + c0 * (PL_R - 3) / 32768, y1 = cy + s0 * (PL_R - 3) / 32768;
        cv_line(x0, y0, x1, y1, M_TEXT);
        cv_line(x0 + 1, y0, x1 + 1, y1, M_TEXT);
    }
    m1_disc(cx, cy, 11, M_PANEL);                                  /* the hub */
    m1_ring(cx, cy, 11, M_TEXT);
    m1_disc(cx, cy, 2, M_TEXT);
}

/* the whole deck in screen coordinates; the strip being drawn clips it (cv_oy) */
static void pl_deck(uint32_t p1000, uint32_t ang)
{
    int32_t lt = 16 + (int32_t)((1000u - p1000) * 30u / 1000u), rt = 16 + (int32_t)(p1000 * 30u / 1000u);
    cv_line(PL_RL - lt, PL_RY, PL_GL - 6, PL_GY, M_TEXT);          /* the tape: down to the left roller, */
    cv_line(PL_GL, PL_GY + 6, PL_GR, PL_GY + 6, M_TEXT);           /* across under the head, */
    cv_line(PL_RR + rt, PL_RY, PL_GR + 6, PL_GY, M_TEXT);          /* up to the right reel */
    m1_disc(PL_GL, PL_GY, 6, M_PANEL);                             /* the rollers */
    m1_ring(PL_GL, PL_GY, 6, M_TEXT);
    m1_disc(PL_GL, PL_GY, 1, M_TEXT);
    m1_disc(PL_GR, PL_GY, 6, M_PANEL);
    m1_ring(PL_GR, PL_GY, 6, M_TEXT);
    m1_disc(PL_GR, PL_GY, 1, M_TEXT);
    cv_rect(108, PL_GY - 2, 24, 8, M_TEXT);                        /* the head, its two guides */
    cv_rect(86, PL_GY + 1, 6, 5, M_DIM);
    cv_rect(148, PL_GY + 1, 6, 5, M_DIM);
    pl_reel(PL_RL, PL_RY, lt, ang);
    pl_reel(PL_RR, PL_RY, rt, ang + 0x2AAAAAAAu);
}

static void m1_player_draw(int force)
{
    static uint32_t last_frame, low_sig;
    const pl_item_t *it = &pl_item[pl_idx % PL_MAX];
    uint32_t el = demo_pos / 441u, dur = it->dur10, p1000 = dur ? (el >= dur ? 1000u : el * 1000u / dur) : 0u;
    uint32_t ang = demo_pos * 1715u, sig;                          /* a turn every ~2.5 s */
    int list = pl_list_ms && fm1_ms - pl_list_ms < PL_LIST_MS;
    char b[24], c[16];
    if (force || ui.frame - last_frame >= 4u) {                     /* the reels */
        last_frame = ui.frame;
        m1_begin(240, 124, 0, 0);
        pl_deck(p1000, ang);
        if (ui.msg_t)
            m1_text(6, 3, FN_SM, ui.msg, M_TEXT);
        fmt_int(b, (int32_t)pl_idx + 1);
        str_cpy(b + str_len(b), " / ", 4);
        fmt_int(c, (int32_t)pl_count);
        str_cpy(b + str_len(b), c, 8);
        m1_text_r(234, 3, FN_LAB, b, M_GREY);
        cv_blit(0, 0);
    }
    sig = pl_idx * 7u + pl_cursor * 131u + pl_count * 977u + el / 100u * 13u + TSEL->preset * 31u +
          (uint32_t)TSEL->eng_req * 5u + (uint32_t)list * 7919u + p1000 / 30u * 101u + pl_del * 65537u;
    if (!force && sig == low_sig)
        return;
    low_sig = sig;
    m1_begin(240, 116, 0, 124);
    cv_oy = -124;
    if (!list) {                                                   /* the rollers, the name, the time */
        pl_deck(p1000, ang);
        m1_text(8, 166, FN_NAME, it->name, M_TEXT);
        str_cpy(c, ENGINES[TSEL->eng_req]->presets[TSEL->preset % ENGINES[TSEL->eng_req]->npresets].name, sizeof c);
        m1_text_r(232, 196, FN_LAB, c, M_GREY);
        pl_time(b, el);
        str_cpy(b + str_len(b), " / ", 4);
        pl_time(c, dur);
        str_cpy(b + str_len(b), c, 8);
        m1_text(8, 196, FN_LAB, b, M_TEXT);
        cv_rect(8, 212, 224, 2, M_DIM);                            /* where the piece is */
        cv_rect(8, 212, (int32_t)(224u * p1000 / 1000u), 2, M_TEXT);
        m1_text_c(120, 224, FN_LAB, "K1 LIST  OCT+ NEXT  OCT- START", M_GREY);
    } else {                                                       /* the list, popped up */
        int32_t first = clamp((int32_t)pl_cursor - 2, 0, pl_count > 6u ? (int32_t)pl_count - 6 : 0), row;
        pl_deck(p1000, ang);
        cv_rect(4, 128, 232, 108, M_BG);
        cv_rect(4, 128, 232, 1, M_TEXT);
        cv_rect(4, 235, 232, 1, M_TEXT);
        cv_rect(4, 128, 1, 108, M_TEXT);
        cv_rect(235, 128, 1, 108, M_TEXT);
        for (row = 0; row < 6 && first + row < (int32_t)pl_count; row++) {
            uint32_t k = (uint32_t)(first + row);
            int32_t y = 133 + row * 17;
            int sel = k == pl_cursor, playing = k == pl_idx;
            if (pl_del == k + 1u) {                                /* "erase?" */
                cv_rect(5, y - 3, 230, 16, M_TEXT);
                str_cpy(b, "ERASE ", 8);
                str_cpy(b + 6, pl_item[k].name, sizeof b - 6u);
                m1_text(12, y, FN_LAB, b, M_BG);
                m1_text_r(228, y, FN_LAB, "HOLD REC", M_BG);
                continue;
            }
            if (sel)
                cv_rect(5, y - 3, 230, 16, M_PANEL);
            fmt_int(b, (int32_t)k + 1);
            m1_text(10, y, FN_LAB, playing ? ">" : " ", M_TEXT);
            m1_text(22, y, FN_LAB, b, sel || playing ? M_TEXT : M_GREY);
            m1_text(44, y, FN_LAB, pl_item[k].name, sel || playing ? M_TEXT : M_GREY);
            pl_time(c, pl_item[k].dur10);
            m1_text_r(230, y, FN_LAB, c, M_GREY);
        }
    }
    cv_oy = 0;
    cv_blit(0, 124);
}

static void ui_draw(void)
{
    ui.frame++;
    m1_theme_apply(0);
    if (!m1.started) {                                              /* the opening */
        m1.started = 1;
        m1.splash = 1;
        m1.splash_t0 = fm1_ms;
        m1.touch_ms = fm1_ms;
        lcd_fill(0, 0, 240, 240, M_BG);
    }
    if (m1.splash) {
        uint32_t ms = fm1_ms - m1.splash_t0;
        if (ms < SPLASH_MS && !fm1_in.notes && !ui.menu) {
            m1_splash(ms);
            ui_tick();
            return;
        }
        m1.splash = 0;
        m1.touch_ms = fm1_ms;
        ui.force = 1;
    }
    m1_touch_check();
    if (ui.menu) {
        m1.saver = 0;
        draw_menu();
        ui.force = 0;
        return;
    }
    if (ui.confirm) {                                               /* clear-the-sequence dialog */
        if (ui.force) {
            char b[20] = "CLEAR TRACK 1?", s[24] = "CLEAR T1 SEQUENCE?";
            b[12] = (char)('1' + ui.confirm_trk);
            s[7] = b[12];
            lcd_fill(0, 0, 240, 240, M_BG);
            m1_begin(240, 80, 0, 80);
            m1_text_c(120, 10, FN_MID, ui.confirm == 2 ? b : s, M_TEXT);
            m1_text_c(70, 50, FN_LAB, "OCT-  NO", M_GREY);
            m1_text_c(170, 50, FN_LAB, "OCT+  YES", M_REC);
            cv_blit(0, 80);
            ui.force = 0;
        }
        return;
    }
    {                                                               /* the player: the cassette */
        static uint8_t was;
        if (demo_on) {
            if (m1.saver) {
                m1.saver = 0;
                city_leave();
            }
            m1_player_draw(!was || ui.force);
            was = 1;
            ui.force = 0;
            ui_tick();
            return;
        }
        if (was) {
            was = 0;
            ui.force = 1;
        }
    }
    if (!SAVER_OFF && TSEL->eng_req == MT_ENGINE && fm1_ms - m1.touch_ms > SAVER_MS) {   /* the window on the town */
        if (!m1.saver)
            city_leave();                                           /* (a fresh full picture) */
        m1.saver = 1;
        city_draw();
        ui.force = 0;
        return;
    }
    if (m1.saver) {                                                 /* woken: the page again */
        m1.saver = 0;
        city_leave();
        ui.force = 1;
    }
    cursor_fix();
    if (ui.force)
        lcd_fill(0, 0, 240, 240, M_BG);
    felucca_dbg.stage = 3;
    draw_top();
    felucca_dbg.stage = 4;
    draw_main();
    felucca_dbg.stage = 5;
    draw_knobs();
    ui_tick();
    felucca_dbg.stage = 6;
    ui.force = 0;
}
