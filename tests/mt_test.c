/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Town FM on the host: the film scene of the prototype through the whole firmware mix
 * (TOWN on part 1, the Modular Town delay and plate, the island on the master).
 *   build/host/mt_test OUT.wav [PRESET]
 * Scene (64 s + 12 s of tails, slow chords and a held top note, preset PRINTEMP by default):
 *    0-16 s day, Montee 0 / 16-32 s Montee 0 -> 127 -> 25 / 32-48 s night, Montee 38
 *   48-64 s the island (night), Montee 0 -> 127; released at 59 s.
 * Checks: no clipping at the output, no DC, the effect lines never saturate, silence at the end,
 * every voice free; prints the cost of the mix in ns / sample. Exit status = failed checks. */
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

#define SECS 76                          /* 64 s of scene, 12 s for the tails */
static const uint8_t CH[4][5] = {{45, 57, 60, 64, 76}, {41, 57, 60, 65, 77}, {36, 55, 60, 64, 79}, {43, 55, 59, 62, 74}};

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static int16_t clip16(int32_t x) { return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }

static uint32_t sat_count(const int16_t *b, uint32_t n)
{
    uint32_t i, c = 0;
    for (i = 0; i < n; i++)
        c += b[i] == 32767 || b[i] == -32768;
    return c;
}

static float ramp(float t, float t0, float t1, float a, float b)
{
    return t <= t0 ? a : t >= t1 ? b : a + (b - a) * (t - t0) / (t1 - t0);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "mt_scene.wav";
    uint32_t pre = argc > 2 ? (uint32_t)atoi(argv[2]) : 0u, i, k, s, N = SECS * FS, fails = 0, sat = 0;
    track_t *t = &trk[0];
    const preset_t *P = &ENG_TOWN.presets[pre % ENG_TOWN.npresets];
    FILE *f = fopen(path, "wb");
    int32_t o[2 * CTL], chord = -1, peak = 0;
    double dc_l = 0, sq = 0;
    uint64_t ns = 0;
    uint32_t hdr[11] = {0x46464952, 36 + N * 4, 0x45564157, 0x20746d66, 16, 0x00020001, FS, FS * 4, 0x00100004,
                        0x61746164, N * 4};
    fwrite(hdr, 4, 11, f);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
    }
    song.master_q12 = 4096;
    t->engine = t->eng_req = MT_ENGINE;              /* the whole preset, as ui.c apply_preset_to */
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
        t->p[P_AMODE + i] = (int16_t)(P->arp[i] ? P->arp[i] - 1 : TP[P_AMODE + i].def);
    }
    preset_extras(t, P);
    for (s = 0; s < N; s += CTL) {
        float sec = (float)s / FS;
        int32_t c = (int32_t)(s / (FS * 4u)) % 4;
        float m;
        t->p[P_E5] = sec < 32 ? P->e[5] : 127;                          /* night from 32 s */
        t->p[P_E6] = sec >= 48 ? 1 : P->e[6];                             /* the island from 48 s */
        if (sec < 16)
            m = P->e[4];
        else if (sec < 32)
            m = sec < 26 ? ramp(sec, 16, 26, 0, 127) : ramp(sec, 26, 32, 127, 25);
        else if (sec < 48)
            m = 38;
        else
            m = ramp(sec, 50, 60, 0, 127);
        t->p[P_E4] = (int16_t)m;
        if (sec < 59 && c != chord && s % (FS * 4u) < CTL) {
            if (chord >= 0)
                for (k = 0; k < 5u; k++)
                    trk_note_off(t, CH[chord][k]);
            for (k = 0; k < 5u; k++)
                trk_note_on(t, CH[c][k], 100);
            chord = c;
        }
        if (sec >= 59 && chord >= 0) {
            for (k = 0; k < 5u; k++)
                trk_note_off(t, CH[chord][k]);
            chord = -2;
        }
        {
            uint64_t t0 = now_ns();
            mix_block(o, CTL);
            ns += now_ns() - t0;
        }
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i], r = o[2 * i + 1], a = l < 0 ? -l : l;
            int16_t w[2] = {clip16(l), clip16(r)};
            fwrite(w, 2, 2, f);
            if (a > peak)
                peak = a;
            dc_l += l;
            if (s + i >= N - FS / 2u)
                sq += (double)l * l;
        }
        if (s % (FS / 4u) < CTL)                    /* the effect lines, four times a second */
            sat += sat_count(mt_dl, MT_DLEN_L) + sat_count(mt_dr, MT_DLEN_R) + sat_count(mt_comb, MT_COMB_L + MT_COMB_R)
                 + sat_count(mt_dc, 2 * (2731 + 2999)) + sat_count(mt_psl, MT_PS) + sat_count(mt_psr, MT_PS);
    }
    fclose(f);
    {
        double dc = dc_l / N / 32768.0, tail = sqrt(sq / (FS / 2)) / 32768.0;
        uint32_t busy = 0;
        for (i = 0; i < NVOICE; i++)
            busy += t->v[i].active;
        printf("mt_test: %s preset %s: peak %.1f dBFS, DC %.5f, tail %.1f dBFS, line saturations %u, "
               "voices left %u, mix %.0f ns/sample\n", path, P->name, 20 * log10(peak / 32768.0 + 1e-9), dc,
               20 * log10(tail + 1e-9), sat, busy, (double)ns / N);
        if (peak >= 32767) { printf("mt_test: FAIL clipping at the output\n"); fails++; }
        if (fabs(dc) > 0.002) { printf("mt_test: FAIL DC\n"); fails++; }
        if (sat) { printf("mt_test: FAIL an effect line saturates\n"); fails++; }
        if (busy) { printf("mt_test: FAIL voices still active at the end\n"); fails++; }
    }
    return (int)fails;
}
