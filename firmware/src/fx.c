/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Effects: per-track DIST insert, then sends into three
 * shared buses (chorus, tempo delay, reverb), stereo dry mix.
 * Modular Town FM: the delay and the reverb are the Modular Town ones (mt_fx.c,
 * stereo), and the island (Dream Mode) sits on the master. The chorus is Felucca's. */
#define DLY_LEN 32768u           /* longest delay (the right line of mt_fx.c) */
#define CHO_LEN 2048u
static int16_t cho_buf[CHO_LEN] __attribute__((section(".pool")));
static struct {
    uint32_t cho_w, cho_ph;
} fx;

/* DIST: low cut -> drive (1x..8x, exponential) -> asymmetric soft clip
 * (a little bias = even harmonics) -> tone low-pass that closes with drive ->
 * make-up gain. State per part (track_t dist_*). */
static void track_dist(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0;
    if (!d)
        return;                                         /* states kept: switching on does not click */
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = 32000 - d * 95;                                  /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = 30000 - d * 120;                                /* make-up */
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;           /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - t->dist_hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;   /* >> 2 first: no overflow for loud poly */
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);         /* two poles: tames the fizz */
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}

/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
static volatile uint8_t fx_lowcut;     /* settings: 12 dB/oct ~110 Hz for the small speaker */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (It was rounded, and a rounded step of (x - dc) / 4096 stops moving at |x - dc| < 2048: a
 * constant offset of up to +-31 stayed at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> 6;
    *err = e - (d << 6);
    *lc += d;
    return x - *lc;
}

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {                  /* two one-pole high-passes, error feedback as dc_block (the */
        *l = lowcut1(*l, &lc_l1, &lce[0]);          /* rounded step stopped at |x - lc| < 32: an offset) */
        *l = lowcut1(*l, &lc_l2, &lce[1]);
        *r = lowcut1(*r, &lc_r1, &lce[2]);
        *r = lowcut1(*r, &lc_r2, &lce[3]);
    }
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 12) + 1;
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* beats = 1 / DEN */
static uint32_t div_samples(uint32_t div)
{
    return (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM] / DIV_DEN[div % 6u];
}

static uint32_t delay_samples(void)
{
    uint32_t s = div_samples((uint32_t)song.g[G_DTIME]);
    return s < 16u ? 16u : s >= DLY_LEN ? DLY_LEN - 1u : s;
}

#include "mt_fx.c"               /* Modular Town: ping-pong delay, plate, the island */

/* process the three buses for one block; sends in, stereo wet out */
static void fx_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wl, int32_t *wr,
                     uint32_t n)
{
    uint32_t i;
    int32_t cdepth = song.g[G_CDEPTH] * 6;
    uint32_t cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    mt_delay_reverb(dly_in, rev_in, wl, wr, n);
    for (i = 0; i < n; i++) {
        int32_t r, y;
        /* chorus: modulated short delay, 5..15 ms */
        cho_buf[fx.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
        fx.cho_ph += cinc;
        r = (400 << 8) + ((osc_sine(fx.cho_ph) + 32768) * cdepth >> 8);   /* Q8 delay: read between samples */
        {
            uint32_t ri = (uint32_t)r >> 8;
            int32_t f = r & 255, c0 = cho_buf[(fx.cho_w - ri) & (CHO_LEN - 1u)];
            int32_t c1 = cho_buf[(fx.cho_w - ri - 1u) & (CHO_LEN - 1u)];
            y = (c0 + (((c1 - c0) * f) >> 8)) << 1;
        }
        fx.cho_w++;
        wl[i] += y;
        wr[i] += y;
    }
}

/* one block of the whole mix (shared with tests/hostsim.c): events -> each part
 * -> dist -> SLICER -> level / pan / sends -> buses -> master; out: stereo Q15 */
static void events_block(uint32_t n);                    /* seq.c */
static int32_t send_c[CTL], send_d[CTL], send_r[CTL], wet_l[CTL], wet_r[CTL], mix_l[CTL], mix_r[CTL], part_buf[CTL];

/* one synth part into the dry mix and the sends; a part with no voice sounding costs
 * the LFO tick and a cleared buffer only (after the DIST tail has run out) */
static void mix_part(track_t *t, uint32_t n)
{
    int32_t *b = part_buf;
    uint32_t i;
    if (track_render(t, b, n))
        t->tail = 16;                                   /* blocks of DIST state to run out after the last voice */
    else if ((!t->tail || !t->p[P_DIST] || !--t->tail) && !slicer_busy(t)) {
        slicer_track(t, 0, n);                          /* (the SLICER's step clock runs on) */
        return;
    }
    {
        int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127], pan = t->p[P_PAN];
        int32_t gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
        int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        if (t->engine == MT_ENGINE) {                   /* TOWN: the Montee (+0.35) and the night (+0.15) open the reverb */
            if (r)
                r += mulq15(mt_night(t), 4915);
            r = clamp(r + mulq15(mt_mont(t), 11469), 0, 32767);
        }
        int32_t xmax = c > d ? c : d;
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        track_dist(t, b, n);
        slicer_track(t, b, n);                          /* slicer.c: before the level, pan and sends */
        for (i = 0; i < n; i++) {
            int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (c)
                send_c[i] += mulq15(xs, c);
            if (d)
                send_d[i] += mulq15(xs, d);
            if (r)
                send_r[i] += mulq15(xs, r);
            mix_l[i] += (x * gl) >> 12;
            mix_r[i] += (x * gr) >> 12;
        }
        t->peak = pk;
    }
}

/* the start-up chime (the opening, ui_draw.c): two square notes, C6 then C7 ringing down, before the
 * master volume. The UI sets m1_chime_req; the rest is integer, in the audio interrupt. */
static volatile uint8_t m1_chime_req;
static uint32_t m1_chime_n, m1_chime_ph;
static int32_t m1_chime_amp;
static void m1_chime(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    if (m1_chime_req) {
        m1_chime_req = 0;
        m1_chime_n = 1;
        m1_chime_ph = 0;
        m1_chime_amp = 2400 << 8;                        /* Q8 */
    }
    for (i = 0; i < n && m1_chime_n; i++) {
        int32_t w = m1_chime_ph < 0x80000000u ? m1_chime_amp >> 8 : -(m1_chime_amp >> 8);
        if (m1_chime_n == 3500u)                         /* 80 ms: the second, higher note */
            m1_chime_amp = 2400 << 8;
        if (m1_chime_n >= 3500u)
            m1_chime_amp -= (m1_chime_amp >> 13) + 1;    /* rings down, ~0.2 s per 8.7 dB */
        m1_chime_ph += m1_chime_n < 3500u ? 101920450u : 203840998u;   /* C6, C7 */
        l[i] += w;
        r[i] += w;
        if (++m1_chime_n > 3500u && m1_chime_amp < (2 << 8))
            m1_chime_n = 0;
    }
}

static void mix_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
    events_block(n);
    for (i = 0; i < NPART; i++)
        mix_part(&trk[i], n);
    mt_city_update();                                   /* night, Montee, island of the TOWN parts */
    fx_buses(send_c, send_d, send_r, wet_l, wet_r, n);
    for (i = 0; i < n; i++) {
        mix_l[i] = (mix_l[i] + wet_l[i]) >> 2;
        mix_r[i] = (mix_r[i] + wet_r[i]) >> 2;
    }
    mt_island(mix_l, mix_r, n);                         /* the island, before the master volume */
    if (m1_chime_n || m1_chime_req)
        m1_chime(mix_l, mix_r, n);
    for (i = 0; i < n; i++) {
        int32_t l = (mix_l[i] * (int32_t)song.master_q12) >> 10;
        int32_t r = (mix_r[i] * (int32_t)song.master_q12) >> 10;
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
