/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Town FM: the window. When the selected track plays TOWN, the screen looks across the
 * water at Modular Town: the far shore under a large sky, the island on the sea horizon.
 * Nothing performs; the light changes, and a few things drift:
 *   NUIT   the hour: the sun goes down behind the island, the sky turns, windows light up one
 *          by one, the lamps of the quay come on, the moon and the stars come out
 *   BRIL   the sun (or the moon) and its path of glints on the water
 *   MONT   the evening fire in the clouds
 *   SUS    how many windows stay lit at night
 *   REL    the water: long release, calm water and long reflections
 *   ILE    the island comes out of the haze, its lantern lights at night
 *   notes  rings on the water; the sound feeds the glints
 * Clouds drift, barges glide past now and then, windows go on and off as people live behind them.
 * The values show in a quiet band only while a knob turns.
 *
 * No framebuffer: the picture is rebuilt a few rows at a time (row decode -> palette or computed
 * sky / water -> canvas -> panel), the upper rows and the water each on their own rolling cursor,
 * so every change of light reaches the whole screen within a second at a constant small cost.
 * The barges, the rings and the bands are redrawn as rectangles. All in the main loop: never in
 * the audio interrupt. */
#include "mt_city_data.h"

static uint32_t str_hash(uint32_t h, const char *s);   /* ui_draw.c */
static void ui_tick(void);                             /* ui_draw.c */

#define CITY_H 240
static const uint8_t GB_LUMA_CITY[5] = {24, 64, 128, 196, 6};   /* the town's light on the four shades */
#define BAND_TOP 16                      /* the header band: rows 0..15 */
#define BAND_BOT 222                     /* the values band: rows 222..239 */
#define BAND_T 110                       /* frames a band stays */
#define ROLL_UP 4u                       /* rows of the upper picture rebuilt per frame */
#define ROLL_WATER 4u                    /* rows of water rebuilt per frame */

typedef struct { int16_t x, y, w, h; } crect_t;

typedef struct {
    int32_t x8;                          /* left edge, 1/256 px */
    int16_t yb, len, dir, speed;         /* waterline row, length, -1 / +1, 1/256 px per frame */
    uint8_t on;
    int16_t drawn;                       /* left edge last drawn */
} barge_t;

typedef struct { int16_t x, y, r, t; } ring_t;

static struct {
    uint8_t shown, nlev, isle, lit[MT_NWIN];
    int8_t jit[MT_NWIN];
    uint32_t tick, rng;
    uint16_t pal[256];                   /* native RGB565 */
    uint8_t sky[MT_ROWS][3];
    uint8_t water[3];
    int16_t sun_x, sun_y, moon_a;        /* moon_a 0..16 */
    int32_t bril, mont, sus, rel, lev, lev_slow;
    uint16_t ucur, wcur, settle;         /* settle: frames of fast rebuild after a change of light */
    barge_t barge[2];
    ring_t ring[4];
    uint16_t ring_gap;
    /* bands */
    char lab[4][6], val[4][10], head[28];
    uint8_t hot, top_t, bot_t;
    uint32_t head_sig, val_sig;
    crect_t dirty[16];
    uint8_t ndirty;
} city __attribute__((section(".pool")));   /* zeroed at boot */

static uint32_t city_rand(void)
{
    city.rng = city.rng * 1664525u + 1013904223u;
    return city.rng >> 8;
}

static inline uint16_t c565(int32_t r, int32_t g, int32_t b)
{
    r = clamp(r, 0, 255);
    g = clamp(g, 0, 255);
    b = clamp(b, 0, 255);
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
static inline int32_t lerpi(int32_t a, int32_t b, int32_t t, int32_t sh) { return a + (((b - a) * t) >> sh); }

/* a + (b - a) * t / 16, in RGB565 */
static inline uint16_t mix565(uint32_t a, uint32_t b, uint32_t t)
{
    int32_t ar = (int32_t)(a >> 11), ag = (int32_t)((a >> 5) & 63u), ab = (int32_t)(a & 31u);
    int32_t br = (int32_t)(b >> 11), bg = (int32_t)((b >> 5) & 63u), bb = (int32_t)(b & 31u);
    return (uint16_t)(((ar + (((br - ar) * (int32_t)t) >> 4)) << 11) | ((ag + (((bg - ag) * (int32_t)t) >> 4)) << 5) |
                      (ab + (((bb - ab) * (int32_t)t) >> 4)));
}

static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t t)
{
    uint32_t h = x * 73856093u ^ y * 19349663u ^ t * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

/* ------------------------------------------------------------- the light --- */
/* the colours of things at the hour n (0 day .. 32 night): a little warmth at dusk, then the blue night */
static void tint(const uint8_t *c, uint32_t n, int32_t *o)
{
    int32_t k, dusk = (int32_t)(n < 16u ? n : 32u - n);           /* 0..16 */
    static const int32_t WARM[3] = {236, 150, 120}, NIGHT[3] = {26, 24, 52};
    for (k = 0; k < 3; k++) {
        int32_t v = c[k];
        v += (WARM[k] - v) * dusk * 3 / 256;                        /* up to 19 % warmer at dusk */
        v += (NIGHT[k] - v) * (int32_t)n * 26 / 1024;               /* up to 81 % night */
        o[k] = v;
    }
}
static uint16_t tint565(const uint8_t *c, uint32_t n)
{
    int32_t o[3];
    tint(c, n, o);
    return c565(o[0], o[1], o[2]);
}

static void city_sky_rows(void)
{
    static const uint8_t KEY[3][2][3] = {                             /* top, horizon: day, dusk, night */
        {{150, 166, 196}, {234, 214, 194}},
        {{86, 70, 122}, {248, 158, 112}},
        {{12, 12, 34}, {50, 40, 86}},
    };
    static const uint8_t WK[3][3] = {{86, 104, 128}, {72, 60, 100}, {12, 14, 32}};
    uint32_t n = city.nlev, y, c, k = n < 16u ? 0u : 1u, t = (n & 15u) * 16u;
    if (n >= 32u)
        k = 1u, t = 256u;
    for (y = 0; y < MT_ROWS; y++) {
        uint32_t f = y < MT_HZ ? y * 256u / MT_HZ : 256u;
        f = f * f / 256u;                                              /* the horizon glow stays low */
        for (c = 0; c < 3u; c++) {
            int32_t a = lerpi(KEY[k][0][c], KEY[k][1][c], (int32_t)f, 8), b = lerpi(KEY[k + 1u][0][c], KEY[k + 1u][1][c], (int32_t)f, 8);
            city.sky[y][c] = (uint8_t)lerpi(a, b, (int32_t)t, 8);
        }
    }
    for (c = 0; c < 3u; c++)                                           /* the water's own colour */
        city.water[c] = (uint8_t)lerpi(WK[k][c], WK[k + 1u][c], (int32_t)t, 8);
    city.sun_x = MT_ISLE_X - 7;                                      /* above the island */
    city.sun_y = (int16_t)(50 + (int32_t)n * 5);                     /* it sets behind the island */
    city.moon_a = (int16_t)clamp((int32_t)n - 14, 0, 16);
}

static void city_palette(void)
{
    uint32_t i, n = city.nlev;
    static const uint8_t PANE[3] = {62, 72, 92}, LAMP_OFF[3] = {206, 198, 180};
    for (i = 0; i < MT_NPAL; i++)
        city.pal[i] = tint565(MT_PAL + 3u * i, n);
    {   /* the island: from the haze of the horizon (ILE off) to its colours (ILE on) */
        const uint8_t *hz = city.sky[MT_HZ - 2];
        uint32_t f = 3u + city.isle * 13u / 32u;                       /* 3..16 */
        for (i = 0; i < 8u; i++) {
            int32_t o[3];
            tint(MT_ISLE_PAL + 3u * i, n, o);
            city.pal[MT_I_ISLE + i] = mix565(c565(hz[0], hz[1], hz[2]), c565(o[0], o[1], o[2]), f);
        }
        if (city.isle > 24u && n >= 8u)                                /* the lantern */
            city.pal[MT_I_ISLE + 6u] = c565(255, 204, 128);
    }
    city.pal[MT_I_LAMP] = n >= 12u ? c565(255, 218, 152) : tint565(LAMP_OFF, n);
    {   /* the handheld: by day the town a shade under its sky, at night a shade over it (the sky goes black) */
        int32_t gain = 12 + (int32_t)n * 12 / 32;                       /* /16: x 0.75 by day .. x 1.5 at night */
        for (i = 0; i < 256u; i++)
            if (city.pal[i] && !(i == MT_I_LAMP && n >= 12u) && !(i == MT_I_ISLE + 6u && city.isle > 24u && n >= 8u)) {
                uint32_t c = city.pal[i];
                city.pal[i] = c565((int32_t)((c >> 11) << 3) * gain / 16, (int32_t)(((c >> 5) & 63u) << 2) * gain / 16,
                                   (int32_t)((c & 31u) << 3) * gain / 16);
            }
    }
    for (i = 0; i < MT_NWIN; i++)
        city.pal[MT_I_WIN + i] = city.lit[i] ? c565(MT_WIN_LIT[3u * i], MT_WIN_LIT[3u * i + 1u], MT_WIN_LIT[3u * i + 2u])
                                             : tint565(PANE, n * 3u / 4u + 8u);
}

/* ------------------------------------------------------------------ sky --- */
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

typedef struct { int16_t y, len, th, sp, ph; } cloud_t;
static const cloud_t CLOUDS[5] = {      /* long spring strata; sp: frames per pixel */
    {30, 92, 3, 160, 10}, {46, 60, 2, 210, 150}, {72, 120, 3, 130, 60}, {88, 70, 2, 190, 200}, {101, 54, 2, 240, 30},
};

static uint16_t sky_px(int32_t x, int32_t y)
{
    const uint8_t *s = city.sky[y];
    int32_t r = s[0], g = s[1], b = s[2], n = (int32_t)city.nlev, k;
    /* the sun, low and hazy; BRIL widens its halo */
    if (city.sun_y < MT_HZ + 8) {
        int32_t dx = x - city.sun_x, dy = y - city.sun_y, d2 = dx * dx + dy * dy, R = 9 + (city.bril >> 11);
        if (d2 < 30) {
            int32_t red = clamp(n * 8, 0, 160);
            r = 255;
            g = 236 - red / 2;
            b = 196 - red;
        } else if (d2 < R * R * 4) {
            int32_t a = (R * R * 4 - d2) * (10 + (city.bril >> 12)) / (R * R * 4);   /* /32 */
            r += (255 - r) * a / 32;
            g += (220 - g) * a / 32;
            b += (170 - b) * a / 32;
        }
    }
    /* clouds: thin strata, lit from below at dusk (MONT sets them on fire) */
    for (k = 0; k < 5; k++) {
        const cloud_t *c = &CLOUDS[k];
        int32_t L = c->len, x0 = (int32_t)((c->ph + city.tick / (uint32_t)c->sp) % (uint32_t)(240 + L)) - L, u = x - x0, dy, hx;
        if (u <= 0 || u >= L)
            continue;
        dy = y - c->y;
        if (dy < -c->th || dy > c->th)
            continue;
        hx = u * (L - u) * 4 * c->th * 16 / (L * L) + (sine_i((uint32_t)u * (10u << 24) + (uint32_t)k * (48u << 24)) >> 11);   /* /16, lumpy */
        if ((dy < 0 ? -dy : dy) * 16 > hx)
            continue;
        {
            int32_t edge = (dy < 0 ? -dy : dy) * 16 > hx - 12;                /* the soft edge, dithered */
            if (edge && ((x + y) & 1))
                goto stars;
        }
        {
            int32_t dusk = n < 16 ? n : 32 - n, fire = clamp(dusk * (8 + (city.mont >> 11)) / 24, 0, 16);
            int32_t cr = lerpi(232, 250, fire, 4), cg = lerpi(226, 150, fire, 4), cb = lerpi(222, 132, fire, 4);
            int32_t a = dy > 0 ? 10 : 6;                                 /* the lit underside */
            if (n > 20)                                                 /* night: dark veils over the stars */
                cr = 46, cg = 42, cb = 72, a = 7;
            r += (cr - r) * a / 16;
            g += (cg - g) * a / 16;
            b += (cb - b) * a / 16;
            goto dither;
        }
    }
stars:
    /* the moon and the stars */
    if (city.moon_a) {
        int32_t dx = x - 54, dy = y - 34, d2 = dx * dx + dy * dy;
        if (d2 <= 12) {
            int32_t a = city.moon_a;
            r += (232 - r) * a / 16;
            g += (232 - g) * a / 16;
            b += (238 - b) * a / 16;
        } else if (d2 < 200) {
            int32_t a = (200 - d2) * city.moon_a / 1100;
            r += (180 - r) * a / 16;
            g += (184 - g) * a / 16;
            b += (214 - b) * a / 16;
        } else if (y < 118) {
            uint32_t h = hash3((uint32_t)x, (uint32_t)y, 7u);
            if ((h & 2047u) < 6u) {
                int32_t a = city.moon_a * (12 + (int32_t)((h >> 11) & 3u)) / 16;
                if ((h >> 13) & 1u)                                      /* some twinkle */
                    a = a * (int32_t)(8u + (hash3((uint32_t)x, (uint32_t)y, city.tick >> 5) & 7u)) / 16;
                r += (236 - r) * a / 16;
                g += (232 - g) * a / 16;
                b += (240 - b) * a / 16;
            }
        }
    }
dither:
    {
        int32_t d = BAYER[y & 3][x & 3];
        return c565(r + (d >> 1) - 4, g + (d >> 2) - 2, b + (d >> 1) - 4);
    }
}

/* ------------------------------------------------------------ the picture --- */
static void city_row(uint32_t row, uint8_t *out)
{
    const uint8_t *p = MT_RLE + MT_ROW[row];
    uint32_t x = 0;
    while (x < 240u) {
        uint32_t t = *p++, k;
        if (t < 128u) {
            uint8_t v = *p++;
            for (k = 0; k < t && x < 240u; k++)
                out[x++] = v;
        } else {
            for (k = 0; k < t - 128u && x < 240u; k++)
                out[x++] = *p++;
        }
    }
}

/* a barge, u from the bow (0) to the stern (len-1), r rows above the waterline; 0 = water */
static uint16_t barge_px(const barge_t *b, int32_t u, int32_t r)
{
    int32_t L = b->len;
    uint32_t n = city.nlev;
    static const uint8_t HULL[3] = {44, 40, 50}, STRAKE[3] = {150, 66, 54}, TARP[3] = {112, 98, 80}, TARP2[3] = {132, 116, 92},
                         CAB[3] = {206, 194, 172}, ROOF[3] = {48, 56, 80}, PANE[3] = {80, 92, 110};
    if (u < 0 || u >= L || r < 0)
        return 0;
    if (r <= 1)
        return (r == 0 && (u < 2 || u > L - 2)) ? 0 : tint565(HULL, n);
    if (r == 2)
        return u < 1 ? 0 : tint565(STRAKE, n);
    if (u >= L - 8 && u <= L - 3) {                                       /* the cabin at the stern */
        if (r == 7)
            return tint565(ROOF, n);
        if (r >= 3 && r <= 6) {
            if (r == 5 && (u == L - 6 || u == L - 5))
                return n >= 10u ? c565(250, 210, 140) : tint565(PANE, n);
            return tint565(CAB, n);
        }
    }
    if (r == 7 && u == L - 9)
        return tint565(ROOF, n);
    if (r == 3 && u >= 3 && u <= L - 10)
        return tint565(TARP, n);
    if (r == 4 && u >= 5 && u <= L - 12)
        return tint565(TARP2, n);
    if (u == L - 2 && (r == 8 || r == 9))                                 /* the little mast */
        return tint565(HULL, n);
    if (u == L - 2 && r == 10)
        return n >= 10u ? c565(255, 240, 210) : tint565(CAB, n);
    return 0;
}
static uint16_t barge_at(int32_t x, int32_t y, int *refl)
{
    uint32_t i;
    for (i = 0; i < 2u; i++) {
        const barge_t *b = &city.barge[i];
        int32_t x0, u, r;
        uint16_t c;
        if (!b->on)
            continue;
        x0 = b->x8 >> 8;
        if (x < x0 || x >= x0 + b->len || y < b->yb - 10 || y > b->yb + 4)
            continue;
        u = b->dir > 0 ? x0 + b->len - 1 - x : x - x0;
        r = b->yb - y;
        *refl = r < 0;
        if (r < 0)
            r = -r - 1;                                                   /* its reflection, under it */
        c = barge_px(b, u, r);
        if (c)
            return c;
    }
    return 0;
}

/* the mirror of the water below the quay: the quay, then (past the end of the shore) the sea horizon,
 * eased over 24 px so the reflection bends instead of breaking */
#define AX_A (MT_SHORE - 12)
#define AX_B (MT_SHORE + 12)
static inline int32_t axis_of(int32_t x)
{
    return x < AX_A ? MT_Q : x >= AX_B ? MT_HZ : MT_Q - (x - AX_A) * (MT_Q - MT_HZ) / (AX_B - AX_A);
}

/* decoded rows, a few kept (the reflection reads 1 to 10 source rows per water row) */
static const uint8_t *city_row_c(int32_t r)
{
    static uint8_t rc[12][240];
    static int16_t id[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    static uint8_t next;
    uint32_t i;
    r = clamp(r, 0, MT_Q);
    for (i = 0; i < 12u; i++)
        if (id[i] == r)
            return rc[i];
    i = next;
    next = (uint8_t)((next + 1u) % 12u);
    id[i] = (int16_t)r;
    city_row((uint32_t)r, rc[i]);
    return rc[i];
}

/* water from x0, w pixels of row y: the reflection of the picture above its axis, rippled.
 * far: the far sea in the stored rows (its mirror is the horizon) */
static void water_row(int32_t y, int32_t x0, int32_t w, uint16_t *o, int far)
{
    int32_t i, k, t = (int32_t)city.tick;
    int32_t calm = 3 - (city.rel >> 14);                                   /* REL calms the water: 3..1 px */
    uint16_t wc = c565(city.water[0], city.water[1], city.water[2]);
    const uint8_t *hs = city.sky[MT_HZ - 4];
    uint16_t swell = c565(hs[0], hs[1], hs[2]), ringc = c565(hs[0] + 30, hs[1] + 30, hs[2] + 30);
    int sun = city.sun_y < MT_HZ;
    uint16_t gl = sun ? c565(255, 236, 196) : c565(222, 228, 244);
    int32_t lx = sun ? city.sun_x : 54, light = sun ? 16 : city.moon_a;
    int32_t dens = light * (8 + (city.bril >> 10) + (city.lev >> 9)) / 16; /* glints / 256 */
    for (i = 0; i < w; i++) {
        int32_t x = x0 + i, ax = far ? MT_HZ : axis_of(x), d = y - ax, sy = 2 * ax + 1 - y, sx, amp, wob;
        int refl = 0;
        uint16_t c, bc;
        if (d <= 0) {
            o[i] = wc;
            continue;
        }
        amp = 1 + d * calm / 48;
        wob = (sine_i((uint32_t)y * (13u << 24) + (uint32_t)t * ((6u << 20) - ((uint32_t)city.rel << 5)) + (uint32_t)(x >> 5) * (2u << 24)) * amp) >> 15;
        sx = clamp(x + wob, 0, 239);
        if (sy < 0) {
            c = sky_px(sx, 0);
        } else {
            uint8_t v = city_row_c(sy)[sx];
            if (sy > MT_Q)
                sy = MT_Q;
            c = v == MT_I_SKY ? sky_px(sx, sy) : v == MT_I_WATER ? wc : city.pal[v];
        }
        c = mix565(c, wc, (uint32_t)clamp(5 + d / 9, 5, 12));
        if (((y * 3 + (t >> 4)) % 11) == 0 && sine_i((uint32_t)x * (3u << 24) + (uint32_t)y * (41u << 24) + (uint32_t)t * (1u << 20)) > 9000)
            c = mix565(c, swell, 3u);                                      /* broken lines of light on the swell */
        if (dens) {                                                        /* the path of light */
            int32_t half = 2 + d / 4, dx = x - lx - wob, adx = dx < 0 ? -dx : dx;
            if (adx < half && (int32_t)(hash3((uint32_t)x, (uint32_t)y, (uint32_t)t >> 3) & 255u) < dens * (half - adx) / half)
                c = mix565(c, gl, 11u);
        }
        if (city.nlev >= 12u) {                                            /* the lamps draw long streaks */
            int32_t L = 10 + (city.rel >> 11);
            if (d <= L && ((d + (t >> 4)) % 3) != 2)
                for (k = 0; k < MT_NLAMP; k++) {
                    int32_t px = MT_LAMPS[2 * k];
                    if (x - wob == px && (px < AX_A) == (ax == MT_Q))
                        c = mix565(c, c565(255, 214, 150), (uint32_t)(14 - d * 10 / L));
                }
        }
        if (city.isle > 24u && city.nlev >= 8u && x >= AX_B && d < 18 && x - wob == MT_ISLE_X - 3 && ((d + (t >> 4)) % 3) != 2)
            c = mix565(c, c565(255, 204, 128), (uint32_t)(12 - d * 2 / 3));   /* the island lantern */
        for (k = 0; k < 4; k++) {                                          /* the rings of the notes */
            const ring_t *g = &city.ring[k];
            int32_t dx, dy, d2;
            if (!g->t)
                continue;
            dx = x - g->x;
            dy = (y - g->y) * 3;
            d2 = dx * dx + dy * dy;
            if (d2 <= g->r * g->r && d2 >= (g->r - 1) * (g->r - 1))
                c = mix565(c, ringc, (uint32_t)clamp(9 - g->r / 2, 1, 9));
        }
        bc = barge_at(x, y, &refl);                                        /* the barges, and their reflection */
        if (bc)
            c = refl ? mix565(bc, wc, 9u) : bc;
        o[i] = c;
    }
}

/* ----------------------------------------------------------------- bands --- */
static int32_t rx, ry;
static void band_text(int32_t x, int32_t y, const char *s, uint16_t fg)
{
    x -= rx;
    y -= ry;
    for (; *s; s++) {
        uint32_t gi = glyph(&FONT_LAB, (uint8_t)*s), gx, gy, gw = FONT_LAB.bw[gi], bpr = (gw + 1u) / 2u;
        const uint8_t *gd = FONT_LAB.data + FONT_LAB.off[gi];
        for (gy = 0; gy < FONT_LAB.h; gy++)
            for (gx = 0; gx < gw; gx++) {
                int32_t px = x - FONT_LAB.pad + (int32_t)gx, py = y + (int32_t)gy;
                uint32_t v = gd[gy * bpr + gx / 2u];
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v && (uint32_t)px < cv_w && (uint32_t)py < cv_h) {
                    uint16_t *q = &cv_px[(uint32_t)py * cv_w + (uint32_t)px];
                    *q = swap16(mix565(swap16(*q), fg, v + (v >> 3)));
                }
            }
        x += FONT_LAB.adv[gi];
    }
}

static void bands_text(void)
{
    uint16_t fg = c565(232, 224, 206), hot = c565(255, 255, 255), dim = c565(176, 168, 156);
    uint32_t c;
    if (city.top_t && ry < BAND_TOP)
        band_text(6, 0, city.head, fg);
    if (city.bot_t && ry + (int32_t)cv_h > BAND_BOT) {
        if (city.hot < 4u && city.lab[city.hot][0]) {           /* the knob being turned, in the middle */
            int32_t lw = text_w(&FONT_LAB, city.lab[city.hot]), vw = text_w(&FONT_LAB, city.val[city.hot]), x0 = 120 - (lw + 16 + vw) / 2;
            band_text(x0, BAND_BOT + 1, city.lab[city.hot], dim);
            band_text(x0 + lw + 16, BAND_BOT + 1, city.val[city.hot], hot);
        } else {                                                 /* a new page: what the four knobs do */
            for (c = 0; c < 4u; c++)
                if (city.lab[c][0])
                    band_text(30 + (int32_t)c * 60 - text_w(&FONT_LAB, city.lab[c]) / 2, BAND_BOT + 1, city.lab[c], fg);
        }
    }
}

/* ---------------------------------------------------------------- render --- */
static void city_region(int32_t x, int32_t y, int32_t w, int32_t h)
{
    static uint8_t row[240];
    static uint16_t line[240];
    int32_t j, i;
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (y + h > CITY_H)
        h = CITY_H - y;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (x + w > 240)
        w = 240 - x;
    if (w <= 0 || h <= 0)
        return;
    while (h > 0) {                                        /* in bands that fit the canvas */
        int32_t bh = h > (int32_t)(CV_MAX / (uint32_t)w) ? (int32_t)(CV_MAX / (uint32_t)w) : h;
        lcd_sync();
        cv_w = (uint32_t)w;
        cv_h = (uint32_t)bh;
        rx = x;
        ry = y;
        for (j = 0; j < bh; j++) {
            int32_t yy = y + j;
            uint16_t *o = cv_px + (uint32_t)j * (uint32_t)w;
            if (yy > MT_Q) {
                water_row(yy, x, w, line, 0);
            } else {
                city_row((uint32_t)yy, row);
                for (i = 0; i < w; i++) {
                    uint8_t v = row[x + i];
                    if (v == MT_I_SKY) {
                        line[i] = sky_px(x + i, yy);
                    } else if (v == MT_I_WATER) {                  /* the far sea, right of the shore: one run */
                        int32_t e = i;
                        while (e < w && row[x + e] == MT_I_WATER)
                            e++;
                        water_row(yy, x + i, e - i, &line[i], 1);
                        i = e - 1;
                    } else {
                        line[i] = city.pal[v];
                    }
                }
            }
            if ((city.top_t && yy < BAND_TOP) || (city.bot_t && yy >= BAND_BOT))
                for (i = 0; i < w; i++)                            /* the quiet band: the picture, darker */
                    line[i] = (uint16_t)(((line[i] >> 1) & 0x7BEFu) - ((line[i] >> 3) & 0x18E3u));
            for (i = 0; i < w; i++)
                o[i] = swap16(line[i]);
        }
        bands_text();
        gb_quantize(cv_px, (uint32_t)w, (uint32_t)bh, (uint32_t)x, (uint32_t)y, GB_LUMA_CITY);   /* the handheld's four greens */
        lcd_blit((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)bh, cv_px);
        y += bh;
        h -= bh;
    }
}

static void dirty(int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (city.ndirty < sizeof city.dirty / sizeof city.dirty[0]) {
        crect_t *r = &city.dirty[city.ndirty++];
        r->x = (int16_t)x;
        r->y = (int16_t)y;
        r->w = (int16_t)w;
        r->h = (int16_t)h;
    }
}

/* ------------------------------------------------------- what the bands say --- */
static void city_values(void)
{
    uint32_t c, sig = 0;
    for (c = 0; c < 4u; c++) {
        int16_t *vp = 0;
        const param_desc_t *d = ui.home ? home_param(c, &vp) : page_desc(cur_page(), c, &vp);
        const char *unit = "";
        if (!d || !vp || !d->label[0] || d->label[0] == '-') {
            city.lab[c][0] = 0;
            city.val[c][0] = 0;
            continue;
        }
        str_cpy(city.lab[c], d->label, 6);
        param_format(d, *vp, city.val[c], &unit);
        if (str_len(city.val[c]) + str_len(unit) <= 5u)
            str_cpy(city.val[c] + str_len(city.val[c]), unit, 6);
        sig = sig * 31u + (uint32_t)*vp + 7u + c;
    }
    sig += (uint32_t)(ui.home ? 0xFFu : ui.page) * 977u + 1u;
    if (sig != city.val_sig) {
        if (city.val_sig)                                      /* (not on the first look) */
            city.bot_t = BAND_T;
        city.val_sig = sig;
        city.hot = ui.hot_t ? ui.hot_col : 0xFF;
        dirty(0, BAND_BOT, 240, CITY_H - BAND_BOT);
    }
}

static void city_head(void)
{
    char b[28], n[12];
    uint32_t sig;
    const engine_t *e = ENGINES[TSEL->eng_req];
    if (ui.msg_t) {
        str_cpy(b, ui.msg, sizeof b);
    } else {
        fmt_int(n, song.g[G_BPM]);
        str_cpy(b, n, sizeof b);
        str_cpy(b + str_len(b), " BPM   T", sizeof b - str_len(b));
        n[0] = (char)('1' + song.sel);
        n[1] = 0;
        str_cpy(b + str_len(b), n, sizeof b - str_len(b));
        str_cpy(b + str_len(b), "   ", sizeof b - str_len(b));
        str_cpy(b + str_len(b), TSEL->user ? "USER" : e->presets[TSEL->preset % e->npresets].name, sizeof b - str_len(b));
    }
    sig = str_hash(5u, b);
    if (sig != city.head_sig) {
        city.head_sig = sig;
        str_cpy(city.head, b, sizeof city.head);
        city.top_t = BAND_T;
        dirty(0, 0, 240, BAND_TOP);
    }
}

/* -------------------------------------------------------------- what drifts --- */
static void barge_start(barge_t *b, int far)
{
    b->on = 1;
    b->dir = (city_rand() & 1u) ? 1 : -1;
    b->len = far ? 18 : 28;
    b->yb = far ? (int16_t)(158 + city_rand() % 4u) : (int16_t)(176 + city_rand() % 14u);
    b->speed = far ? 14 : 24;                                   /* 1/256 px per frame: ~45 s (near) to 70 s across */
    b->x8 = (b->dir > 0 ? -b->len : 240) * 256;
    b->drawn = (int16_t)(b->x8 >> 8);
}

static void move_barges(void)
{
    uint32_t i;
    for (i = 0; i < 2u; i++) {
        barge_t *b = &city.barge[i];
        int32_t xi;
        if (!b->on) {
            if (city_rand() % 5400u == 0u)                      /* now and then */
                barge_start(b, (int)i);
            continue;
        }
        b->x8 += b->dir * b->speed;
        xi = b->x8 >> 8;
        if (xi != b->drawn) {
            int32_t a = xi < b->drawn ? xi : b->drawn;
            dirty(a - 1, b->yb - 11, b->len + 3, 16);
            b->drawn = (int16_t)xi;
        }
        if (xi < -b->len - 2 || xi > 242)
            b->on = 0;
    }
}

static void move_rings(void)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        ring_t *g = &city.ring[k];
        if (!g->t)
            continue;
        if (++g->t % 5u)
            continue;
        dirty(g->x - g->r - 2, g->y - g->r / 3 - 2, 2 * g->r + 5, 2 * g->r / 3 + 5);
        if (++g->r > 16) {
            dirty(g->x - g->r - 2, g->y - g->r / 3 - 2, 2 * g->r + 5, 2 * g->r / 3 + 5);
            g->t = 0;
        }
    }
    if (city.ring_gap)
        city.ring_gap--;
    if (!city.ring_gap && city.lev > city.lev_slow * 3 / 2 + 1200)   /* a note: the sound jumps up */
        for (k = 0; k < 4u; k++)
            if (!city.ring[k].t) {
                ring_t *g = &city.ring[k];
                g->x = (int16_t)(20 + city_rand() % 200u);
                g->y = (int16_t)(162 + city_rand() % 56u);
                g->r = 1;
                g->t = 1;
                city.ring_gap = 24;
                break;
            }
}

static void move_windows(void)
{
    uint32_t g, n = city.nlev;
    int32_t on = clamp(((int32_t)n - 7) * 256 / 18, 0, 256) * (90 + (city.sus >> 7)) / 346;   /* SUS keeps more lit */
    if (!(city.tick % 40u)) {                                   /* someone, somewhere, switches a light */
        g = city_rand() % MT_NWIN;
        city.jit[g] = (int8_t)((int32_t)(city_rand() % 81u) - 40);
    }
    for (g = 0; g < MT_NWIN; g++) {
        int32_t thr = MT_WIN_THR[g] + (MT_WIN_THR[g] ? city.jit[g] : 0);
        city.lit[g] = (uint8_t)(n >= 8u && thr < on);
    }
}

/* --------------------------------------------------------------- the entry --- */
static int city_wanted(void)
{
    const track_t *t = TSEL;
    uint32_t fam;
    if (t->eng_req != MT_ENGINE)
        return 0;
    if (ui.home)
        return 1;
    fam = cur_page()->fam;
    return (fam == FAM_EDIT && cur_page()->scope == SC_ENGINE) || (fam == FAM_ENV && cur_page()->graph == GR_ADSR);
}

static void city_draw(void)
{
    uint32_t k = (uint32_t)(TSEL - trk), nlev, i, full = 0, isle_t;
    int32_t pk = 0;
    if (!city.shown) {
        if (!city.rng) {
            city.rng = 81u;
            barge_start(&city.barge[0], 0);                     /* one barge already on its way */
            city.barge[0].x8 = (city.barge[0].dir > 0 ? 30 : 150) * 256;
            city.barge[0].drawn = (int16_t)(city.barge[0].x8 >> 8);
            city.isle = TSEL->p[P_E6] ? 32u : 0u;
        }
        city.shown = 1;
        city.head_sig = 0;
        city.val_sig = 0;
        full = 1;
    }
    if (ui.force)                                              /* back from a menu or a dialog */
        full = 1;
    city.tick++;
    /* the sound drives the picture */
    city.bril = TSEL->p[P_E0] * 258;
    city.mont = k < NPART ? mt_part[k].mont30 >> 15 : 0;
    city.sus = TSEL->p[P_SUS] * 258;
    city.rel = TSEL->p[P_REL] * 258;
    for (i = 0; i < NPART; i++) {                              /* how loud the parts are (as the TRACKS meter) */
        if (trk[i].peak > pk)
            pk = trk[i].peak;
        trk[i].peak = 0;
    }
    pk = clamp(pk, 0, 32767);
    city.lev = pk > city.lev ? pk : city.lev - city.lev / 12;
    city.lev_slow += (city.lev - city.lev_slow) / 24;
    isle_t = TSEL->p[P_E6] ? 32u : 0u;                         /* the island comes out of the haze in ~2 s */
    if (city.isle != isle_t && !(city.tick % 4u))
        city.isle = (uint8_t)(city.isle + (city.isle < isle_t ? 1 : -1));
    nlev = (uint32_t)(k < NPART ? mt_part[k].night30 >> 15 : 0) >> 10;
    if (nlev != city.nlev || full) {
        city.nlev = (uint8_t)nlev;
        city_sky_rows();
        city.settle = 40;                                      /* sweep the new light through quickly */
    }
    if (city.isle != isle_t)
        city.settle = 40;
    move_windows();
    city_palette();
    move_barges();                                             /* (a screen saver now: no bands) */
    move_rings();
    if (full) {
        city.ndirty = 0;
        city.top_t = city.bot_t = 0;
        city_region(0, 0, 240, CITY_H);
    } else {
        /* the rolling rebuild: the upper picture and the water, each on its cursor */
        uint32_t ru = city.settle ? 3u * ROLL_UP : ROLL_UP, rw = city.settle ? 2u * ROLL_WATER : ROLL_WATER;
        if (city.settle)
            city.settle--;
        if (city.ucur + ru > MT_Q + 1u)
            ru = MT_Q + 1u - city.ucur;
        if (city.wcur + rw > CITY_H - MT_Q - 1u)
            rw = CITY_H - MT_Q - 1u - city.wcur;
        city_region(0, city.ucur, 240, (int32_t)ru);
        city.ucur = (uint16_t)((city.ucur + ru) % (MT_Q + 1u));
        city_region(0, MT_Q + 1 + city.wcur, 240, (int32_t)rw);
        city.wcur = (uint16_t)((city.wcur + rw) % (CITY_H - MT_Q - 1u));
        for (i = 0; i < city.ndirty; i++)
            city_region(city.dirty[i].x, city.dirty[i].y, city.dirty[i].w, city.dirty[i].h);
    }
    city.ndirty = 0;
    if (city.top_t && !--city.top_t)
        dirty(0, 0, 240, BAND_TOP);
    if (city.bot_t && !--city.bot_t)
        dirty(0, BAND_BOT, 240, CITY_H - BAND_BOT);
    ui_tick();
}

/* leaving the town (another page or engine): Felucca redraws its own screen */
static void city_leave(void)
{
    if (city.shown) {
        city.shown = 0;
        ui.force = 1;
    }
}
