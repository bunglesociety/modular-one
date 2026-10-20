#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Town FM: three ways of showing the town on the FM-1 screen (240 x 240).
   python3 tools/mt_city_concepts.py OUT_DIR
A  "La promenade": a long street wider than the screen; every page is a place along it
   and the view travels there. The four knobs are four different objects of that place,
   left to right in knob order but each at its own height. The island is at the end of
   the quay, out at sea.
B  "La fenetre": one fixed view of the town, no knobs drawn; the town itself shows the
   sound (the sun = brightness, kites = the Montee, the lamps = sustain, ripples = release),
   and a ticket strip at the bottom gives the four exact values.
C  "La carte": the town seen from above like a hand-drawn map; each knob is a landmark,
   the one just turned is lit; the island is a dot in the sea, joined by the tram."""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mt_city_mockup import (BLUE, BROWN, CREAM, DKGREEN, DKRED, GREEN, INK, OCHRE, ORANGE, PAPER, PAPER2,  # noqa: E402
                            PAPER3, RED, SAKURA, SAKURA2, SLATE, STONE, STONE2, WINLIT, YELLOW, H, W, font, mix,
                            night_tint)


class Scene:
    def __init__(self, night=0.0, island=0.0, seed=1):
        self.n, self.isl = night, island
        self.im = Image.new("RGB", (W, H))
        self.d = ImageDraw.Draw(self.im)
        self.f = font()
        self.r = random.Random(seed)

    def c(self, col):
        return night_tint(col, self.n)

    def box(self, x0, y0, x1, y1, col, ol=INK):
        self.d.rectangle([x0, y0, x1, y1], fill=self.c(col), outline=self.c(ol) if ol else None)

    def txt(self, x, y, s, col=INK):
        self.d.text((x, y), s, font=self.f, fill=self.c(col) if self.n < 0.5 else (232, 226, 208))

    def txt_c(self, cx, y, s, col=INK):
        self.txt(cx - 4 * len(s), y, s, col)

    def sky(self, y1=110):
        top = mix((238, 214, 196), (30, 34, 62), self.n)
        bot = mix(PAPER, (62, 58, 92), self.n)
        if self.isl:
            top, bot = mix(top, (120, 92, 140), self.isl * 0.6), mix(bot, (244, 176, 140), self.isl * 0.7)
        for i, y in enumerate(range(0, y1, 8)):
            self.d.rectangle([0, y, W, y + 8], fill=mix(top, bot, i / max(1, y1 // 8)))
        return top, bot

    def sun(self, x, y, b):
        if self.n < 0.5:
            r = 7 + int(6 * b)
            self.d.ellipse([x - r - 4, y - r - 4, x + r + 4, y + r + 4], fill=mix(CREAM, (250, 240, 215), 0.5))
            self.d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 236, 180))
        else:
            self.d.ellipse([x - 8, y - 8, x + 8, y + 8], fill=(238, 232, 210))
            self.d.ellipse([x - 4, y - 10, x + 10, y + 4], fill=mix((30, 34, 62), (62, 58, 92), 0.3))
            for _ in range(22):
                self.im.putpixel((self.r.randrange(2, W - 2), self.r.randrange(4, 70)), (220, 218, 200))

    def matriarch(self, x, y, v, r=8):
        self.d.ellipse([x - r - 3, y - r - 3, x + r + 3, y + r + 3], fill=self.c(mix(STONE2, INK, 0.25)))
        self.d.ellipse([x - r - 2, y - r - 2, x + r + 2, y + r + 2], fill=self.c((176, 172, 162)))
        self.d.ellipse([x - r, y - r, x + r, y + r], fill=(28, 28, 27), outline=(9, 9, 9))
        a = -math.pi * 0.75 + v * math.pi * 1.5 - math.pi / 2
        self.d.line([x, y, x + math.cos(a) * (r - 1.5), y + math.sin(a) * (r - 1.5)], fill=(238, 234, 224), width=2)

    def petals(self, n=8, y0=20, y1=200):
        for _ in range(n):
            x, y = self.r.randrange(2, W - 4), self.r.randrange(y0, y1)
            self.d.rectangle([x, y, x + 2, y + 1], fill=self.c(SAKURA))

    def person(self, x, y, coat, hat=None):
        self.box(x - 2, y - 11, x + 2, y - 4, coat, None)
        self.box(x - 2, y - 3, x - 1, y, INK, None)
        self.box(x + 1, y - 3, x + 2, y, INK, None)
        self.d.ellipse([x - 2, y - 16, x + 2, y - 12], fill=self.c((230, 196, 160)))
        if hat:
            self.box(x - 3, y - 17, x + 3, y - 16, hat, None)

    def cat(self, x, y):
        self.box(x, y - 3, x + 7, y, INK, None)
        self.box(x + 6, y - 6, x + 9, y - 3, INK, None)
        self.d.line([x, y - 3, x - 3, y - 7], fill=self.c(INK))

    def cherry(self, x, y, s=1.0):
        self.d.line([x, y, x, y - int(18 * s)], fill=self.c(BROWN), width=3)
        for dx, dy, r in ((-7, -20, 7), (7, -21, 7), (0, -26, 8), (-3, -16, 5), (5, -15, 5)):
            r = int(r * s)
            cx, cy = x + int(dx * s), y + int(dy * s)
            self.d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=self.c(SAKURA), outline=self.c(SAKURA2))

    def header(self, left, right):
        band = mix(PAPER, (40, 40, 66), self.n)
        self.d.rectangle([0, 0, W, 16], fill=band)
        self.d.line([0, 16, W, 16], fill=self.c(INK))
        col = INK if self.n < 0.5 else (230, 224, 206)
        self.d.text((4, 0), left, font=self.f, fill=col)
        self.d.text((W - 4 - 8 * len(right), 0), right, font=self.f, fill=col)


# ----------------------------------------------------------------- A: la promenade
def promenade(night, island, labels, place, preset, seed):
    s = Scene(night, island, seed)
    s.sky(176)
    s.sun(206, 38, labels[0][2])
    # far: hills left, the sea at the far right (the end of the quay), the island out there
    sea = mix((120, 160, 186), (34, 48, 80), night)
    s.d.ellipse([-30, 70, 110, 130], fill=s.c(mix(GREEN, PAPER, 0.35)))
    s.d.rectangle([150, 94, W, 176], fill=sea)
    s.d.rectangle([0, 112, 150, 176], fill=s.c(mix(GREEN, PAPER, 0.45)))         # gardens behind the place
    if island:
        s.d.ellipse([196, 88, 236, 97], fill=s.c(mix(GREEN, (120, 92, 140), 0.45)))
        s.d.line([214, 89, 214, 78], fill=s.c(BROWN), width=2)
        for dx in (-6, 6):
            s.d.line([214, 78, 214 + dx, 81], fill=s.c(DKGREEN), width=2)
    # the place: irregular buildings of different widths, a tower, a fountain, a tree
    s.box(0, 60, 46, 176, PAPER2)                                   # tall house, left
    s.d.polygon([(-2, 61), (23, 40), (48, 61)], fill=s.c(RED), outline=s.c(INK))
    s.box(46, 92, 92, 176, (226, 206, 176))                          # low workshop
    s.box(44, 88, 94, 92, BROWN)
    s.box(124, 70, 150, 176, (214, 196, 170))                        # narrow clock tower
    s.d.polygon([(122, 71), (137, 50), (152, 71)], fill=s.c(SLATE), outline=s.c(INK))
    s.box(150, 112, 174, 176, PAPER3)                                # a kiosk by the water
    s.d.polygon([(148, 113), (162, 100), (176, 113)], fill=s.c(OCHRE), outline=s.c(INK))
    win = WINLIT if night >= 0.5 else mix(BLUE, PAPER, 0.55)
    for x, y in ((8, 70), (28, 70), (8, 96), (54, 102), (74, 102), (131, 78)):
        s.d.rectangle([x, y, x + 9, y + 10], fill=win if night >= 0.5 else s.c(win), outline=s.c(INK))
    s.cherry(108, 176, 1.2)
    # the four controls, left to right in knob order, each a different object at its own height
    (l1, v1, k1), (l2, v2, k2), (l3, v3, k3), (l4, v4, k4) = labels
    # 1: a knob on the tall house's plaque
    s.box(6, 118, 40, 160, PAPER, INK)
    s.txt_c(23, 118, l1)
    s.matriarch(23, 141, k1)
    # 2: a hanging sign on the workshop
    s.d.line([58, 106, 58, 112], fill=s.c(INK))
    s.d.line([82, 106, 82, 112], fill=s.c(INK))
    s.box(52, 112, 88, 150, (250, 244, 230), INK)
    s.txt_c(70, 112, l2)
    s.matriarch(70, 135, k2, 7)
    # 3: the clock face of the tower
    s.d.ellipse([125, 80, 149, 104], fill=s.c(PAPER), outline=s.c(INK))
    s.matriarch(137, 92, k3, 7)
    s.txt_c(137, 106, l3)
    # 4: a knob on the kiosk counter
    s.matriarch(162, 136, k4, 7)
    s.txt_c(162, 148, l4[:4])
    # street, quay, water
    s.box(0, 176, W, 190, STONE, None)
    s.d.line([0, 176, W, 176], fill=s.c(INK))
    s.d.line([0, 189, W, 189], fill=s.c(SLATE))
    for x in range(178, W, 10):                                      # the quay ends: steps to the sea
        s.d.line([x, 176, x + 4, 190], fill=s.c(STONE2))
    s.person(34, 189, BLUE, BROWN)
    s.person(118, 189, RED)
    s.cat(196, 188)
    water = mix((86, 128, 160), (30, 44, 72), night)
    s.d.rectangle([0, 191, W, 222], fill=water)
    for _ in range(12):
        x, y = s.r.randrange(0, W - 10), s.r.randrange(194, 220)
        s.d.line([x, y, x + s.r.randrange(4, 10), y], fill=mix(water, (240, 236, 220), 0.35))
    s.petals(7)
    s.header("120 T1", preset)
    # the values: a small tram ticket at the bottom
    band = mix(PAPER2, (36, 36, 58), night)
    s.d.rectangle([0, 223, W, H], fill=band)
    s.d.line([0, 223, W, 223], fill=s.c(INK))
    for i, v in enumerate((v1, v2, v3, v4)):
        s.txt_c(30 + i * 60, 224, v)
    # where we are, on a street sign
    s.box(176, 18, 236, 34, (60, 92, 72), INK)
    s.d.text((206 - 4 * len(place), 18), place, font=s.f, fill=(240, 236, 220))
    return s.im


# --------------------------------------------------------------- B: la fenetre
def fenetre(night, island, vals, preset, seed, people=True):
    bril, mont, sus, rel = vals
    s = Scene(night, island, seed)
    s.sky(130)
    s.sun(60, 26 + int(40 * (1 - bril)), bril)                       # brightness: the sun rises
    sea = mix((120, 160, 186), (34, 48, 80), night)
    s.d.rectangle([0, 104, W, 130], fill=sea)
    if island:
        s.d.ellipse([150, 97, 210, 107], fill=s.c(mix(GREEN, (120, 92, 140), 0.45)))
        s.d.line([180, 98, 180, 86], fill=s.c(BROWN), width=2)
        for dx in (-7, 7):
            s.d.line([180, 86, 180 + dx, 89], fill=s.c(DKGREEN), width=2)
    # kites rise with the Montee
    for i, (kx, col) in enumerate(((150, RED), (186, YELLOW), (214, BLUE))):
        ky = 92 - int(mont * (50 + 12 * i))
        s.d.polygon([(kx, ky - 6), (kx + 5, ky), (kx, ky + 7), (kx - 5, ky)], fill=s.c(col), outline=s.c(INK))
        s.d.line([kx, ky + 7, 120 + i * 8, 168], fill=s.c(STONE2))
    # a row of roofs seen from a window, irregular
    xs = 0
    rr = random.Random(seed)
    while xs < W:
        w = rr.choice((26, 34, 40, 22))
        h = rr.randrange(118, 140)
        col = rr.choice((PAPER2, (226, 206, 176), (214, 196, 170), PAPER3))
        s.box(xs, h, xs + w, 172, col)
        s.d.polygon([(xs - 1, h + 1), (xs + w // 2, h - 10), (xs + w + 1, h + 1)],
                    fill=s.c(rr.choice((RED, SLATE, OCHRE, BROWN))), outline=s.c(INK))
        for k in range(xs + 5, xs + w - 6, 10):
            lit = night >= 0.5 and rr.random() < 0.25 + 0.7 * sus       # sustain: windows that stay lit
            s.d.rectangle([k, h + 10, k + 5, h + 16], fill=WINLIT if lit else s.c(mix(BLUE, PAPER, 0.55)),
                          outline=s.c(INK))
        xs += w
    s.cherry(36, 186, 1.3)
    s.cherry(206, 186, 1.0)
    s.box(0, 172, W, 186, STONE, None)
    s.d.line([0, 172, W, 172], fill=s.c(INK))
    if people:
        s.person(90, 185, BLUE, BROWN)
        s.person(160, 185, RED)
        s.cat(120, 184)
    water = mix((86, 128, 160), (30, 44, 72), night)
    s.d.rectangle([0, 187, W, 220], fill=water)
    for k in range(int(4 + 14 * rel)):                               # release: ripples that linger
        x, y = s.r.randrange(0, W - 14), s.r.randrange(190, 218)
        s.d.line([x, y, x + 6 + int(8 * rel), y], fill=mix(water, (240, 236, 220), 0.35))
    s.petals(9)
    s.header("120 T1", preset)
    band = mix(PAPER2, (36, 36, 58), night)
    s.d.rectangle([0, 221, W, H], fill=band)
    s.d.line([0, 221, W, 221], fill=s.c(INK))
    for i, (lab, v) in enumerate((("BRI", "55"), ("MON", f"{int(mont * 100)}"), ("SUS", f"{int(sus * 100)}"),
                                  ("REL", "2.0"))):
        s.d.text((4 + i * 60, 223), f"{lab} {v}"[:7], font=s.f, fill=INK if night < 0.5 else (230, 224, 206))
    return s.im


# ----------------------------------------------------------------- C: la carte
def carte(night, island, vals, hot, preset, seed):
    s = Scene(night, island, seed)
    land = mix((222, 214, 186), (52, 56, 76), night)
    sea = mix((150, 182, 200), (30, 44, 72), night)
    s.d.rectangle([0, 0, W, H], fill=sea)
    s.d.polygon([(0, 18), (170, 18), (190, 60), (176, 120), (196, 170), (150, 222), (0, 222)], fill=land,
                outline=s.c(INK))
    # the canal and the streets, hand drawn
    s.d.line([20, 200, 80, 150, 120, 140, 176, 120], fill=mix(sea, land, 0.2), width=7)
    for a, b in (((14, 60), (160, 60)), ((60, 22), (70, 218)), ((14, 112), (150, 104)), ((112, 22), (122, 210))):
        s.d.line([a, b], fill=s.c(STONE2), width=3)
    # the tram line to the island
    s.d.line([150, 104, 196, 150, 214, 186], fill=s.c(INK), width=1)
    for t in range(0, 20, 4):
        s.im.putpixel((150 + t * 3, 104 + t * 3), s.c(PAPER))
    s.d.ellipse([196, 176, 232, 202], fill=s.c(mix(GREEN, (120, 92, 140), 0.35 if island else 0)), outline=s.c(INK))
    s.d.line([214, 188, 214, 178], fill=s.c(BROWN), width=2)
    # blocks of houses, small roofs seen from above
    rr = random.Random(seed)
    for _ in range(26):
        x, y = rr.randrange(14, 150), rr.randrange(24, 200)
        if abs(y - 60) < 6 or abs(x - 66) < 6 or abs(x - 117) < 6 or abs(y - 108) < 6:
            continue
        w, h = rr.randrange(8, 16), rr.randrange(7, 13)
        s.box(x, y, x + w, y + h, rr.choice((RED, SLATE, OCHRE, BROWN)), INK)
    for x, y in ((30, 140), (96, 80), (140, 170), (40, 40)):
        s.d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=s.c(SAKURA), outline=s.c(SAKURA2))
    # the four landmarks = the four knobs
    marks = ((40, 84, "BRIL"), (96, 40, "MONT"), (90, 160, "SUS"), (150, 84, "REL"))
    for i, ((x, y, lab), v) in enumerate(zip(marks, vals)):
        lit = i == hot
        if lit:
            s.d.ellipse([x - 16, y - 16, x + 16, y + 16], fill=mix(land, CREAM, 0.6))
        s.matriarch(x, y, v, 8)
        s.d.rectangle([x - 4 * len(lab) - 2, y + 12, x + 4 * len(lab) + 1, y + 28],
                      fill=mix(PAPER, land, 0.3) if not lit else RED)
        s.d.text((x - 4 * len(lab), y + 12), lab, font=s.f, fill=INK if not lit else (250, 244, 230))
    # compass rose and the title cartouche
    s.d.line([214, 30, 214, 54], fill=s.c(INK))
    s.d.line([202, 42, 226, 42], fill=s.c(INK))
    s.d.text((210, 16), "N", font=s.f, fill=s.c(INK) if night < 0.5 else (230, 224, 206))
    s.header("120 T1", preset)
    band = mix(PAPER2, (36, 36, 58), night)
    s.d.rectangle([0, 223, W, H], fill=band)
    s.d.line([0, 223, W, 223], fill=s.c(INK))
    s.d.text((4, 224), "MONT  0%  TOURNE", font=s.f, fill=INK if night < 0.5 else (230, 224, 206))
    return s.im


def main(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    home = [("BRIL", "55%", 0.55), ("MONT", "0%", 0.0), ("SUS", "70%", 0.7), ("REL", "2.0s", 0.82)]
    ville = [("MONT", "80%", 0.8), ("NUIT", "55%", 0.55), ("ILE", "ON", 1.0), ("", "", 0.0)]
    imgs = {
        "A1_promenade_place_jour": promenade(0.0, 0.0, home, "PLACE", "PRINTEMP", 2),
        "A2_promenade_port_ile": promenade(0.55, 1.0, [("MONT", "80%", 0.8), ("NUIT", "55%", 0.55),
                                                       ("ILE", "ON", 1.0), ("LUNE", "-", 0.0)], "PORT", "L'ILE", 4),
        "B1_fenetre_jour": fenetre(0.0, 0.0, (0.55, 0.25, 0.7, 0.8), "PRINTEMP", 6),
        "B2_fenetre_ile": fenetre(0.6, 1.0, (0.4, 0.85, 0.8, 0.9), "L'ILE", 6),
        "C1_carte_jour": carte(0.0, 0.0, (0.55, 0.0, 0.7, 0.82), 1, "PRINTEMP", 9),
        "C2_carte_nuit_ile": carte(0.8, 1.0, (0.55, 0.8, 0.7, 0.82), 3, "L'ILE", 9),
    }
    for name, im in imgs.items():
        im.resize((W * 3, H * 3), Image.NEAREST).save(out / f"{name}.png")
    del ville
    print(f"concepts in {out}")


if __name__ == "__main__" and not (len(sys.argv) > 2 and sys.argv[2] == "habitants"):
    main(sys.argv[1] if len(sys.argv) > 1 else "build/concepts")


# ------------------------------------------------- the inhabitants, as pixel sprites
# After index.js: _drawTraveller (blue coat, wide straw hat, a bundle on the back, a stick),
# _drawWoman (dark red dress, golden belt, a terracotta parasol with ribs, a brown hat),
# _catBody (black ink, pale eyes, the tail up). Facing right; '.' is transparent.
SPRITES = {
    "traveller": (
        "....hhhh......",
        "...hHHHHh.....",
        ".hhhhhhhhhh...",
        "....ssss......",
        "....sssk......",
        ".....ss.......",
        "..rrbbbb......",
        ".rrrbbbbb.....",
        ".rrrbbbbbs....",
        "..rrbbbbbst...",
        "....bbbbb.t...",
        "....bbbbb.t...",
        "....bBbbb.t...",
        "....bBbbb.t...",
        "....bBbbbb.t..",
        "....bbbbbb.t..",
        "....k...k..t..",
        "....k...k..t..",
        "...k....k...t.",
        "..kk....kk..t.",
    ),
    "woman": (
        "......pppppp......",
        "....pppPpppPppp...",
        "...pPpppPpppPppp..",
        "...kkkkkkkkkkkkk..",
        ".........k........",
        "....yyy..k........",
        "...yyyyy.k........",
        "....sss..k........",
        "....ssk..k........",
        ".....s...k........",
        "....ddd.sk........",
        "...ddddds.........",
        "...ddddd..........",
        "...ggggg..........",
        "...ddddd..........",
        "..dddddDd.........",
        "..ddddDddd........",
        ".dddddDdddd.......",
        ".ddddDddddd.......",
        "....k..k..........",
        "...kk..kk.........",
    ),
    "cat": (
        "k..........k.k",
        "k..........kkk",
        ".k........kkek",
        ".k........kkkk",
        "..kkkkkkkkkkk.",
        "..kkkkkkkkkk..",
        "..kkkkkkkkk...",
        "..k.k....k.k..",
        ".k..k....k..k.",
    ),
}
SPRITE_COL = {
    "k": (30, 22, 12), "h": (198, 170, 110), "H": (170, 142, 86), "s": (224, 190, 158),
    "b": (78, 98, 134), "B": (58, 74, 104), "r": (170, 94, 58), "t": (126, 92, 56),
    "p": (216, 136, 102), "P": (170, 96, 70), "y": (178, 136, 102), "d": (122, 58, 56),
    "D": (96, 42, 40), "g": (212, 182, 122), "e": (230, 220, 180),
}


def sprite(s, name, x, y, flip=False):
    """draw a sprite with its feet on y; ink stays ink at night, the clothes darken"""
    rows = SPRITES[name]
    h = len(rows)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row[::-1] if flip else row):
            if ch == ".":
                continue
            col = SPRITE_COL[ch]
            if ch not in "ke":
                col = s.c(col)
            s.im.putpixel((x + i, y - h + j), col)


def fenetre2(night, island, vals, preset, seed):
    """B with the three inhabitants instead of the generic passers-by"""
    im = fenetre(night, island, vals, preset, seed, people=False)
    s = Scene(night, island, seed)
    s.im = im
    s.d = ImageDraw.Draw(im)
    sprite(s, "traveller", 58, 186)
    sprite(s, "cat", 112, 186)
    sprite(s, "woman", 150, 186, flip=True)
    return im


def inhabitants(out):
    s = Scene(0.0, 0.0, 1)
    s.im = Image.new("RGB", (80, 30), mix(STONE, PAPER, 0.5))
    s.d = ImageDraw.Draw(s.im)
    sprite(s, "traveller", 4, 26)
    sprite(s, "woman", 24, 26)
    sprite(s, "cat", 52, 26)
    s.im.resize((80 * 8, 30 * 8), Image.NEAREST).save(out / "habitants_x8.png")


if __name__ == "__main__" and len(sys.argv) > 2 and sys.argv[2] == "habitants":
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    fenetre2(0.0, 0.0, (0.55, 0.25, 0.7, 0.8), "PRINTEMP", 6).resize((W * 3, H * 3), Image.NEAREST).save(out / "B1_fenetre_habitants_jour.png")
    fenetre2(0.6, 1.0, (0.4, 0.85, 0.8, 0.9), "L'ILE", 6).resize((W * 3, H * 3), Image.NEAREST).save(out / "B2_fenetre_habitants_ile.png")
    inhabitants(out)
    print(f"inhabitants in {out}")
