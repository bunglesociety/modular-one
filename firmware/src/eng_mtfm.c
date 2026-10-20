/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* TOWN: the FM voice of Modular Town (the winter engine of the Modular Town V2 Cmajor
 * patch, IndustrieSynth), for film music. Per voice: a carrier, modulator 1 with
 * feedback, modulator 2 (detuned for bells), then a soft one-pole low-pass.
 *   BRIL  brightness: the FM index (was Cutoff), with its level tracking
 *   GRAIN feedback of modulator 1 (was Resonance)
 *   TIMB  ratios of the two modulators, in notches
 *   CLOCH detune of modulator 2, +-12 semitones: inharmonic, bells
 *   MONT  "Montee": opens the index (up to x2), a 5.2 Hz vibrato (up to 20 cents),
 *         more reverb (mix_part) and, on the island, a longer cloud (mt_fx.c)
 *   NUIT  night: darker, softer FM (less index and feedback, the low-pass closes)
 *   ILE   the island: Dream Mode on the master (mt_fx.c)
 * The amplitude is Felucca's ADSR (ENV page: ATK DEC SUS REL) and velocity.
 * Everything is integer: the Cmajor curves are sampled in mt_tables.h. */
#include "mt_tables.h"

#define MT_ENGINE 9u                 /* index of ENG_TOWN in ENGINES (engines.c) */

/* TIMB: twelve FM ratio pairs of the two modulators */
#define MT_NTIMB 12u
static const char *const N_MTTIMB[MT_NTIMB] = {"1:1", "1:2", "1:3", "2:1", "2:3", "3:1",
                                              "1:.5", ".5:1", "1:3.5", "2:5", "1:5", "1:7"};
static const uint16_t MT_R1_Q8[12] = {256, 256, 256, 512, 512, 768, 256, 128, 256, 512, 256, 256};
static const uint16_t MT_R2_Q8[12] = {256, 512, 768, 256, 768, 256, 128, 256, 896, 1280, 1280, 1792};

/* per part: night and Montee glide once per block (mt_block), the vibrato phase */
static struct {
    int32_t night30, mont30;         /* Q30 */
    uint32_t vph;
} mt_part[NPART];
#define MT_VIB_INC 16205953u       /* 5.2 Hz per control block: 5.2 * CTL / FS * 2^32 (no float on the FM-1) */

static inline int32_t mt_night(const track_t *t) { return mt_part[t - trk].night30 >> 15; }   /* Q15 */
static inline int32_t mt_mont(const track_t *t) { return mt_part[t - trk].mont30 >> 15; }

static inline int32_t mt_lerp(const uint16_t *tab, int32_t x, uint32_t last)   /* x: index << 8 */
{
    uint32_t i = (uint32_t)x >> 8;
    int32_t f = x & 255;
    if (i >= last)
        return tab[last];
    return tab[i] + (((tab[i + 1] - tab[i]) * f) >> 8);
}

static void mt_block(track_t *t)
{
    uint32_t k = (uint32_t)(t - trk);
    if (k >= NPART)
        return;
    /* night ~1.5 s (Cmajor: 1.2 s), Montee ~180 ms (prototype: 150 ms) */
    mt_part[k].night30 += (((int32_t)t->p[P_E5] * 258 << 15) - mt_part[k].night30) >> 11;
    mt_part[k].mont30 += (((int32_t)t->p[P_E4] * 258 << 15) - mt_part[k].mont30) >> 8;
    mt_part[k].vph += MT_VIB_INC;
}

static void mt_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;   /* carrier, modulator 1, modulator 2 */
    v->s[0] = 0;                          /* modulator 1 feedback */
    v->s[1] = 0;                          /* low-pass */
    v->s[5] = 0;                          /* ramps not started */
    v->s[6] = 0;                          /* the SUB's phase */
}

static void mt_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t night = mt_night(t), mont = mt_mont(t);
    /* BRIL follows ENV > FLT and LFO > FLT (as a filter would), GRAIN follows the SHP routes */
    int32_t cut = clamp(((((int32_t)p[P_E0] << 8) + m->cutoff) * 129 >> 7) - mulq15(night, 5243), 0, 32767);   /* - 0.16 night */
    int32_t res = clamp(((((int32_t)p[P_E1] << 8) + m->shape - (64 << 8)) * 129 >> 7) + mulq15(night, 3277), 0, 32767);
    int32_t sub = p[P_E7] * 150;                                                /* SUB: up to 0.58 */
    uint32_t tim = (uint32_t)p[P_E2] % MT_NTIMB, i;
    int32_t idx, idx2, fb, g, lpg, vib;
    uint32_t inc1, inc2, im1, im2, ph, pm1, pm2, phs;
    int32_t fbk = v->s[0], lp = v->s[1];
    int32_t idx0, idx20, g0, didx, didx2, dg;
    /* FM index: c * sqrt(c) * 1.8 (Q12), less at night, up to x2 with the Montee */
    idx = mt_lerp(MT_IDX_Q12, cut, 128);                                     /* cut: index << 8 */
    idx = mulq15(idx, 32767 - mulq15(night, 20316));                         /* x (1 - 0.62 night) */
    idx += mulq15(idx, mont);
    idx = mulq15(idx, 9830 + mulq15(m->envq15, 22937));                      /* x (0.3 + 0.7 env) */
    idx = mulq15(idx, MT_KF_Q15[(m->pitch16 >> 4) & 127]);                   /* high notes: less */
    idx2 = mulq15(idx, 14746 - mulq15(night, 11797));                        /* 0.45 (1 - 0.8 night) */
    fb = mulq15((res * 14418) >> 15, 32767 - mulq15(night, 27853));          /* 0.22 r (1 - 0.85 night), Q16 */

    /* level: resonance loss 1 / (1 + 3.8 r), a note above the brightness fades, +25 % at night */
    {
        int32_t fcn16 = 360 + ((cut * 1824) >> 15);                          /* cutoff note: 22.5 + 114 c st */
        int32_t dn = clamp(m->pitch16 - fcn16 + (64 << 4), 0, 128 << 4);
        int32_t trk = mt_lerp(MT_TRK_Q15, dn << 4, 128);
        if (trk < 19661)                                                     /* soft sounds stay audible: about -5 dB at most */
            trk = 19661 + ((trk - 19661) >> 3);
        g = mulq15(trk, mt_lerp(MT_RESG_Q15, res, 128));
        g += mulq15(g, night >> 2);
        g = mulq15(g, 29491);                                                /* 0.9: the Cmajor output gain */
    }
    lpg = mt_lerp(MT_FMLP_Q15, night >> 3, 16);

    /* pitches: vibrato with the Montee (+-20 cents at full), modulator 2 detuned by CLOCH */
    vib = (mulq15(sine_i(mt_part[t - trk].vph), mont) * 47) >> 15;          /* 1/4096: 47 = 20 cents */
    inc1 = m->inc + (uint32_t)((int32_t)(m->inc >> 12) * vib);
    inc2 = (uint32_t)(((uint64_t)m->inc * MT_DET_Q16[clamp(p[P_E3] * 3 + 192, 0, 384)]) >> 16);   /* keeps TUNE, unison */
    inc2 += (uint32_t)((int32_t)(inc2 >> 12) * vib);
    im1 = fm_ratio_inc(inc1, MT_R1_Q8[tim]);
    im2 = fm_ratio_inc(inc2, MT_R2_Q8[tim]);

    /* index and level glide over the block (no zipper when a knob or the envelope moves) */
    if (!v->s[5]) {
        v->s[2] = idx;
        v->s[3] = idx2;
        v->s[4] = g;
        v->s[5] = 1;
    }
    idx0 = v->s[2];
    idx20 = v->s[3];
    g0 = v->s[4];
    didx = idx - idx0;
    didx2 = idx2 - idx20;
    dg = g - g0;
    ph = v->ph[0];
    phs = (uint32_t)v->s[6];
    pm1 = v->ph[1];
    pm2 = v->ph[2];
    for (i = 0; i < n; i++) {
        int32_t m1, m2, car, s, ki = idx0 + ((didx * (int32_t)i) >> CTL_LOG2);
        int32_t ki2 = idx20 + ((didx2 * (int32_t)i) >> CTL_LOG2), kg = g0 + ((dg * (int32_t)i) >> CTL_LOG2);
        m1 = sine_i(pm1 + ((uint32_t)(fb * fbk) << 1));
        fbk = (fbk + m1) >> 1;
        m2 = sine_i(pm2);
        car = sine_i(ph + ((uint32_t)(ki * m1) << 5) + ((uint32_t)(ki2 * m2) << 5));
        lp += mulq15(car - lp, lpg);
        s = mulq15(lp, kg);
        if (sub) {                                       /* SUB: a pure sine an octave below, past the brightness */
            s = mulq15(s, 32767 - (sub >> 1)) + mulq15(sine_i(phs), sub);
            phs += inc1 >> 1;
        }
        out[i] += mulq15(mulq15(s, amp_at(m, i)), VOICE_FS) * 2;    /* x 2: the level of the other engines */
        ph += inc1;
        pm1 += im1;
        pm2 += im2;
    }
    v->ph[0] = ph;
    v->s[6] = (int32_t)phs;
    v->ph[1] = pm1;
    v->ph[2] = pm2;
    v->s[0] = fbk;
    v->s[1] = lp;
    v->s[2] = idx;
    v->s[3] = idx2;
    v->s[4] = g;
}

/* Modular One: 16 presets, the sound of Modular Town, in 2 families of 8 (MT_FAM): the breath, the electric pianos.
 * E = BRIL GRAIN TIMB CLOCH MONT NUIT ILE SUB, env = ATK DEC SUS REL (time index: 10000 ** (v / 127) ms),
 * fenv = ENV > BRIL, FX = DIST CHORUS DELAY REVERB sends, ARP = MODE RATE OCT GATE,
 * XT = VOICE (0 POLY 1 MONO 2 LEG 3 UNI) GLIDE, LFO RATE WAVE (SIN TRI SAW SQR S&H), LFO > BRIL, LFO > PITCH */
#define MT_NFAM 2
static const char *const MT_FAM[MT_NFAM] = {"SOUFFLES", "KEYS"};
static const char *const MT_FAM3[MT_NFAM] = {"SOU", "KEY"};
static const preset_t MTFM_PRESETS[] = {
    /* SOUFFLES: the breath (feedback grain, the low modulator, the night) */
    {"SOUFFLE", {40, 60, 7, 0, 20, 40, 0, 0}, {110, 95, 127, 115}, 0, 0, FX(0, 25, 10, 60), PAT(5)},
    {"MAREE", {35, 70, 7, 0, 60, 60, 0, 20}, {120, 105, 127, 122}, 0, 0, FX(0, 30, 20, 90), PAT(5), XT(0, 0, 20, 0, 12, 0)},
    {"ZEPHYR", {48, 55, 7, 0, 30, 20, 0, 10}, {104, 92, 120, 110}, 0, 0, FX(0, 35, 15, 60), PAT(5), XT(0, 0, 40, 0, 6, 0)},
    {"PRINTEMP", {62, 40, 2, 0, 10, 10, 0, 0}, {98, 80, 100, 104}, 0, 0, FX(0, 20, 28, 50), PAT(5)},
    {"VELOURS", {45, 45, 6, 0, 0, 50, 0, 20}, {104, 90, 115, 108}, 0, 0, FX(0, 40, 15, 55), PAT(5)},
    {"NUIT", {60, 50, 7, 0, 15, 110, 0, 0}, {100, 90, 110, 112}, 0, 0, FX(0, 15, 30, 60), PAT(5)},
    {"BRUINE", {44, 58, 7, 6, 25, 50, 0, 10}, {112, 98, 124, 118}, 0, 0, FX(0, 30, 25, 75), PAT(5), XT(0, 0, 28, 0, 8, 0)},
    {"CREPUSCULE", {52, 48, 6, 0, 35, 80, 0, 15}, {106, 95, 122, 118}, 0, 0, FX(0, 25, 20, 80), PAT(5)},
    /* KEYS: soft electric pianos of the tine kind (M1 the body, M2 at 7x the tine, ENV > BRIL a gentle bark) */
    {"VELOURS EP", {25, 15, 11, 0, 0, 30, 0, 10}, {0, 104, 45, 76}, 22, 0, FX(0, 45, 12, 35), PAT(6)},
    {"SALON", {30, 15, 10, 0, 0, 35, 0, 0}, {0, 102, 40, 74}, 12, 0, FX(0, 40, 20, 45), PAT(6), XT(0, 0, 74, 0, 6, 0)},
    {"LAMPE", {35, 40, 0, 0, 0, 50, 0, 0}, {0, 98, 50, 80}, 18, 0, FX(5, 40, 15, 45), PAT(6)},
    {"FEUTRE", {26, 12, 11, 0, 0, 45, 0, 10}, {0, 100, 40, 80}, 15, 0, FX(0, 40, 10, 40), PAT(6)},
    {"VEILLEUSE", {28, 25, 11, 2, 0, 60, 0, 0}, {0, 106, 40, 92}, 15, 0, FX(0, 30, 25, 60), PAT(6)},
    {"COTON", {28, 18, 0, 0, 0, 40, 0, 10}, {0, 100, 45, 80}, 14, 0, FX(0, 50, 12, 40), PAT(6)},
    {"BOUGIE", {30, 20, 11, 2, 0, 30, 0, 0}, {0, 100, 40, 76}, 18, 0, FX(0, 30, 15, 40), PAT(6), XT(0, 0, 84, 1, 10, 0)},
    {"DIMANCHE", {32, 35, 11, 0, 15, 40, 0, 0}, {10, 104, 55, 95}, 15, 0, FX(0, 35, 20, 60), PAT(6)},
};

static const engine_t ENG_TOWN = {
    "TOWN", {"FM", "ESPACE"},
    {
        {"BRIL", F_PCT, 0, 127, 70, 0, 0},
        {"GRAIN", F_PCT, 0, 127, 15, 0, 0},
        {"TIMB", F_ENUM, 0, MT_NTIMB - 1, 2, N_MTTIMB, 0},
        {"CLOCH", F_BIPCT, -64, 63, 0, 0, 0},
        {"MONT", F_PCT, 0, 127, 0, 0, 0},
        {"NUIT", F_PCT, 0, 127, 0, 0, 0},
        {"ILE", F_ONOFF, 0, 1, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
    },
    MTFM_PRESETS, sizeof(MTFM_PRESETS) / sizeof(MTFM_PRESETS[0]), -1, mt_note_on, mt_render,
    0x9FD3, {P_E0, P_E4, P_SUS, P_REL}, 0, 0, 0, mt_block,
};
