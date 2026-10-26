/* SPDX-License-Identifier: GPL-3.0-only */
/* Modular One 0.2: the sketchbook (seq.c sk_put / sk_extract) and the player (several pieces,
 * next / restart / previous, the gap between pieces). No flash: the pieces are in RAM.
 * m1_book_test */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)
#define M1_GYMNO                  /* the player starts with the Gymnopedie (seq.c) */
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

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("m1_book: FAIL " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* a fake flash for m1_book.c: the three user slots, AND-writes, 4 KiB erases */
#ifndef SMP_USER_BASE
#define SMP_USER_BASE 0xA0000u
#define SMP_USER_SIZE 0x14000u
#endif
static uint8_t fl_mem[3u * SMP_USER_SIZE];
static int flash_ok = 1;
static char last_msg[40];
static const uint8_t *fake_xip(uint32_t off) { return fl_mem + (off - SMP_USER_BASE); }
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void audio_silence(void) {}
static void fm1_wdt_feed(void) {}
static void ui_message(const char *m) { str_cpy(last_msg, m, sizeof last_msg); }
static int fl_erase4k(uint32_t off, uint32_t *took) { *took = 0; memset(fl_mem + (off - SMP_USER_BASE), 0xFF, 0x1000); return 0; }
static int fl_write(uint32_t off, const uint8_t *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        fl_mem[off - SMP_USER_BASE + i] &= p[i];
    return 0;
}
static uint32_t st_crc32(const void *p, uint32_t n)
{
    const uint8_t *b = p;
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *b++;
        for (k = 0; k < 8u; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
#define fm1_xip_ptr fake_xip
#include "../firmware/src/m1_book.c"
#undef fm1_xip_ptr

/* the sketchbook in flash: four sketches, erase one in the middle, the last, the first */
static void book_erase_test(void)
{
    static uint8_t d[4][9000];
    static const uint32_t L[4] = {1000, 6000, 300, 9000};      /* 1, 2, 1, 3 sectors */
    uint32_t i, k, s;
    memset(fl_mem, 0xFF, sizeof fl_mem);
    demo_on = 0;
    m1_rescan();
    for (i = 0; i < 4u; i++) {
        for (k = 0; k < L[i]; k++)
            d[i][k] = (uint8_t)(k * 7u + i * 31u);
        CHECK(!m1_sk_write(m1b.free_sect, m1b.next_num, 0xFFFFFFFFu, d[i], L[i], 10u + i, 100u * (i + 1u)), "write sketch %u", i + 1u);
        m1_rescan();
    }
    CHECK(pl_count == 4 && m1b.free_sect == 7, "4 sketches in 7 sectors (%u, %u)", pl_count, m1b.free_sect);
    m1_del(1);                                                  /* CROQUIS 2: 2 sectors */
    CHECK(pl_count == 3 && m1b.free_sect == 5, "erase the 2nd: 3 left in 5 sectors (%u, %u)", pl_count, m1b.free_sect);
    CHECK(!strcmp(pl_item[0].name, "CROQUIS 1") && !strcmp(pl_item[1].name, "CROQUIS 3") && !strcmp(pl_item[2].name, "CROQUIS 4"),
          "names kept: %s, %s, %s", pl_item[0].name, pl_item[1].name, pl_item[2].name);
    CHECK(pl_item[1].n == 12 && !memcmp(pl_item[1].data, d[2], L[2]) && pl_item[2].n == 13 && !memcmp(pl_item[2].data, d[3], L[3]),
          "the sketches after it moved down intact");
    for (s = 5; s < 7; s++)
        CHECK(fl_mem[SMP_USER_SIZE + s * 0x1000u] == 0xFF, "sector %u left blank", s);
    CHECK(!strcmp(last_msg, "CROQUIS 2 ERASED"), "message: %s", last_msg);
    m1_del(2);
    CHECK(pl_count == 2 && m1b.free_sect == 2, "erase the last (%u, %u)", pl_count, m1b.free_sect);
    m1_del(0);
    CHECK(pl_count == 1 && !strcmp(pl_item[0].name, "CROQUIS 3") && m1b.free_sect == 1 && !memcmp(pl_item[0].data, d[2], L[2]),
          "erase the first (%u, %s)", pl_count, pl_item[0].name);
    CHECK(!m1_sk_write(m1b.free_sect, m1b.next_num, 0xFFFFFFFFu, d[0], L[0], 9u, 50u), "write after erases");
    m1_rescan();
    CHECK(pl_count == 2 && !strcmp(pl_item[1].name, "CROQUIS 4"), "the next one: CROQUIS 4 (after the highest left) (%s)", pl_item[1].name);
    CHECK(!m1_sk_rename(0, "nuit bleue"), "rename");               /* a sketch from before names */
    m1_rescan();
    CHECK(!strcmp(pl_item[0].name, "NUIT BLEUE") && pl_item[0].n == 12 && !memcmp(pl_item[0].data, d[2], L[2]),
          "renamed, notes intact (%s)", pl_item[0].name);
    CHECK(!m1_sk_rename(0, "AUBE"), "rename again");
    m1_rescan();
    CHECK(!strcmp(pl_item[0].name, "AUBE") && !memcmp(pl_item[0].data, d[2], L[2]), "renamed again (%s)", pl_item[0].name);
    CHECK(!m1_sk_rename(0, ""), "rename to nothing");
    m1_rescan();
    CHECK(!strcmp(pl_item[0].name, "CROQUIS 3"), "no name: CROQUIS 3 (%s)", pl_item[0].name);
    CHECK(!m1_sk_rename(1, "PLUIE"), "rename the 2nd");
    m1_del(0);
    CHECK(pl_count == 1 && !strcmp(pl_item[0].name, "PLUIE") && !memcmp(pl_item[0].data, d[0], L[0]) && pl_item[0].n == 9,
          "a name moves with its sketch (%s)", pl_item[0].name);
    CHECK(!m1_sk_write(m1b.free_sect, m1b.next_num, 0xFFFFFFFFu, d[3], 4096u - 32u - 4u, 7u, 100u), "a full sector");
    m1_rescan();
    CHECK(m1_sk_rename(1, "X") == 4, "no room for a name in a full sector: rc 4");
    m1_del(0); m1_del(0);
    CHECK(pl_count == 0 && m1b.free_sect == 0, "all erased (%u)", pl_count);
}


static void at(double sec) { sk_clock = (uint32_t)(sec * 44100.0); }
static void on(double sec, uint32_t note) { at(sec); sk_put(note, 100); }
static void off(double sec, uint32_t note) { at(sec); sk_put(note, 0); }

/* decode a piece: count its notes, its length (s) */
static uint32_t decode(const uint8_t *d, uint32_t n, double *end, uint32_t *first_note)
{
    uint32_t i;
    double e = 0;
    pl_data = d;
    pl_n = n;
    demo_i = demo_rd = demo_at = 0;
    for (i = 0;; i++) {
        uint32_t a, no, len, vel;
        if (!demo_next(&a, &no, &len, &vel))
            break;
        if (!i && first_note)
            *first_note = no;
        if ((a + len) / 44100.0 > e)
            e = (a + len) / 44100.0;
    }
    *end = e;
    return i;
}

static void run(double sec)
{
    int32_t o[2 * CTL];
    uint32_t s;
    for (s = 0; s < (uint32_t)(sec * 44100.0); s += CTL)
        mix_block(o, CTL);
}

int main(void)
{
    static uint8_t buf[SK_EV * 6u], a1[4096], a2[4096];
    uint32_t len, notes, dur, got, nfirst, n1, n2, i, k;
    double end;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
    }
    song.master_q12 = 4096;
    trk[0].engine = trk[0].eng_req = MT_ENGINE;

    /* nothing played */
    len = sk_extract(1, buf, sizeof buf, &notes, &dur, &got);
    CHECK(!len, "nothing played but %u bytes", len);

    /* phrase A: 60 62 64, with a 3 s pause inside (no split), then 6 s of silence, phrase B: 72 74 */
    on(1.0, 60); off(1.5, 60);
    on(1.6, 62); off(2.0, 62);
    on(5.0, 64); off(5.5, 64);                 /* 3 s after 62: the same phrase */
    on(11.5, 72); on(11.6, 74); off(12.5, 72); off(13.0, 74);
    at(20.0);
    len = sk_extract(1, buf, sizeof buf, &notes, &dur, &got);
    n1 = decode(buf, notes, &end, &nfirst);
    CHECK(notes == 2 && n1 == 2 && nfirst == 72 && got == 1, "last phrase: %u notes (first %u), %u phrases", notes, nfirst, got);
    CHECK(end > 1.35 && end < 1.65, "last phrase lasts %.2f s, not 1.5", end);
    CHECK(dur >= 145 && dur <= 155, "dur10 %u", dur);
    len = sk_extract(2, buf, sizeof buf, &notes, &dur, &got);
    n2 = decode(buf, notes, &end, &nfirst);
    CHECK(notes == 5 && n2 == 5 && nfirst == 60 && got == 2, "two phrases: %u notes (first %u), %u phrases", notes, nfirst, got);
    CHECK(end > 11.9 && end < 12.1, "two phrases last %.2f s, not 12", end);
    len = sk_extract(3, buf, sizeof buf, &notes, &dur, &got);
    CHECK(got == 2 && notes == 5, "only two phrases exist: got %u", got);

    /* a key still held: it lasts until now */
    on(30.0, 50);
    at(32.0);
    len = sk_extract(1, buf, sizeof buf, &notes, &dur, &got);
    decode(buf, notes, &end, &nfirst);
    CHECK(notes == 1 && nfirst == 50 && end > 1.9 && end < 2.1, "held key: %u notes, %.2f s", notes, end);
    off(32.5, 50);

    /* older than 2 minutes: forgotten */
    at(200.0);
    len = sk_extract(1, buf, sizeof buf, &notes, &dur, &got);
    CHECK(!len, "phrases older than 2 minutes are kept (%u notes)", notes);

    /* the ring wraps: 3000 notes in one phrase, the sketch has the last ones, no overflow */
    for (i = 0; i < 1500u; i++) {
        on(300.0 + i * 0.05, 40 + i % 40);
        off(300.0 + i * 0.05 + 0.04, 40 + i % 40);
    }
    at(400.0);
    len = sk_extract(1, buf, sizeof buf, &notes, &dur, &got);
    CHECK(len && len <= sizeof buf && notes > 600u, "wrapped ring: %u bytes, %u notes", len, notes);

    /* the player: three pieces, next / restart / previous, then it runs to the end by itself */
    sk_w = 0;
    on(1000.0, 60); off(1001.0, 60); on(1001.0, 64); off(1002.0, 64);
    at(1010.0);
    len = sk_extract(1, a1, sizeof a1, &n1, &dur, &got);
    sk_w = 0;
    on(1100.0, 67); off(1100.5, 67);
    at(1110.0);
    len = sk_extract(1, a2, sizeof a2, &n2, &dur, &got);
    pl_item[1].data = a1; pl_item[1].n = n1; str_cpy(pl_item[1].name, "CROQUIS 1", PL_NAME);
    pl_item[2].data = a2; pl_item[2].n = n2; str_cpy(pl_item[2].name, "CROQUIS 2", PL_NAME);
    pl_count = 3;
    demo_req = 1;
    run(1.0);
    CHECK(demo_on && pl_idx == 0, "start: on %u, piece %u", demo_on, pl_idx);
    demo_req = 3; run(0.1);
    CHECK(demo_on && pl_idx == 1, "next: piece %u", pl_idx);
    demo_req = 4; run(0.1);
    CHECK(demo_on && pl_idx == 1 && demo_pos < 0.2 * 44100, "restart: piece %u at %.2f s", pl_idx, demo_pos / 44100.0);
    demo_req = 5; run(0.1);
    CHECK(demo_on && pl_idx == 0, "previous: piece %u", pl_idx);
    demo_req = 5; run(0.1);
    CHECK(demo_on && pl_idx == 2, "previous of the first: the last, piece %u", pl_idx);
    demo_req = 3; run(0.1);
    CHECK(demo_on && pl_idx == 0, "next of the last: the first, piece %u", pl_idx);
    demo_req = 3; run(0.1);
    run(3.6);                                     /* piece 1 (2 s) ends, 1.5 s gap, piece 2 */
    CHECK(demo_on && pl_idx == 2, "auto next: piece %u", pl_idx);
    run(2.5);
    CHECK(demo_on && pl_idx == 0, "after the last piece: the first again (piece %u, on %u)", pl_idx, demo_on);
    demo_req = 2; run(0.5);
    {
        uint32_t busy = 0;
        for (i = 0; i < NVOICE; i++)
            busy += trk[0].v[i].gate;
        CHECK(!busy, "%u notes left held", busy);
    }
    CHECK(trk[0].p[P_VOICE] == TP[P_VOICE].def, "voice mode not restored");
    demo_req = 1; run(0.5); demo_req = 2; run(0.5);
    CHECK(!demo_on, "stop");
    book_erase_test();
    printf("m1_book: %s\n", fails ? "FAILED" : "ok");
    return fails;
}
