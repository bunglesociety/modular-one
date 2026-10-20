/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Town FM: the town screen on the host. The panel is a 240x240 buffer; frames are written
 * as PPM into DIR (build/city_frames), with a pixel count per frame (what the SPI would carry).
 *   build/host/mt_city_host DIR
 * Story (60 frames/s): day with a barge, BRIL turned (the band), notes (rings), the night falls,
 * ILE (the island out of the haze), back to dusk with the Montee (the clouds on fire). */
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
#include "../firmware/src/gfx.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/mt_city.c"
static void ui_tick(void) {}
static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static void save(const char *dir, uint32_t n)
{
    char p[256];
    snprintf(p, sizeof p, "%s/f%04u.ppm", dir, n);
    FILE *f = fopen(p, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (uint32_t i = 0; i < 240 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/city_frames";
    uint32_t f, maxpx = 0, sum = 0, frames = 0, every = argc > 2 ? (uint32_t)atoi(argv[2]) : 2u, N = 2800;
    double t0, tmax = 0, tsum = 0;
    track_t *t;
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
    }
    t = &trk[0];
    song.sel = 0;
    ui.home = 1;
    if (!city_wanted()) {
        printf("city: not wanted on HOME with TOWN\n");
        return 1;
    }
    for (f = 0; f < N; f++) {
        int32_t night = 0;
        if (f == 300) {                                /* BRIL turned: the values band */
            t->p[P_E0] = 112;
            ui.hot_col = 0;
            ui.hot_t = 40;
        }
        if (f >= 600 && f < 1500)
            night = (int32_t)((f - 600) * 32767u / 900u);
        else if (f >= 1500 && f < 2300)
            night = 32767;
        else if (f >= 2300 && f < 2600)
            night = 32767 - (int32_t)((f - 2300) * 16000u / 300u);
        else if (f >= 2600)
            night = 32767 - 16000;
        mt_part[0].night30 = night << 15;
        if (f == 1700)
            t->p[P_E6] = 1;                            /* ILE */
        if (f == 2100)
            mt_part[0].mont30 = 30000 << 15;           /* the Montee */
        if (f % 97 == 40 && f > 200)
            trk[0].peak = 9000 + (int32_t)(f % 5) * 3000;   /* a note */
        lcd_px = 0;
        t0 = (double)clock() / CLOCKS_PER_SEC;
        city_draw();
        t0 = (double)clock() / CLOCKS_PER_SEC - t0;
        if (f) {
            sum += lcd_px;
            frames++;
            tsum += t0;
            if (t0 > tmax)
                tmax = t0;
            if (lcd_px > maxpx)
                maxpx = lcd_px;
        }
        if (!(f % every))
            save(dir, f / every);
    }
    printf("city: %u frames, %u px/frame on average (%.1f%% of the panel), max %u; host %.3f ms/frame, max %.3f\n",
           frames, sum / frames, 100.0 * sum / frames / 57600.0, maxpx, 1000 * tsum / frames, 1000 * tmax);
    return sum / frames < 6000u ? 0 : 1;
}
