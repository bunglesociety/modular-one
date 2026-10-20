/* SPDX-License-Identifier: GPL-3.0-only */
/* Modular One: the demo, Satie's 1st Gymnopedie (seq.c demo_block, album_data.h), through the
 * whole mix on one TOWN preset: it ends by itself, no clipping, every voice released at the end.
 * m1_demo out.wav [preset] */
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

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "demo.wav";
    uint32_t pre = argc > 2 ? (uint32_t)atoi(argv[2]) : 16u, i, k, s, N = 0, fails = 0, maxs = 1100u * FS, ended = 0;
    track_t *t = &trk[0];
    const preset_t *P = &ENG_TOWN.presets[pre % ENG_TOWN.npresets];
    FILE *f = fopen(path, "wb");
    int32_t o[2 * CTL], peak = 0;
    uint32_t hdr[11] = {0x46464952, 0, 0x45564157, 0x20746d66, 16, 0x00020001, FS, FS * 4, 0x00100004, 0x61746164, 0};
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
        t->p[P_AMODE + i] = (int16_t)(P->arp[i] ? P->arp[i] - 1 : TP[P_AMODE + i].def);
    }
    preset_extras(t, P);
    {
        int16_t vm = t->p[P_VOICE], fe = t->p[P_ED_FLT];
        demo_req = (uint8_t)(argc > 3 ? atoi(argv[3]) : 1);
        for (s = 0; s < maxs; s += CTL) {
            mix_block(o, CTL);
            if (s > FS && !demo_on && !ended)
                ended = s;
            if (ended && s > ended + 8u * FS)
                break;
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i], r = o[2 * i + 1], a = l < 0 ? -l : l;
                int16_t w[2] = {clip16(l), clip16(r)};
                fwrite(w, 2, 2, f);
                if (a > peak)
                    peak = a;
            }
            N += CTL;
        }
        {
            uint32_t busy = 0;
            for (i = 0; i < NVOICE; i++)
                busy += t->v[i].gate;
            printf("m1_demo: %s on %s: %.1f s, peak %.1f dBFS, gates left %u\n", path, P->name,
                   (double)ended / FS, 20 * log10(peak / 32768.0 + 1e-9), busy);
            if (!ended) { printf("m1_demo: FAIL the demo never ends\n"); fails++; }
            if (peak >= 32767) { printf("m1_demo: FAIL clipping\n"); fails++; }
            if (busy) { printf("m1_demo: FAIL notes left held\n"); fails++; }
            if (t->p[P_VOICE] != vm || t->p[P_ED_FLT] != fe || t->p[P_E0] != P->e[0]) { printf("m1_demo: FAIL settings not restored\n"); fails++; }
        }
    }
    hdr[1] = 36 + N * 4;
    hdr[10] = N * 4;
    fseek(f, 0, SEEK_SET);
    fwrite(hdr, 4, 11, f);
    fclose(f);
    return (int)fails;
}
