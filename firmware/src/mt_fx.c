/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Town effects, ported from the Modular Town V2 Cmajor patch:
 *   - BBD ping-pong delay (IndustrieSynth): stereo, the right line 1.38x the left,
 *     cross feedback; a time change glides (~75 ms) like a BBD clock instead of jumping
 *   - "80's" plate (IndustrieSynth): pre-delay, two input diffusers, 4 damped combs per
 *     side with slowly breathing lengths, 2 allpasses; L and R have their own lengths
 *   - the island (DreamFX) on the master: slow deep chorus, a cloud of two combs and an
 *     allpass, an octave-up shimmer of that cloud, a veil low-pass and a slow swell that
 *     turns attacks into pads; fades in over 3.5 s
 * The city (night, Montee, island) comes from the parts that play TOWN (mt_city_update).
 * Lines are int16 at -12 dB (x 1/4 in, x 4 out): the float patch goes up to ~3x inside its
 * combs. Every line has the length it needs (no power-of-two rounding): RAM is short. */

/* ---------------------------------------------------------------- city --- */
static int32_t mt_city_night, mt_city_mont;     /* Q15: the most of the TOWN parts */
static uint8_t mt_city_ile;                      /* a TOWN part has ILE on */

static void mt_city_update(void)
{
    uint32_t k;
    int32_t nn = 0, mm = 0;
    uint8_t ile = 0;
    for (k = 0; k < NPART; k++) {
        const track_t *t = &trk[k];
        if (t->engine != MT_ENGINE || t->p[P_MUTE])
            continue;
        if (mt_night(t) > nn)
            nn = mt_night(t);
        if (mt_mont(t) > mm)
            mm = mt_mont(t);
        if (t->p[P_E6])
            ile = 1;
    }
    mt_city_night = nn;
    mt_city_mont = mm;
    mt_city_ile = ile;
}

static inline int16_t mt_sat16(int32_t x) { return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }
/* Q15 product rounded towards zero: in a feedback loop a floor (>> 15) keeps a small negative
 * value alive for ever (x = floor(0.9 x) at x = -10), a hum at the loop frequency after the sound */
static inline int32_t mt_tz(int32_t x, uint32_t n) { return (x + ((x >> 31) & ((1 << n) - 1))) >> n; }   /* x >> n towards zero */
/* one-pole low-pass with a Q23 state (8 bits below the signal): a Q15 state stalls up to
 * 32768 / k LSB short of its input, and inside a feedback loop that error never dies out.
 * Returns the state towards zero, so a silent input reaches exactly 0. */
static inline int32_t mt_lp23(int32_t *st, int32_t x, int32_t k)
{
    int32_t e = x * 256 - *st;
    *st += (e >> 15) * k + (((e & 32767) * k) >> 15);
    return mt_tz(*st, 8);
}
static inline int32_t mt_half(int32_t x) { return (x + ((x >> 31) & 1)) >> 1; }   /* x / 2 towards zero */
static inline int32_t mt_mulz(int32_t a, int32_t b)
{
    int32_t p = a * b;
    return (p + ((p >> 31) & 32767)) >> 15;
}

/* -------------------------------------------------------------- delay --- */
#define MT_DLEN_L 24576u                 /* 0.557 s: the right line is 1.38x longer */
#define MT_DLEN_R 32768u                 /* 0.743 s, as the Cmajor buffer */
static int16_t mt_dl[MT_DLEN_L] __attribute__((section(".pool")));
static int16_t mt_dr[MT_DLEN_R] __attribute__((section(".pool")));
static struct {
    uint32_t wl, wr;
    int32_t tl, tr;                      /* glided read offsets, Q16 samples */
    int32_t lpl, lpr;                    /* COLR low-pass in the feedback, Q23 */
    uint32_t quiet;                      /* samples with no input and a silent output */
} mt_d = {0, 0, 4000 << 16, 5520 << 16, 0, 0, 0};

/* fractional read d (Q16) samples behind w in a line of len samples */
static inline int32_t mt_tap(const int16_t *b, uint32_t len, uint32_t w, int32_t d)
{
    int32_t i0 = (int32_t)w - (d >> 16), f = (d & 0xFFFF) >> 1, i1;
    if (i0 < 0)
        i0 += (int32_t)len;
    i1 = i0 - 1;
    if (i1 < 0)
        i1 += (int32_t)len;
    return b[i0] + mt_tz((b[i1] - b[i0]) * f, 15);
}

/* ------------------------------------------------------------- plate --- */
static const uint16_t MT_CL[4] = {1557, 1617, 1491, 1422}, MT_CR[4] = {1499, 1663, 1433, 1381};
#define MT_CPAD 16u                      /* + the breathing (6 + 6 samples) and the interpolation */
#define MT_COMB_L (1557 + 1617 + 1491 + 1422 + 4 * MT_CPAD)
#define MT_COMB_R (1499 + 1663 + 1433 + 1381 + 4 * MT_CPAD)
static int16_t mt_comb[MT_COMB_L + MT_COMB_R] __attribute__((section(".pool")));
static int16_t mt_ap[225 + 191 + 341 + 379] __attribute__((section(".pool")));
#define MT_PRE 1792u                     /* pre-delay up to 40 ms */
static int16_t mt_pre[MT_PRE] __attribute__((section(".pool")));
static int16_t mt_dif[142 + 107] __attribute__((section(".pool")));
static struct {
    uint16_t cw[8], aw[4], pw, dw[2];
    int32_t clp[8];                      /* comb low-passes, Q23 */
    int32_t hp, lp;                      /* input filters, Q23 */
    uint32_t mp[4];                      /* comb breathing phases */
} mt_r;

static void mt_delay_reverb(const int32_t *dly_in, const int32_t *rev_in, int32_t *wl, int32_t *wr, uint32_t n)
{
    static const uint32_t MP_INC[4] = {(uint32_t)(0.43 * 4294967296.0 / FS), (uint32_t)(0.57 * 4294967296.0 / FS),
                                       (uint32_t)(0.71 * 4294967296.0 / FS), (uint32_t)(0.37 * 4294967296.0 / FS)};
    int32_t night = mt_city_night;
    /* delay: time from the tempo division (TIME), the right line 1.38x; FDBK + 0.18 at night, x 0.78 */
    int32_t tl = (int32_t)delay_samples(), trr;
    int32_t fb = mulq15(clamp(song.g[G_DFDBK] * 258 + mulq15(night, 5898), 0, 32767), 25559);
    int32_t col = 2000 + song.g[G_DCOLOR] * 240, dmix = song.g[G_DMIX] * 258;
    /* plate: SIZE (+ 0.22 at night) sets the decay, DAMP the comb low-pass (70 = the patch) */
    int32_t size = song.g[G_RSIZE] * 258;
    int32_t rfb = 22938 + mulq15(clamp(size + mulq15(night, 7209), 0, 32767), 8847);   /* 0.70 + 0.27 s */
    int32_t damp = 8192 + (127 - song.g[G_RDAMP]) * 155;                              /* 0.25 .. 0.85 */
    uint32_t pd = (uint32_t)(970 + ((size * 794) >> 15)), i, k, cl[8];                  /* 22 .. 40 ms */
    int32_t d0[8], dd[8];
    /* asleep: no input and nothing out for longer than the longest line, so every line holds
     * zeros (the loops round towards zero): skip the work, bit-exact */
    {
        uint32_t quiet = 1;
        for (i = 0; i < n && quiet; i++)
            quiet = !dly_in[i] && !rev_in[i];
        if (quiet && mt_d.quiet > MT_DLEN_R + MT_PRE + 4096u) {
            for (i = 0; i < n; i++)
                wl[i] = wr[i] = 0;
            return;
        }
        if (!quiet)
            mt_d.quiet = 0;
    }
    if (tl > (int32_t)MT_DLEN_L - 64)
        tl = (int32_t)MT_DLEN_L - 64;
    trr = tl + mulq15(tl, 12452);                                                       /* x 1.38 */
    if (trr > 32700)
        trr = 32700;
    /* comb lengths: base + 6 + 6 sin (R: cos), from this block's start to its end */
    for (k = 0; k < 8u; k++) {
        uint32_t j = k & 3u, q = k < 4u ? 0u : 0x40000000u;
        cl[k] = k < 4u ? MT_CL[j] : MT_CR[j];
        d0[k] = ((int32_t)cl[k] << 16) + (6 << 16) + sine_i(mt_r.mp[j] + q) * 12;
        dd[k] = ((int32_t)cl[k] << 16) + (6 << 16) + sine_i(mt_r.mp[j] + q + MP_INC[j] * n) * 12 - d0[k];
    }
    for (k = 0; k < 4u; k++)
        mt_r.mp[k] += MP_INC[k] * n;
    for (i = 0; i < n; i++) {
        int32_t x, ol, or_, sl = 0, sr = 0, a1l, a1r, a2l, a2r;
        /* ── delay ── */
        {
            int32_t d = (tl << 16) - mt_d.tl;
            mt_d.tl += (d >> 12) + (d >> 14);
            d = (trr << 16) - mt_d.tr;
            mt_d.tr += (d >> 12) + (d >> 14);
        }
        ol = mt_tap(mt_dl, MT_DLEN_L, mt_d.wl, mt_d.tl);
        or_ = mt_tap(mt_dr, MT_DLEN_R, mt_d.wr, mt_d.tr);
        {
            int32_t fl = mt_lp23(&mt_d.lpl, ol, col), fr = mt_lp23(&mt_d.lpr, or_, col);
            mt_dl[mt_d.wl] = mt_sat16((dly_in[i] >> 3) + mt_mulz(fr, fb));   /* ping-pong: each side feeds the other */
            mt_dr[mt_d.wr] = mt_sat16((dly_in[i] >> 3) + mt_mulz(fl, fb));   /* (Felucca's /2, -12 dB of room) */
        }
        if (++mt_d.wl >= MT_DLEN_L)
            mt_d.wl = 0;
        if (++mt_d.wr >= MT_DLEN_R)
            mt_d.wr = 0;
        wl[i] = mulq15(ol, dmix) * 15;                          /* x 4 room, x 2 Felucca, x 1.9: the patch's balance */
        wr[i] = mulq15(or_, dmix) * 15;

        /* ── plate: input high-pass 170 Hz, low-pass 8.5 kHz, pre-delay, two diffusers ── */
        x = mulq15(rev_in[i], 2580);                              /* Felucca's send level, -6 dB of room */
        {
            int32_t e = x * 256 - mt_r.hp;
            mt_r.hp += mt_tz(e, 6) + mt_tz(e, 7);
            x -= mt_tz(mt_r.hp, 8);
        }
        x = mt_lp23(&mt_r.lp, x, 23010);
        {
            int32_t rp = (int32_t)mt_r.pw - (int32_t)pd;
            if (rp < 0)
                rp += MT_PRE;
            mt_pre[mt_r.pw] = mt_sat16(x);
            x = mt_pre[rp];
            if (++mt_r.pw >= MT_PRE)
                mt_r.pw = 0;
        }
        {
            int16_t *d1 = mt_dif, *d2 = mt_dif + 142;
            int32_t b1 = d1[mt_r.dw[0]], y1 = b1 - mt_mulz(20316, x), b2, y2;   /* g = 0.62 */
            d1[mt_r.dw[0]] = mt_sat16(x + mt_mulz(20316, y1));
            if (++mt_r.dw[0] >= 142u)
                mt_r.dw[0] = 0;
            b2 = d2[mt_r.dw[1]];
            y2 = b2 - mt_mulz(20316, y1);
            d2[mt_r.dw[1]] = mt_sat16(y1 + mt_mulz(20316, y2));
            if (++mt_r.dw[1] >= 107u)
                mt_r.dw[1] = 0;
            x = y2;
        }
        /* ── 4 combs per side, lengths breathing +-6 samples at 0.37 .. 0.71 Hz ── */
        {
            int16_t *cb = mt_comb;
            for (k = 0; k < 8u; k++) {
                uint32_t len = cl[k] + MT_CPAD;
                int32_t c = mt_tap(cb, len, mt_r.cw[k], d0[k] + ((dd[k] * (int32_t)i) >> CTL_LOG2));
                cb[mt_r.cw[k]] = mt_sat16(x + mt_mulz(mt_lp23(&mt_r.clp[k], c, damp), rfb));
                if (++mt_r.cw[k] >= len)
                    mt_r.cw[k] = 0;
                if (k < 4u)
                    sl += c;
                else
                    sr += c;
                cb += len;
            }
        }
        sl >>= 2;
        sr >>= 2;
        /* ── 2 allpasses, buffer - in, feedback 0.5 ── */
        {
            int16_t *a = mt_ap;
            int32_t bl = a[mt_r.aw[0]], br = a[225 + mt_r.aw[1]];
            a1l = bl - sl;
            a1r = br - sr;
            a[mt_r.aw[0]] = mt_sat16(sl + mt_half(bl));
            a[225 + mt_r.aw[1]] = mt_sat16(sr + mt_half(br));
            if (++mt_r.aw[0] >= 225u)
                mt_r.aw[0] = 0;
            if (++mt_r.aw[1] >= 191u)
                mt_r.aw[1] = 0;
            a += 225 + 191;
            bl = a[mt_r.aw[2]];
            br = a[341 + mt_r.aw[3]];
            a2l = bl - a1l;
            a2r = br - a1r;
            a[mt_r.aw[2]] = mt_sat16(a1l + mt_half(bl));
            a[341 + mt_r.aw[3]] = mt_sat16(a1r + mt_half(br));
            if (++mt_r.aw[2] >= 341u)
                mt_r.aw[2] = 0;
            if (++mt_r.aw[3] >= 379u)
                mt_r.aw[3] = 0;
        }
        wl[i] += a2l * 14;                                        /* x 2 room, x 7: the plate as loud as in the patch */
        wr[i] += a2r * 14;
        if (wl[i] || wr[i] || dly_in[i] || rev_in[i])
            mt_d.quiet = 0;
        else
            mt_d.quiet++;
    }
}

/* ---------------------------------------------------------- the island --- */
#define MT_SH 2048u                      /* chorus: 22 +- 14 ms */
#define MT_PSG 5292                      /* shimmer grain: 120 ms */
#define MT_PS 5300u
static int16_t mt_shl[MT_SH] __attribute__((section(".pool")));
static int16_t mt_shr[MT_SH] __attribute__((section(".pool")));
static int16_t mt_dc[2 * (2731 + 2999)] __attribute__((section(".pool")));
static int16_t mt_dap[2 * 441] __attribute__((section(".pool")));
static int16_t mt_psl[MT_PS] __attribute__((section(".pool")));
static int16_t mt_psr[MT_PS] __attribute__((section(".pool")));
static struct {
    int32_t amt;                         /* Q24: 0 = dry, 1 = on the island */
    uint32_t sw, shpl, shpr;
    uint16_t cw[4], aw[2], pw;
    int32_t g1l, g1r;                    /* shimmer grain read offsets */
    int32_t lpl, lpr, psl, psr;
    int32_t peak, swell;                 /* Q28 */
    uint8_t idle;
} mt_i = {0, 0, 0, (uint32_t)(0.37 * 4294967296.0), {0}, {0}, 0, 1, MT_PSG / 2, 0, 0, 0, 0, 0, 0, 1};

static void mt_island_clear(void)
{
    uint32_t i;
    for (i = 0; i < MT_SH; i++)
        mt_shl[i] = mt_shr[i] = 0;
    for (i = 0; i < sizeof mt_dc / 2; i++)
        mt_dc[i] = 0;
    for (i = 0; i < sizeof mt_dap / 2; i++)
        mt_dap[i] = 0;
    for (i = 0; i < MT_PS; i++)
        mt_psl[i] = mt_psr[i] = 0;
    mt_i.lpl = mt_i.lpr = mt_i.psl = mt_i.psr = mt_i.peak = mt_i.swell = 0;
}

static inline int32_t mt_ps_tap(const int16_t *b, int32_t d)
{
    int32_t j = (int32_t)mt_i.pw - d;
    if (j < 0)
        j += MT_PS;
    return b[j];
}

/* the island on the master mix (l, r: the Q15 mix before the master volume) */
static void mt_island(int32_t *l, int32_t *r, uint32_t n)
{
    static const uint32_t SH_L = (uint32_t)(0.17 * 4294967296.0 / FS), SH_R = (uint32_t)(0.13 * 4294967296.0 / FS);
    static const uint32_t W_INC = (uint32_t)(4294967296.0 / (2.0 * MT_PSG));
    int32_t tgt = mt_city_ile ? 1 << 24 : 0, inc = 108, mont = mt_city_mont;   /* 3.5 s: 2^24 / (3.5 FS) */
    int32_t pad, da, sm, cfb, cfb2, dlp;
    uint32_t i;
    if (!mt_i.amt && !tgt) {
        if (!mt_i.idle) {
            mt_island_clear();
            mt_i.idle = 1;
        }
        return;
    }
    mt_i.idle = 0;
    pad = mt_i.swell >= mt_i.peak ? 32767 : mt_i.peak > (26 << 13) ? (int32_t)(((int64_t)mt_i.swell << 15) / mt_i.peak) : 32767;
    for (i = 0; i < n; i++) {
        int32_t x0l = l[i], x0r = r[i], dl = clamp(x0l, -65535, 65535), dr = clamp(x0r, -65535, 65535);   /* mulq15 range */
        int32_t e, sl, sr, c1l, c2l, c1r, c2r, csl, csr, bl, br, inl, inr, aol, aor;
        int32_t up_l, up_r, d2l, d2r, wl, wr;
        if (mt_i.amt < tgt)
            mt_i.amt = mt_i.amt + inc > tgt ? tgt : mt_i.amt + inc;
        else if (mt_i.amt > tgt)
            mt_i.amt = mt_i.amt - inc < tgt ? tgt : mt_i.amt - inc;
        da = mt_i.amt >> 9;                                          /* Q15 */
        /* pad-ifier: a slow follower of a fast peak follower; attacks swell in */
        e = clamp(dl < 0 ? -dl : dl, 0, 131071);
        {
            int32_t er = clamp(dr < 0 ? -dr : dr, 0, 131071), d;
            if (er > e)
                e = er;
            d = (e << 13) - mt_i.peak;
            mt_i.peak += d > 0 ? d >> 7 : d >> 13;                   /* ~3 ms / ~190 ms */
            mt_i.swell += (mt_i.peak - mt_i.swell) >> 14;           /* ~370 ms */
        }
        /* slow deep chorus, 22 +- 14 ms, L 0.17 Hz / R 0.13 Hz */
        mt_i.shpl += SH_L;
        mt_i.shpr += SH_R;
        mt_shl[mt_i.sw] = mt_sat16(dl);
        mt_shr[mt_i.sw] = mt_sat16(dr);
        {
            int32_t ql = (970 << 16) + sine_i(mt_i.shpl) * 1234, qr = (970 << 16) + sine_i(mt_i.shpr) * 1234;
            int32_t ol = mt_tap(mt_shl, MT_SH, mt_i.sw, ql), or_ = mt_tap(mt_shr, MT_SH, mt_i.sw, qr);
            sl = (dl + mulq15(ol - dl, 19661)) >> 2;                 /* 60 % wet, -12 dB of room */
            sr = (dr + mulq15(or_ - dr, 19661)) >> 2;
        }
        if (++mt_i.sw >= MT_SH)
            mt_i.sw = 0;
        /* reverb cloud: two combs (longer with the Montee), one allpass */
        cfb = 25559 + mulq15(da, 2949 + mulq15(1311, mont));
        cfb2 = cfb - 1638;
        {
            int16_t *cl1 = mt_dc, *cl2 = mt_dc + 2731, *cr1 = mt_dc + 2731 + 2999, *cr2 = cr1 + 2731;
            c1l = cl1[mt_i.cw[0]];
            cl1[mt_i.cw[0]] = mt_sat16(sl + mt_mulz(c1l, cfb));
            c2l = cl2[mt_i.cw[1]];
            cl2[mt_i.cw[1]] = mt_sat16(sl + mt_mulz(c2l, cfb2));
            c1r = cr1[mt_i.cw[0]];
            cr1[mt_i.cw[0]] = mt_sat16(sr + mt_mulz(c1r, cfb));
            c2r = cr2[mt_i.cw[1]];
            cr2[mt_i.cw[1]] = mt_sat16(sr + mt_mulz(c2r, cfb2));
            if (++mt_i.cw[0] >= 2731u)
                mt_i.cw[0] = 0;
            if (++mt_i.cw[1] >= 2999u)
                mt_i.cw[1] = 0;
        }
        csl = (c1l + c2l) >> 1;
        csr = (c1r + c2r) >> 1;
        bl = mt_dap[mt_i.aw[0]];
        inl = csl + mt_half(bl);
        aol = bl - mt_half(inl);
        mt_dap[mt_i.aw[0]] = mt_sat16(inl);
        br = mt_dap[441 + mt_i.aw[0]];
        inr = csr + mt_half(br);
        aor = br - mt_half(inr);
        mt_dap[441 + mt_i.aw[0]] = mt_sat16(inr);
        if (++mt_i.aw[0] >= 441u)
            mt_i.aw[0] = 0;
        /* octave-up shimmer of the cloud: two crossfaded grains read at double speed */
        mt_psl[mt_i.pw] = mt_sat16(aol);
        mt_psr[mt_i.pw] = mt_sat16(aor);
        if (++mt_i.pw >= MT_PS)
            mt_i.pw = 0;
        if (--mt_i.g1l <= 0)
            mt_i.g1l += MT_PSG;
        if (--mt_i.g1r <= 0)
            mt_i.g1r += MT_PSG;
        d2l = mt_i.g1l + MT_PSG / 2;
        if (d2l > MT_PSG)
            d2l -= MT_PSG;
        d2r = mt_i.g1r + MT_PSG / 2;
        if (d2r > MT_PSG)
            d2r -= MT_PSG;
        up_l = mulq15(mt_ps_tap(mt_psl, mt_i.g1l), sine_i((uint32_t)mt_i.g1l * W_INC))
             + mulq15(mt_ps_tap(mt_psl, d2l), sine_i((uint32_t)d2l * W_INC));
        up_r = mulq15(mt_ps_tap(mt_psr, mt_i.g1r), sine_i((uint32_t)mt_i.g1r * W_INC))
             + mulq15(mt_ps_tap(mt_psr, d2r), sine_i((uint32_t)d2r * W_INC));
        mt_i.psl += mulq15(up_l - mt_i.psl, 10663);                  /* 2.6 kHz */
        mt_i.psr += mulq15(up_r - mt_i.psr, 10663);
        /* veil: 3.4 kHz closing to 1.3 kHz as the island comes */
        dlp = mt_lerp(MT_DLP_Q15, da >> 3, 16);
        mt_i.lpl += mulq15(aol - mt_i.lpl, dlp);
        mt_i.lpr += mulq15(aor - mt_i.lpr, dlp);
        /* mix: dry 0.65 + veil 0.85 + shimmer (0.45 + 0.25 Montee), the swell, soft clip */
        sm = mulq15(14746 + mulq15(8192, mont), da);
        {
            int32_t sw = 32767 - mulq15(da, 32767 - pad);
            int32_t xl = mulq15(dl, 21299) + (mulq15(mt_i.lpl, 27853) << 2) + (mulq15(mt_i.psl, sm) << 2);
            int32_t xr = mulq15(dr, 21299) + (mulq15(mt_i.lpr, 27853) << 2) + (mulq15(mt_i.psr, sm) << 2);
            wl = softclip((int32_t)(((int64_t)xl * sw) >> 15));
            wr = softclip((int32_t)(((int64_t)xr * sw) >> 15));
        }
        l[i] = x0l + (int32_t)(((int64_t)(wl - x0l) * da) >> 15);
        r[i] = x0r + (int32_t)(((int64_t)(wr - x0r) * da) >> 15);
    }
}
