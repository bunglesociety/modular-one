#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Town FM: mock-ups of the spring town on the FM-1 screen (240 x 240).
   python3 tools/mt_city_mockup.py OUT_DIR
Drawn pixel by pixel at the screen's own size, with the firmware's font (Terminus 8x16),
so what is approved here is what the firmware will draw. The look follows index.js of
Modular Town V2: flat, muted "cut paper", warm paper tones, soft ink outlines, Matriarch
knobs set into the facades, sakura petals in spring. One building = one knob (KNOB 1..4)."""
import math
import random
import sys
from pathlib import Path

from PIL import BdfFontFile, Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
W = H = 240

# palette of index.js (the most used colours), plus spring
INK = (47, 41, 35)
PAPER = (242, 236, 220)
PAPER2 = (233, 225, 204)
PAPER3 = (232, 220, 192)
RED = (184, 72, 58)
DKRED = (158, 42, 34)
BLUE = (58, 127, 196)
OCHRE = (201, 164, 90)
BROWN = (122, 83, 53)
GREEN = (122, 148, 96)
DKGREEN = (95, 122, 78)
YELLOW = (224, 180, 58)
ORANGE = (232, 145, 47)
STONE = (217, 210, 196)
STONE2 = (188, 179, 163)
SLATE = (95, 107, 122)
SAKURA = (240, 176, 186)
SAKURA2 = (226, 150, 166)
CREAM = (255, 227, 161)
WINLIT = (255, 209, 120)


def font():
    pil = Path("/tmp/mt_ter16.pil")
    if not pil.exists():
        with open(ROOT / "assets/fonts/ter-u16n.bdf", "rb") as f:
            BdfFontFile.BdfFontFile(f).save(str(pil))
    return ImageFont.load(str(pil))


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def night_tint(c, n):
    """darken towards a blue night, as index.js does with daynight"""
    return mix(c, (int(c[0] * 0.30), int(c[1] * 0.34), int(c[2] * 0.52) + 18), n)


class Town:
    def __init__(self, night=0.0, island=0.0, seed=7):
        self.n, self.isl = night, island
        self.im = Image.new("RGB", (W, H))
        self.d = ImageDraw.Draw(self.im)
        self.f = font()
        self.rnd = random.Random(seed)

    def c(self, col):
        return night_tint(col, self.n)

    def rect(self, x0, y0, x1, y1, col, outline=None):
        self.d.rectangle([x0, y0, x1, y1], fill=self.c(col), outline=self.c(outline) if outline else None)

    def text(self, x, y, s, col, shadow=None):
        if shadow:
            self.d.text((x + 1, y + 1), s, font=self.f, fill=self.c(shadow))
        self.d.text((x, y), s, font=self.f, fill=self.c(col))

    def text_w(self, s):
        return 8 * len(s)

    # ---------------------------------------------------------------- sky
    def sky(self):
        day_top, day_bot = (238, 214, 196), PAPER          # spring: a pink morning
        top = mix(day_top, (30, 34, 62), self.n)
        bot = mix(day_bot, (62, 58, 92), self.n)
        if self.isl:                                         # the island: a warm dusk glow
            top = mix(top, (120, 92, 140), self.isl * 0.6)
            bot = mix(bot, (244, 176, 140), self.isl * 0.7)
        for i, y in enumerate(range(0, 112, 8)):             # flat bands, cut paper
            self.d.rectangle([0, y, W, y + 8], fill=mix(top, bot, i / 13))
        if self.n < 0.5:                                     # the sun
            cx, cy = 196, 44
            self.d.ellipse([cx - 13, cy - 13, cx + 13, cy + 13], fill=mix(CREAM, (255, 236, 196), 0.3))
            self.d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], fill=(255, 238, 190))
        else:                                                # the moon and stars
            cx, cy = 190, 40
            self.d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], fill=(238, 232, 210))
            self.d.ellipse([cx - 5, cy - 11, cx + 11, cy + 5], fill=mix(top, bot, 0.25))
            for _ in range(26):
                x, y = self.rnd.randrange(4, W - 4), self.rnd.randrange(20, 80)
                self.im.putpixel((x, y), (230, 226, 210) if self.rnd.random() < 0.6 else (160, 160, 190))
        for cx, cy, w in ((44, 40, 34), (122, 30, 26)):    # clouds of paper
            col = mix((250, 244, 230), (90, 88, 118), self.n)
            self.d.rounded_rectangle([cx - w // 2, cy, cx + w // 2, cy + 8], 4, fill=col)
            self.d.rounded_rectangle([cx - w // 4, cy - 5, cx + w // 6, cy + 4], 4, fill=col)

    def island(self):
        if self.isl <= 0:
            return
        hz = 86                                              # on the sea horizon, above the low shop
        col = mix(mix(GREEN, (120, 92, 140), 0.45), (40, 40, 70), self.n * 0.6)
        self.d.ellipse([186, hz - 6, 234, hz + 3], fill=col)
        self.d.line([196, hz - 7, 196, hz - 19], fill=self.c(BROWN), width=2)     # a palm
        for dx, dy in ((-7, 3), (7, 3), (-5, -2), (5, -2)):
            self.d.line([196, hz - 19, 196 + dx, hz - 19 + dy], fill=self.c(DKGREEN), width=2)
        self.d.rectangle([204, hz - 12, 206, hz - 7], fill=self.c(INK))        # Nick, very small
        self.d.ellipse([203, hz - 15, 207, hz - 11], fill=self.c(INK))

    def hills(self):
        sea = mix((120, 160, 186), (34, 48, 80), self.n)
        if self.isl:
            sea = mix(sea, (196, 140, 150), self.isl * 0.4)
        self.d.rectangle([110, 86, W, 112], fill=sea)                 # the sea, far on the right
        self.d.line([110, 86, W, 86], fill=mix(sea, (250, 244, 230), 0.4))
        for x0, w, h, col in ((-20, 110, 22, GREEN), (50, 90, 16, DKGREEN)):
            self.d.ellipse([x0, 96 - h, x0 + w, 96 + h], fill=self.c(mix(col, PAPER, 0.35)))
        self.d.rectangle([0, 96, 120, 112], fill=self.c(mix(GREEN, PAPER, 0.45)))

    # ---------------------------------------------------------- buildings
    def knob(self, x, y, v):
        """Matriarch knob set into the facade: socket, bezel, black body, light needle"""
        self.d.ellipse([x - 11, y - 11, x + 11, y + 11], fill=self.c(mix(STONE2, INK, 0.25)))      # socket
        self.d.ellipse([x - 10, y - 10, x + 10, y + 10], fill=self.c((176, 172, 162)))              # bezel
        self.d.ellipse([x - 8, y - 8, x + 8, y + 8], fill=(28, 28, 27), outline=(9, 9, 9))           # body
        for i in range(18):                                  # knurling
            a = i / 18 * 2 * math.pi
            self.im.putpixel((int(round(x + math.cos(a) * 7.5)), int(round(y + math.sin(a) * 7.5))), (8, 8, 8))
        a = -math.pi * 0.75 + v * math.pi * 1.5 - math.pi / 2
        self.d.line([x, y, x + math.cos(a) * 6.5, y + math.sin(a) * 6.5], fill=(238, 234, 224), width=2)
        for t in (0.0, 1.0):                                 # range dots
            a = -math.pi * 0.75 + t * math.pi * 1.5 - math.pi / 2
            self.im.putpixel((int(x + math.cos(a) * 13), int(y + math.sin(a) * 13)), self.c(INK))

    def building(self, i, style, label, value, v, hot=False):
        x0 = 2 + i * 60
        x1 = x0 + 55
        roof_y = {"gable": 64, "flat": 76, "tall": 58, "shop": 92}[style]
        wall = {"gable": PAPER2, "flat": (226, 206, 176), "tall": (214, 196, 170), "shop": PAPER3}[style]
        roof = {"gable": RED, "flat": BROWN, "tall": SLATE, "shop": OCHRE}[style]
        base = 170
        self.rect(x0, roof_y, x1, base, wall, INK)                               # facade
        if style == "gable":
            self.d.polygon([(x0 - 3, roof_y + 1), ((x0 + x1) // 2, roof_y - 18), (x1 + 3, roof_y + 1)],
                           fill=self.c(roof), outline=self.c(INK))
            self.rect((x0 + x1) // 2 - 4, roof_y - 9, (x0 + x1) // 2 + 3, roof_y - 3, CREAM, INK)  # attic window
        elif style == "flat":
            self.rect(x0 - 2, roof_y - 4, x1 + 2, roof_y, roof, INK)
            self.rect(x1 - 14, roof_y - 14, x1 - 8, roof_y - 4, BROWN, INK)                   # chimney
        elif style == "tall":
            self.d.polygon([(x0 - 2, roof_y + 1), (x0 + 8, roof_y - 12), (x1 - 8, roof_y - 12), (x1 + 2, roof_y + 1)],
                           fill=self.c(roof), outline=self.c(INK))
        else:                                                                     # shop: awning
            self.rect(x0 - 2, roof_y - 4, x1 + 2, roof_y, BROWN, INK)
        # two windows above the plaque, lit at night
        win = mix(BLUE, PAPER, 0.55) if self.n < 0.5 else WINLIT
        for wx in ((x0 + 8, x1 - 19) if style != "shop" else ()):   # the low shop has its awning instead
            self.d.rectangle([wx, roof_y + 6, wx + 11, roof_y + 17], fill=win if self.n >= 0.5 else self.c(win),
                             outline=self.c(INK))
            self.d.line([wx + 6, roof_y + 6, wx + 6, roof_y + 17], fill=self.c(INK))
            self.rect(wx - 1, roof_y + 18, wx + 12, roof_y + 19, roof)                         # sill
        # the plaque: label, knob, value
        lx = (x0 + x1) // 2
        if not label or label == "-":                                             # no control: a flower box
            self.rect(lx - 12, 124, lx + 12, 130, BROWN, INK)
            for k in range(-10, 11, 4):
                self.d.rectangle([lx + k - 1, 119, lx + k + 1, 123], fill=self.c((RED, SAKURA, YELLOW)[(k // 4) % 3]))
            dx = x0 + (6 if i % 2 else 40)
            self.rect(dx, 158, dx + 9, 170, BROWN, INK)
            return
        lab_col = RED if hot else INK
        self.text(lx - self.text_w(label) // 2, 100, label, lab_col)
        self.knob(lx, 128, v)
        vc = (255, 255, 255) if hot else INK
        self.text(lx - self.text_w(value) // 2, 142, value, vc if not hot else INK)
        if style == "shop":                                                       # awning stripes
            for k in range(0, 56, 8):
                self.d.polygon([(x0 + k, 160), (x0 + k + 8, 160), (x0 + k + 6, 166), (x0 + k + 2, 166)],
                               fill=self.c(RED if (k // 8) % 2 else PAPER), outline=self.c(INK))
        # door
        dx = x0 + (6 if i % 2 else 40)
        self.rect(dx, 158, dx + 9, 170, BROWN, INK)

    # -------------------------------------------------------------- street
    def street(self):
        self.rect(0, 170, W, 186, STONE)
        self.d.line([0, 170, W, 170], fill=self.c(INK))
        for x in range(0, W, 12):                            # cobbles
            self.d.line([x + (4 if (x // 12) % 2 else 0), 175, x + 6 + (4 if (x // 12) % 2 else 0), 175], fill=self.c(STONE2))
            self.d.line([x + (8 if (x // 12) % 2 else 2), 181, x + 12, 181], fill=self.c(STONE2))
        self.d.line([0, 185, W, 185], fill=self.c(SLATE))    # tram rail
        # a cherry tree between buildings 2 and 3
        tx = 118
        self.d.line([tx, 186, tx, 168], fill=self.c(BROWN), width=3)
        self.d.line([tx, 174, tx - 6, 167], fill=self.c(BROWN), width=2)
        self.d.line([tx, 172, tx + 6, 166], fill=self.c(BROWN), width=2)
        for cx, cy, r in ((tx - 7, 165, 6), (tx + 7, 164, 6), (tx, 161, 7), (tx - 2, 168, 5), (tx + 4, 169, 5)):
            self.d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=self.c(SAKURA), outline=self.c(SAKURA2))
        for _ in range(10):
            x, y = tx + self.rnd.randrange(-11, 12), 157 + self.rnd.randrange(0, 14)
            self.im.putpixel((x, y), self.c((252, 226, 230)))
        # lamps, lit at night
        for lx in (58, 178):
            self.d.line([lx, 186, lx, 156], fill=self.c(INK), width=1)
            self.rect(lx - 2, 152, lx + 2, 156, INK)
            if self.n >= 0.5:
                self.rect(lx - 1, 153, lx + 1, 155, WINLIT)

    def people(self):
        def person(x, coat, hat=None):
            y = 185
            self.rect(x - 2, y - 11, x + 2, y - 4, coat)                                # coat
            self.rect(x - 2, y - 3, x - 1, y, INK)
            self.rect(x + 1, y - 3, x + 2, y, INK)                                       # legs
            self.d.ellipse([x - 2, y - 16, x + 2, y - 12], fill=self.c((230, 196, 160)))  # head
            if hat:
                self.rect(x - 3, y - 17, x + 3, y - 16, hat)
        person(34, BLUE, BROWN)
        person(152, RED)
        person(206, OCHRE, INK)
        # the cat
        cx, cy = 88, 184
        self.rect(cx, cy - 3, cx + 7, cy, INK)
        self.rect(cx + 6, cy - 6, cx + 9, cy - 3, INK)
        self.im.putpixel((cx + 6, cy - 7), self.c(INK))
        self.im.putpixel((cx + 9, cy - 7), self.c(INK))
        self.d.line([cx, cy - 3, cx - 3, cy - 7], fill=self.c(INK))

    def quay_and_water(self):
        self.rect(0, 186, W, 192, STONE2)
        self.d.line([0, 192, W, 192], fill=self.c(INK))
        for x in range(3, W, 9):                             # flowers along the quay
            col = (RED, YELLOW, SAKURA, (250, 244, 230))[(x // 9) % 4]
            self.d.line([x, 191, x, 188], fill=self.c(DKGREEN))
            self.d.rectangle([x - 1, 186, x + 1, 188], fill=self.c(col))
        water = mix((86, 128, 160), (30, 44, 72), self.n)
        if self.isl:
            water = mix(water, (150, 112, 140), self.isl * 0.35)
        self.d.rectangle([0, 193, W, 224], fill=water)
        hl = mix(water, (240, 236, 220), 0.35)
        for k in range(16):                                  # reflections
            x, y = self.rnd.randrange(0, W - 12), self.rnd.randrange(196, 222)
            self.d.line([x, y, x + self.rnd.randrange(4, 12), y], fill=hl)
        if self.n >= 0.5:                                    # lit windows in the water
            for x in (12, 70, 132, 192):
                self.d.line([x, 198, x + 8, 198], fill=(150, 130, 90))
                self.d.line([x + 2, 202, x + 6, 202], fill=(130, 112, 84))
        for _ in range(5):                                   # petals on the water
            x, y = self.rnd.randrange(4, W - 4), self.rnd.randrange(196, 222)
            self.d.rectangle([x, y, x + 1, y], fill=self.c(SAKURA))

    def petals(self):
        for _ in range(9):
            x, y = self.rnd.randrange(4, W - 6), self.rnd.randrange(24, 168)
            self.d.rectangle([x, y, x + 2, y + 1], fill=self.c(SAKURA))
            self.im.putpixel((x + 1, y + 1), self.c(SAKURA2))

    # -------------------------------------------------------- header, foot
    def header(self, bpm, track, preset):
        band = mix(PAPER, (40, 40, 66), self.n)
        self.d.rectangle([0, 0, W, 17], fill=band)
        self.d.line([0, 17, W, 17], fill=self.c(INK))
        tcol = INK if self.n < 0.5 else (230, 224, 206)
        self.d.text((4, 1), f"▶{bpm}", font=self.f, fill=tcol) if False else self.d.text((4, 1), f"{bpm}", font=self.f, fill=tcol)
        self.d.text((44, 1), track, font=self.f, fill=tcol)
        self.d.text((W - 4 - 8 * len(preset), 1), preset, font=self.f, fill=tcol)

    def footer(self, page, hint):
        band = mix(PAPER2, (36, 36, 58), self.n)
        self.d.rectangle([0, 225, W, H], fill=band)
        self.d.line([0, 225, W, 225], fill=self.c(INK))
        tcol = INK if self.n < 0.5 else (230, 224, 206)
        self.d.text((4, 225), page, font=self.f, fill=tcol)
        self.d.text((W - 4 - 8 * len(hint), 225), hint, font=self.f, fill=mix(tcol, band, 0.45))

    def draw(self, cols, header, footer, hot=-1):
        self.sky()
        self.island()
        self.hills()
        styles = ("gable", "flat", "tall", "shop")
        for i, (lab, val, v) in enumerate(cols):
            self.building(i, styles[i], lab, val, v, hot=(i == hot))
        self.street()
        self.people()
        self.quay_and_water()
        self.petals()
        self.header(*header)
        self.footer(*footer)
        return self.im


def main(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    scenes = {
        "1_home_jour": Town().draw([("BRIL", "55%", 0.55), ("MONT", "0%", 0.0), ("SUS", "70%", 0.7), ("REL", "2.0s", 0.82)],
                                   ("120", "T1", "PRINTEMP"), ("HOME", "TOWN"), hot=1),
        "2_fm_jour": Town(seed=3).draw([("BRIL", "55%", 0.55), ("GRAIN", "12%", 0.12), ("TIMB", "1:3", 2 / 11),
                                        ("CLOCH", "+50%", 0.75)], ("120", "T1", "CLOCHES"), ("FM", "EDIT 1/2")),
        "3_ville_nuit": Town(night=1.0, seed=11).draw([("MONT", "30%", 0.3), ("NUIT", "100%", 1.0), ("ILE", "OFF", 0.0),
                                                       ("", "", 0.0)], ("120", "T1", "NUIT"), ("VILLE", "EDIT 2/2")),
        "4_ile": Town(night=0.55, island=1.0, seed=5).draw([("MONT", "80%", 0.8), ("NUIT", "55%", 0.55),
                                                            ("ILE", "ON", 1.0), ("", "", 0.0)],
                                                           ("120", "T1", "L'ILE"), ("VILLE", "EDIT 2/2")),
    }
    tiles = []
    for name, im in scenes.items():
        big = im.resize((W * 3, H * 3), Image.NEAREST)
        big.save(out / f"{name}.png")
        tiles.append(big)
    sheet = Image.new("RGB", (W * 3 * 2 + 30, H * 3 * 2 + 30), (30, 28, 26))
    for k, t in enumerate(tiles):
        sheet.paste(t, (10 + (k % 2) * (W * 3 + 10), 10 + (k // 2) * (H * 3 + 10)))
    sheet.save(out / "planche.png")
    print(f"mock-ups in {out}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build/mockup")
