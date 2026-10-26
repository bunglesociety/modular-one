/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Modular One menu (HOME held): BOLD TEXT (the small letters drawn bold), LOW CUT, SCREEN SAVER (the
 * town after a minute; the old ZOOM setting, 0 = on), CALIBRATION, ABOUT. One screen only (the handheld
 * green): no theme to choose. BOLD lives in settings.palette (ui_draw.c m1_theme_apply). */
enum { MI_BOLD, MI_LOWCUT, MI_ZOOM, MI_PANEL, MI_ABOUT, MI_BACK, MI_COUNT };
static const char *const MI_NAME[MI_COUNT] = {"BOLD TEXT", "LOW CUT", "SCREEN SAVER", "CALIBRATION", "ABOUT", "BACK"};

static void bold_set(uint32_t bold)
{
    settings.palette = 0x100u | (bold ? 0x200u : 0u);
    m1_theme_apply(0);
    ui.force = 1;
}

static void draw_menu(void)
{
    uint32_t i, pass, sig = ui.menu * 7u + ui.menu_sel * 131u + settings.lowcut * 7919u + settings.zoom * 104729u +
                            settings.palette * 31u + m1_bold * 17u;
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    for (pass = 0; pass < 2u; pass++) {             /* the canvas holds 124 rows: two bands */
        m1_begin(240, 120, 0, (int32_t)(pass * 120u));
        cv_oy = -(int32_t)(pass * 120u);
        if (ui.menu == 2) {
            static const char *const L[] = {"ALEXIS ARAKIS", "BUNGLESOCIETY.COM", "", "BASED ON FELUCCA",
                                            "LEO KUROSHITA, H\xDCGELTON", "GPL-3.0, NO WARRANTY", "",
                                            "FONTS (OFL): SILKSCREEN,", "TERMINUS",
                                            "SAMPLES: VERSILIAN, H\xDCGELTON"};
            m1_rrect_o(4, 4, 232, 232, 0, M_TEXT);
            m1_rrect_o(7, 7, 226, 226, 0, M_DIM);
            m1_text_c(120, 22, FN_NAME, "MODULAR ONE", M_TEXT);
            m1_text_c(120, 46, FN_LAB, "BY BUNGLE SOCIETY", M_GREY);
            m1_text(20, 66, FN_LAB, FELUCCA_VERSION, M_TEXT);
            m1_text_r(220, 66, FN_LAB, __DATE__, M_DIM);
            cv_rect(20, 80, 200, 2, M_LINE);
            for (i = 0; i < sizeof L / sizeof L[0]; i++)
                m1_text(20, 92 + (int32_t)i * 12, FN_LAB, L[i], i < 2u ? M_TEXT : M_GREY);
            m1_text_c(120, 218, FN_LAB, "OCT- BACK", M_DIM);
        } else {
            cv_rect(0, 0, 240, 26, M_TEXT);
            m1_text(12, 5, FN_NAME, "MENU", M_BG);
            for (i = 0; i < MI_COUNT; i++) {
                int32_t y = 36 + (int32_t)i * 28;
                int sel = i == ui.menu_sel;
                if (sel) {                              /* the cursor of the old games: a solid arrow */
                    int32_t k;
                    for (k = 0; k < 5; k++)
                        cv_rect(6 + k * 2, y + 1 + k * 2, 2, 18 - k * 4, M_TEXT);
                }
                m1_text(22, y, FN_MID, MI_NAME[i], sel ? M_TEXT : M_DIM);
                if (i == MI_LOWCUT || i == MI_ZOOM || i == MI_BOLD) {
                    int on = i == MI_LOWCUT ? settings.lowcut != 0 : i == MI_BOLD ? m1_bold != 0 : settings.zoom == 0;
                    m1_rrect(192, y + 2, 40, 14, 0, on ? M_TEXT : M_PANEL);
                    m1_text_c(212, y + 5, FN_LAB, on ? "ON" : "OFF", on ? M_BG : M_DIM);
                }
            }
            cv_rect(0, 206, 240, 2, M_LINE);
            m1_text_c(120, 216, FN_LAB, "PRESETS MOVE   OCT+ OK   OCT- BACK", M_DIM);
        }
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
}

static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

static void menu_close(void)
{
    settings_save();                                   /* settings / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

/* menu: PRESETS moves, OCT+ confirms, OCT- cancels (ABOUT -> list -> close) */
static void menu_input(uint32_t pressed)
{
    int32_t s;
    uint32_t ok = (pressed >> panel.btn[B_OCTUP]) & 1u, back = (pressed >> panel.btn[B_OCTDN]) & 1u;
    if (back) {
        if (ui.menu == 2)
            ui.menu = 1, ui.force = 1;
        else
            menu_close();
        return;
    }
    if ((s = panel_enc(EN_PRESET)) != 0 && ui.menu == 1)
        ui.menu_sel = (uint8_t)((ui.menu_sel + (s > 0 ? 1u : MI_COUNT - 1u)) % MI_COUNT);
    s = panel_enc(EN_K1);
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_BOLD) {       /* right = ON, left = OFF; OCT+ toggles */
        bold_set(s > 0 ? 1u : s < 0 ? 0u : !m1_bold);
        s = 0;
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && (ui.menu_sel == MI_LOWCUT || ui.menu_sel == MI_ZOOM)) {
        /* KNOB 1: right = ON, left = OFF; OCT+ toggles */
        uint32_t *v = ui.menu_sel == MI_LOWCUT ? &settings.lowcut : &settings.zoom;
        int on = ui.menu_sel == MI_LOWCUT ? *v != 0 : *v == 0;
        on = s > 0 ? 1 : s < 0 ? 0 : !on;
        *v = ui.menu_sel == MI_LOWCUT ? (uint32_t)on : (uint32_t)!on;
        fx_lowcut = (uint8_t)(settings.lowcut != 0);
        ok = 0;
    }
    if (ok && ui.menu == 1) {
        switch (ui.menu_sel) {
        case MI_PANEL:
            panel_setup();
            ui.force = 1;
            break;
        case MI_ABOUT:
            ui.menu = 2;
            ui.force = 1;
            break;
        default:
            menu_close();
            break;
        }
    }
    enc_drop();                                        /* swallow the rest while the menu is up */
}
