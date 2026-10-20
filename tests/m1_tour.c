/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One: a listening tour of the TOWN presets, one WAV per family (8 presets, ~5 s each,
 * a phrase that suits the family: held chords, melodies, a bass line).
 *   build/host/m1_tour OUT_DIR */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
#include "../firmware/src/params.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/slicer.c"
#include "../firmware/src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"
#include "../firmware/src/seq.c"


static int16_t clip16(int32_t x) { return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }

static void apply(track_t *t, uint32_t pre)
{
    const preset_t *P = &ENG_TOWN.presets[pre];
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    t->engine = t->eng_req = MT_ENGINE;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = P->e[i];
    t->p[P_ATK] = P->env[0];
    t->p[P_DEC] = P->env[1];
    t->p[P_SUS] = P->env[2];
    t->p[P_REL] = P->env[3];
    t->p[P_ED_FLT] = P->fenv;
    t->p[P_VOICE] = P->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++) {
        t->p[P_DIST + i] = (int16_t)(P->fx[i] ? P->fx[i] - 1 : 0);
        t->p[P_AMODE + i] = 0;                       /* the tour plays its own phrases */
    }
    preset_extras(t, P);
}

typedef struct { float at, len; uint8_t n[5]; } ev_t;
static const ev_t CHORDS[] = {{0.0f, 3.4f, {45, 57, 60, 64, 71}}};
static const ev_t BELLS[] = {{0.0f, 0.4f, {72}}, {0.45f, 0.4f, {76}}, {0.9f, 0.4f, {79}}, {1.35f, 0.4f, {83}},
                             {1.8f, 0.4f, {79}}, {2.25f, 1.6f, {76, 64}}};
static const ev_t KEYS[] = {{0.0f, 1.1f, {57, 60, 64, 67}}, {1.2f, 1.1f, {53, 57, 60, 64}}, {2.4f, 1.6f, {55, 59, 62, 67}}};
static const ev_t BASS[] = {{0.0f, 0.25f, {33}}, {0.3f, 0.25f, {33}}, {0.6f, 0.25f, {45}}, {0.9f, 0.25f, {33}},
                            {1.2f, 0.25f, {36}}, {1.5f, 0.25f, {36}}, {1.8f, 0.25f, {48}}, {2.1f, 0.25f, {38}},
                            {2.4f, 0.55f, {31}}, {3.0f, 0.9f, {33}}};
static const ev_t LEAD[] = {{0.0f, 0.55f, {69}}, {0.5f, 0.3f, {72}}, {0.8f, 0.6f, {74}}, {1.4f, 0.9f, {76}},
                            {2.3f, 0.35f, {74}}, {2.65f, 1.2f, {72}}};
static const ev_t PLUCK[] = {{0.0f, 0.2f, {69}}, {0.25f, 0.2f, {72}}, {0.5f, 0.2f, {76}}, {0.75f, 0.2f, {81}},
                             {1.0f, 0.2f, {79}}, {1.25f, 0.2f, {76}}, {1.5f, 0.2f, {72}}, {1.75f, 0.2f, {74}},
                             {2.0f, 0.2f, {77}}, {2.25f, 0.2f, {81}}, {2.5f, 0.2f, {84}}, {2.75f, 0.9f, {76}}};
static const ev_t TUNE[] = {{0.0f, 0.2f, {60}}, {0.25f, 0.2f, {64}}, {0.5f, 0.2f, {67}}, {0.75f, 0.4f, {69}},
                            {1.25f, 0.2f, {67}}, {1.5f, 0.2f, {64}}, {1.75f, 0.4f, {62}}, {2.25f, 0.2f, {60}},
                            {2.5f, 0.2f, {62}}, {2.75f, 0.9f, {64, 55}}};
static const struct { const ev_t *e; uint32_t n; } PHRASE[MT_NFAM] = {{CHORDS, 1}, {KEYS, 3}};

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/m1_tour";
    uint32_t fam, i, k, s;
    track_t *t = &trk[0];
    int32_t o[2 * CTL], peak = 0;
    static const char *const FN[MT_NFAM] = {"1_souffles", "2_keys"};
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
    }
    song.master_q12 = 4096;
    for (fam = 0; fam < MT_NFAM; fam++) {
        char path[256];
        const float SLOT = 5.2f;
        uint32_t N = (uint32_t)(SLOT * 8 * FS) + 2u * FS;
        uint32_t hdr[11] = {0x46464952, 36 + N * 4, 0x45564157, 0x20746d66, 16, 0x00020001, FS, FS * 4, 0x00100004,
                            0x61746164, N * 4};
        FILE *f;
        snprintf(path, sizeof path, "%s/%s.wav", dir, FN[fam]);
        f = fopen(path, "wb");
        fwrite(hdr, 4, 11, f);
        for (s = 0; s < N; s += CTL) {
            float sec = (float)s / FS, prev = s >= CTL ? (float)(s - CTL) / FS : -1.0f;
            uint32_t slot = (uint32_t)(sec / SLOT), pre = fam * 8u + slot;
            float u = sec - slot * SLOT, up = prev - slot * SLOT;
            if (slot < 8u) {
                const ev_t *e = PHRASE[fam].e;
                if (u < (float)CTL / FS) {                 /* a new preset: silence what sounds */
                    for (k = 0; k < 128u; k++)
                        trk_note_off(t, k);
                    apply(t, pre);
                    printf("%s %u %s\n", FN[fam], slot + 1, ENG_TOWN.presets[pre].name);
                }
                for (i = 0; i < PHRASE[fam].n; i++) {
                    for (k = 0; k < 5u && e[i].n[k]; k++) {
                        if (u >= e[i].at && up < e[i].at)
                            trk_note_on(t, e[i].n[k], 100);
                        if (u >= e[i].at + e[i].len && up < e[i].at + e[i].len)
                            trk_note_off(t, e[i].n[k]);
                    }
                }
            }
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i], r = o[2 * i + 1];
                int16_t w[2] = {clip16(l), clip16(r)};
                fwrite(w, 2, 2, f);
                if ((l < 0 ? -l : l) > peak)
                    peak = l < 0 ? -l : l;
            }
        }
        fclose(f);
    }
    printf("m1_tour: peak %.1f dBFS\n", 20 * log10(peak / 32768.0 + 1e-9));
    return peak >= 32767;
}
