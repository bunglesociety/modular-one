/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One: every screen of the interface on the host (the real ui_draw.c into a 240x240
 * buffer), as PPM files into DIR: the opening, HOME with a chord playing, every page, the menu.
 *   build/host/m1_ui_host DIR [THEME] */
#define main hostsim_main
#include "hostsim.c"
#undef main
static uint16_t fb[240 * 240];
static uint32_t lcd_px;
static uint8_t lcd_busy;
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            fb[(y + j) * 240 + x + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    if (x + w > 240 || y + h > 240) {
        printf("city: blit out of the panel %u %u %u %u\n", x, y, w, h);
        exit(1);
    }
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++) {
            uint16_t c = px[j * w + i];
            fb[(y + j) * 240 + x + i] = (uint16_t)((c >> 8) | (c << 8));
        }
    lcd_px += w * h;
}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
struct felucca_dbg { uint32_t stage; } felucca_dbg;
static void project_save(uint32_t slot) { (void)slot; }
static void panel_setup(void) {}
static void project_load(uint32_t slot) { (void)slot; }
static int project_used(uint32_t slot) { (void)slot; return 0; }
static int up_used(uint32_t k) { (void)k; return 0; }
static int up_load(uint32_t k) { (void)k; return 0; }
static uint32_t up_count(void) { return 0; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t s) { return s; }
static void up_name(uint32_t k, char *b) { (void)k; b[0] = 0; }
static void up_slot_label(char *b, uint32_t k) { (void)k; b[0] = 0; }
static void up_ui(uint32_t op, uint32_t k) { (void)op; (void)k; }
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };

struct { uint32_t magic, palette, lowcut, zoom; } settings = {0, 4, 0, 0};
static struct { uint8_t btn[32]; } panel;
enum { B_OCTDN, B_OCTUP };
static int32_t panel_enc(uint32_t k) { (void)k; return 0; }
static void settings_save(void) {}
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
#define FELUCCA_VERSION "M1 0.1"
#include "../firmware/src/gfx.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/mt_city.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_menu.c"

static void save(const char *dir, const char *name)
{
    char p[256];
    snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
    FILE *f = fopen(p, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (uint32_t i = 0; i < 240 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void audio_run(uint32_t blocks)                 /* the DSP, into the scope as audio.c does */
{
    int32_t o[2 * CTL];
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)(o[2u * i] > 32767 ? 32767 : o[2u * i] < -32768 ? -32768 : o[2u * i]);
    }
}

static void frame(void)
{
    ui.force = 1;
    ui_draw();
    ui_draw();                                          /* the second frame: what changed only */
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/m1_ui";
    uint32_t f, i;
    track_t *t;
    char nm[32];
    settings.palette = argc > 2 ? (uint32_t)atoi(argv[2]) : 4u;
    palette_set(settings.palette);
    for (f = 0; f < G_COUNT; f++)
        song.g[f] = GP[f].def;
    for (f = 0; f < NTRK; f++) {
        t = &trk[f];
        track_defaults(t);
        if (f < NPART) {
            set_engine_of(t, TRK_DEF[f][0]);
            apply_preset_to(t, TRK_DEF[f][1]);
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);
    }
    t = &trk[0];
    song.sel = 0;
    song.master_q12 = 4096;
    ui.home = 1;
    song.master_q12 = 4096;
    if (argc > 3) {                                     /* the opening as frames, 30 per second, and its sound */
        char wp[256];
        FILE *w;
        uint32_t total = 0, hdr[11];
        int32_t o[2 * CTL];
        snprintf(wp, sizeof wp, "%s/opening.wav", dir);
        w = fopen(wp, "wb");
        fwrite(hdr, 4, 11, w);
        for (i = 0; i <= 165; i++) {
            uint32_t want = (i + 1u) * FS / 30u;
            fm1_ms = i * 1000u / 30u;
            ui_draw();
            snprintf(nm, sizeof nm, "op_%03u", i);
            save(dir, nm);
            while (total < want) {
                uint32_t k;
                mix_block(o, CTL);
                for (k = 0; k < CTL; k++) {
                    int16_t s2[2] = {(int16_t)clamp(o[2 * k], -32768, 32767), (int16_t)clamp(o[2 * k + 1], -32768, 32767)};
                    fwrite(s2, 2, 2, w);
                }
                total += CTL;
            }
        }
        {
            uint32_t h2[11] = {0x46464952, 36 + total * 4, 0x45564157, 0x20746d66, 16, 0x00020001, FS, FS * 4, 0x00100004,
                               0x61746164, total * 4};
            fseek(w, 0, SEEK_SET);
            fwrite(h2, 4, 11, w);
        }
        fclose(w);
        return 0;
    }
    /* the opening */
    for (i = 0; i < 4; i++) {
        static const uint32_t MS[4] = {600, 1500, 2600, 4000};
        fm1_ms = MS[i];
        ui_draw();
        snprintf(nm, sizeof nm, "00_opening_%u", i);
        save(dir, nm);
    }
    fm1_ms = 6000;
    apply_preset_to(t, 8);                              /* VERRE */
    audio_run(40);                                      /* (the engine change fades) */
    panic_req = 0;
    trk_note_on(t, 52, 100);
    trk_note_on(t, 64, 100);
    trk_note_on(t, 71, 100);
    audio_run(60);
    frame();
    save(dir, "01_home");
    song.playing = 1;
    t->seq_idx = 5;
    for (i = 0; i < NPAGES; i++) {
        uint32_t k;
        ui.home = 0;
        ui.page = (uint8_t)i;
        if (PAGES[i].graph == GR_TRK)
            for (k = 0; k < NPART; k++)
                trk[k].peak = 8000 + (int32_t)k * 6000;
        frame();
        snprintf(nm, sizeof nm, "%02u_%s", i + 2, PAGES[i].title);
        for (k = 0; nm[k]; k++)
            if (nm[k] == ' ' || nm[k] == '/')
                nm[k] = '_';
        save(dir, nm);
    }
    ui.home = 1;
    ui.menu = 1;
    frame();
    save(dir, "90_menu");
    ui.menu = 2;
    frame();
    save(dir, "91_about");
    {                                                   /* the player: the cassette and the list */
        static const uint8_t P[] = {3, 60, 100, 50, 0, 64, 100, 50, 0, 67, 100, 100};
        static const char *const NM[3] = {"NUIT EN VILLE", "CROQUIS 1", "CROQUIS 2"};
        ui.menu = 0;
        ui.home = 1;
        for (i = 0; i < 3u; i++) {
            pl_item[i].data = P;
            pl_item[i].n = 3;
            pl_item[i].dur10 = 200u + i * 4100u;
            str_cpy(pl_item[i].name, NM[i], PL_NAME);
        }
        pl_count = 3;
        demo_on = 1;
        pl_idx = 1;
        pl_cursor = 2;
        demo_pos = 2000u * 441u;
        frame();
        save(dir, "92_player");
        pl_list_ms = (fm1_ms - 100u) | 1u;              /* KNOB 1 turned: the list pops up */
        frame();
        save(dir, "93_player_list");
        pl_del = 3;                                     /* REC held on CROQUIS 2: "erase?" */
        frame();
        save(dir, "94_player_erase");
        pl_del = 0;
        pl_list_ms = 0;
        demo_on = 0;
    }
    printf("m1_ui: %u screens into %s\n", NPAGES + 10u, dir);
    return 0;
}
