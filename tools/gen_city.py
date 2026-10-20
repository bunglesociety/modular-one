#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Town FM: the town seen from across the water, for the FM-1 screen.

The far shore of Modular Town (brick and cream houses, the grey building with its orange
band, the tram 81 at its stop, cherry trees, the quay and its lamps) under a large sky, the
island far away on the sea horizon. Only rows 0..MT_Q are stored; the water below is
computed by the firmware (the reflection of these rows). Output: firmware/src/mt_city_data.h

Index map of the stored picture:
  0..175    plain colours (MT_PAL), slid towards the night by the firmware
  176..183  the island (MT_ISLE_PAL): fades in from the haze with ILE
  184       lamp heads (lit at dusk)
  192..247  windows, 56 groups (each group lights on its own)
  254       far water (computed, as below the quay)
  255       sky (computed: dithered gradient, sun, moon, clouds, stars)

   python3 tools/gen_city.py [PREVIEW_DIR]"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

W = 240
Q = 150            # the waterline under the quay: the mirror of the reflection
HZ = 141           # the sea horizon (right of the shore)
# the framing: the town drawn 1.2x larger and moved left (a closer view; the first house leaves the frame)
ZOOM, DX = 1.2, -26


def X(x):
    return int(round(x * ZOOM + DX))


def L(n):
    return int(round(n * ZOOM))


SHORE = X(184)     # x where the shore ends: right of it the water mirrors at HZ
ISLE_X = 222       # the island, on the sea horizon
ROWS = Q + 1

I_ISLE, I_LAMP, I_WIN, NWIN, I_WATER, I_SKY = 176, 184, 192, 56, 254, 255

SKYK = (255, 0, 255)
WATK = (0, 255, 255)

INK = (40, 34, 38)
NAVY = (48, 56, 80)
NAVY2 = (64, 74, 100)
BRICK = (158, 78, 62)
BRICK2 = (132, 64, 52)
BRICK3 = (176, 96, 74)
CREAM = (222, 210, 190)
CREAM2 = (198, 186, 168)
BLUEG = (128, 140, 156)
SAGE = (150, 160, 134)
CONC = (176, 170, 166)
CONC2 = (152, 146, 144)
ORANGE = (214, 150, 70)
TRAM_Y = (222, 178, 70)
TRAM_C = (238, 228, 208)
TRAM_R = (190, 72, 58)
PINK = (236, 168, 182)
PINK2 = (214, 136, 158)
PINK3 = (246, 200, 206)
GREEN = (98, 128, 84)
GREEN2 = (122, 150, 96)
GREEN3 = (78, 106, 70)
QUAY = (150, 140, 132)
QUAY2 = (124, 116, 110)
QUAY3 = (176, 166, 156)
HILL = (150, 138, 152)
HILL2 = (132, 122, 142)
POST = (56, 50, 54)


def is_win(c):
    return c[0] == 1 and c[2] == 2


class Pic:
    def __init__(self):
        self.im = Image.new("RGB", (W, ROWS), SKYK)
        self.d = ImageDraw.Draw(self.im)
        self.r = random.Random(81)
        self.groups = list(range(NWIN))
        self.r.shuffle(self.groups)
        self.gi = 1                       # group groups[0]: the tram, lit all night
        self.lamps = []

    def get(self, x, y):
        return self.im.getpixel((x, y)) if 0 <= x < W and 0 <= y < ROWS else SKYK

    def px(self, x, y, c):
        if 0 <= x < W and 0 <= y < ROWS:
            self.im.putpixel((x, y), c)

    def rect(self, x0, y0, x1, y1, c):
        self.d.rectangle([x0, y0, x1, y1], fill=c)

    def window(self, x, y, w=1, h=2, group=None):
        if group is None:
            group = self.groups[self.gi % NWIN]
            self.gi = self.gi % (NWIN - 1) + 1
        for j in range(h):
            for i in range(w):
                self.px(x + i, y + j, (1, group, 2))

    # -------------------------------------------------------------- far hills
    def hills(self):
        for x in range(W):
            y = 108 + 6 * math.sin(x * 0.04 + 1.2) + 3 * math.sin(x * 0.11) + 1.5 * math.sin(x * 0.27)
            if x > 150:                                   # the ridge goes down into the sea
                y += (x - 150) ** 1.6 * 0.07
            y = int(y)
            for yy in range(y, HZ + 1):
                self.px(x, yy, HILL if yy < y + 2 else HILL2)
        for x in range(0, 130):                           # a nearer ridge, a little darker
            y = int(122 + 3 * math.sin(x * 0.06 + 2.0) + 1.5 * math.sin(x * 0.21))
            for yy in range(y, HZ + 1):
                self.px(x, yy, HILL2 if yy > y else HILL)

    # ---------------------------------------------------------------- houses
    def house(self, x, w, h, wall, roof="flat", trim=NAVY, shade=None):
        base = 145
        top = base - h
        self.rect(x, top, x + w - 1, base, wall)
        if shade:
            self.rect(x + w - 1, top, x + w - 1, base, shade)     # the shaded side
        if roof == "flat":
            self.rect(x - 1, top - 1, x + w, top, trim)           # navy cornice
        elif roof == "gable":
            for k in range((w + 1) // 2 + 1):
                self.rect(x + k - 1, top - k, x + w - k, top - k, trim)
        elif roof == "step":                                       # a Dutch stepped gable
            for k in range(3):
                self.rect(x + k * 2, top - k * 2 - 2, x + w - 1 - k * 2, top - k * 2 - 1, wall)
            self.rect(x + 4, top - 7, x + w - 5, top - 7, trim)
        cols = max(1, (w - 2) // 3)
        x0 = x + (w - (cols * 3 - 2)) // 2
        for r in range(top + 3, base - 6, 5):
            for c in range(cols):
                if self.r.random() < 0.88:
                    self.window(x0 + c * 3, r)
        self.rect(x + w // 2 - 1, base - 3, x + w // 2, base, INK)  # the door

    def modern(self, x, w, h):
        base = 145
        top = base - h
        self.rect(x, top, x + w - 1, base, CONC)
        self.rect(x + w - 2, top, x + w - 1, base, CONC2)
        self.rect(x - 1, top - 2, x + w, top, NAVY)                  # the navy slab
        self.rect(x + 3, top - 5, x + 8, top - 3, CONC2)             # rooftop units
        self.rect(x + 12, top - 4, x + 15, top - 3, CONC2)
        self.rect(x + w - 10, top - 6, x + w - 6, top - 3, NAVY2)
        self.rect(x + 2, top + 3, x + w - 3, top + 5, ORANGE)        # the orange band
        for c in range(x + 3, x + w - 4, 3):                          # a ribbon of windows
            self.window(c, top + 8, 2, 2)
        for c in range(x + 3, x + w - 6, 7):                          # shop glass
            self.window(c, base - 5, 5, 3)

    def tree(self, cx, base, rad, cols):
        for y in range(base - rad * 2 - 3, base + 1):
            for x in range(cx - rad - 2, cx + rad + 3):
                dx, dy = x - cx, (y - (base - rad - 2)) * 1.15
                n = math.sin(x * 1.7 + y * 2.3) + math.sin(x * 0.9 - y * 1.3)
                if dx * dx + dy * dy < (rad + 0.6 * n) ** 2 and y < base - 1:
                    k = 0 if dy < -rad * 0.3 else 1
                    if dx > rad * 0.3 and dy > -rad * 0.2:
                        k = 2
                    if (x * 7 + y * 13) % 11 == 0:
                        k = 0
                    self.px(x, y, cols[k])
        self.rect(cx, base - 2, cx, base, INK)

    def tram(self, x):
        y, w, h = 145, L(20), L(7)
        self.rect(x, y - h, x + w - 1, y - 1, TRAM_Y)
        self.rect(x, y - h, x + w - 1, y - h + 2, TRAM_C)
        self.rect(x, y - h + 3, x + w - 1, y - h + 3, TRAM_R)
        for k in range(x + 2, x + w - 2, 3):
            self.window(k, y - h, 2, 2, group=self.groups[0])
        self.rect(x + 1, y, x + 3, y, INK)
        self.rect(x + w - 4, y, x + w - 2, y, INK)
        for k in range(1, 4):                                           # the pantograph up to the wire
            self.px(x + w // 2 - (k & 1), y - h - k, INK)

    def wire(self, a, b, ya, yb, sag):
        for x in range(a, b + 1):
            t = (x - a) / max(1, b - a)
            y = int(round(ya + (yb - ya) * t + sag * math.sin(math.pi * t)))
            if not is_win(self.get(x, y)):
                self.px(x, y, POST)

    def pole(self, px):
        self.rect(px, 104, px, 145, POST)
        self.rect(px - 3, 107, px + 3, 107, POST)
        self.wire(X(86), X(168), 145 - L(7) - 4, 145 - L(7) - 4, 0)     # the tram wire

    def lamp(self, x, top=138):
        self.rect(x, top + 1, x, 145, POST)
        self.px(x, top, (3, 0, 4))
        self.px(x, top - 1, POST)
        self.lamps.append((x, top))

    def quay(self):
        self.rect(0, 146, SHORE - 6, 150, QUAY)
        self.rect(0, 146, SHORE - 6, 146, QUAY3)
        for x in range(0, SHORE - 6, 6):                                 # stones
            self.px(x, 148, QUAY2)
            self.px(x + 3, 149, QUAY2)
        self.rect(0, 150, SHORE - 6, 150, QUAY2)
        self.rect(SHORE - 6, 147, SHORE + 9, 148, QUAY2)                 # the jetty into the sea
        self.rect(SHORE - 6, 146, SHORE + 9, 146, QUAY3)
        for k in (SHORE - 1, SHORE + 4, SHORE + 9):
            self.rect(k, 149, k, 150, POST)

    def grass(self):
        for x in range(0, SHORE - 6):
            if self.r.random() < 0.55:
                h = self.r.choice((1, 1, 2))
                c = self.r.choice((GREEN, GREEN2, GREEN))
                for y in range(146 - h, 146):
                    if not is_win(self.get(x, y)):
                        self.px(x, y, c)
            if self.r.random() < 0.07:
                self.px(x, 144, self.r.choice((PINK3, (236, 210, 120), (230, 230, 220))))

    # ---------------------------------------------------------------- island
    def island(self):
        cx, y = ISLE_X, HZ
        for x in range(cx - 13, cx + 14):                                # the sand, low on the horizon
            h = int(3.4 * math.sqrt(max(0.0, 1 - ((x - cx) / 13.5) ** 2)))
            for yy in range(y - h, y + 1):
                self.px(x, yy, (4, 0 if yy > y - h else 1, 5))
        pts = [(cx + 5, y - 3), (cx + 6, y - 5), (cx + 6, y - 8), (cx + 5, y - 11), (cx + 4, y - 13), (cx + 2, y - 15)]
        for (a, b) in pts:                                               # the curved palm
            self.px(a, b, (4, 2, 5))
        tx, ty = pts[-1]
        for k, (dx, dy) in enumerate(((-1, 0), (-2, 0), (-3, 1), (-4, 2), (-5, 3), (1, -1), (2, -1), (3, 0), (4, 1),
                                      (5, 2), (0, -1), (-1, -1), (-2, -1), (1, 0), (2, 1), (-3, 0), (6, 3))):
            self.px(tx + dx, ty + dy, (4, 3 if k % 3 else 4, 5))
        for (a, b) in ((-7, -4), (-6, -4), (-5, -4), (-4, -4), (-6, -3), (-5, -3), (-6, -2), (-5, -2)):
            self.px(cx + a, y + b, (4, 5, 5))                            # a small hut
        self.px(cx - 3, y - 3, (4, 6, 5))                                # its lantern


ISLE_RGB = [(214, 196, 156), (194, 174, 138), (112, 86, 62), (72, 106, 74), (96, 132, 88), (150, 104, 82),
            (130, 118, 100), (0, 0, 0)]


def build():
    p = Pic()
    p.hills()
    p.rect(SHORE - 6, HZ + 1, W - 1, Q, WATK)          # the sea right of the shore, up to the horizon
    p.island()

    def house(x, w, h, *a, **k):                         # in the old 1x plan, drawn at the zoom
        x0 = X(x)
        p.house(x0, X(x + w) - x0, L(h), *a, **k)
    house(0, 12, 34, BRICK, "flat", NAVY, shade=BRICK2)
    house(12, 11, 26, CREAM, "gable", NAVY, shade=CREAM2)
    house(23, 9, 31, BRICK3, "step", NAVY, shade=BRICK)
    house(42, 13, 38, CREAM, "flat", NAVY, shade=CREAM2)
    house(55, 8, 24, BLUEG, "gable", NAVY)
    house(63, 12, 29, BRICK, "flat", NAVY2, shade=BRICK2)
    p.modern(X(86), X(118) - X(86), L(22))
    house(124, 10, 25, CREAM, "gable", NAVY, shade=CREAM2)
    house(134, 11, 32, BRICK2, "flat", NAVY, shade=BRICK)
    house(145, 9, 21, SAGE, "gable", NAVY2)
    house(154, 10, 27, CREAM2, "step", NAVY)
    p.rect(X(166), 145 - L(8), X(175), 145, CREAM)                       # the kiosk and its awning
    for x in range(X(165), X(177)):
        p.px(x, 145 - L(9), TRAM_R if (x // 2) % 2 else CREAM)
    p.window(X(168), 145 - L(5), 6, 2)
    p.pole(X(78))
    p.tram(X(94))
    p.tree(X(37), 145, L(5), (PINK3, PINK, PINK2))
    p.tree(X(80), 145, L(4), (GREEN2, GREEN, GREEN3))
    p.tree(X(120), 145, L(5), (PINK3, PINK, PINK2))
    p.tree(X(179), 145, L(4), (PINK3, PINK, PINK2))
    p.grass()
    p.quay()
    for x in (30, 58, 84, 116, 142, 162):
        p.lamp(X(x))
    p.lamp(SHORE + 9, 141)
    return p


DRAWN = False      # the town as an illustration: ink outlines, a little broken, and a watercolour wash


def drawn(p):
    """ink and wash over the plain colours (markers untouched): a slow noise lightens and darkens the
    colours in three steps (the wash), and every edge against the sky or a much darker or lighter
    neighbour gets an ink line, broken here and there (the hand)"""
    im = p.im
    src = im.copy()
    r = random.Random(5)
    def plain(c):
        return c not in (SKYK, WATK) and not is_win(c) and c[0] not in (3, 4)
    def lum(c):
        return c[0] * 3 + c[1] * 6 + c[2]
    def wash(x, y):
        v = math.sin(x * 0.21 + math.sin(y * 0.17) * 2) + math.sin(y * 0.29 + x * 0.05) * 0.8
        return -1 if v < -0.7 else 1 if v > 0.9 else 0
    ink = (44, 36, 40)
    for y in range(ROWS):
        for x in range(W):
            c = src.getpixel((x, y))
            if not plain(c):
                continue
            k = wash(x, y)
            c2 = tuple(max(0, min(255, int(v * (1 + 0.06 * k)))) for v in c)
            edge = False
            for dx, dy in ((0, -1), (-1, 0), (1, 0)):
                if 0 <= x + dx < W and 0 <= y + dy < ROWS:
                    n = src.getpixel((x + dx, y + dy))
                    if n == SKYK or (plain(n) and abs(lum(n) - lum(c)) > 420 and lum(c) < lum(n)):
                        edge = True
            if edge and r.random() < 0.82 and y < 146:
                c2 = mix(c2, ink, 0.7)
            im.putpixel((x, y), c2)


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def index_image(p):
    pal, look, rows = [], {}, []
    for y in range(ROWS):
        row = []
        for x in range(W):
            c = p.im.getpixel((x, y))
            if c == SKYK:
                v = I_SKY
            elif c == WATK:
                v = I_WATER
            elif is_win(c):
                v = I_WIN + c[1]
            elif c == (3, 0, 4):
                v = I_LAMP
            elif c[0] == 4 and c[2] == 5:
                v = I_ISLE + c[1]
            else:
                if c not in look:
                    look[c] = len(pal)
                    pal.append(c)
                v = look[c]
            row.append(v)
        rows.append(row)
    assert len(pal) <= I_ISLE, len(pal)
    return pal, rows


def rle(row):
    out, i = [], 0
    while i < len(row):
        j = i
        while j < len(row) and row[j] == row[i] and j - i < 127:
            j += 1
        if j - i >= 3:
            out += [j - i, row[i]]
            i = j
            continue
        lit, k = [], i
        while k < len(row) and len(lit) < 127:
            m = k
            while m < len(row) and row[m] == row[k] and m - k < 3:
                m += 1
            if m - k >= 3:
                break
            lit.append(row[k])
            k += 1
        out += [128 + len(lit)] + lit
        i = k
    return out


def unrle(data):
    out, i = [], 0
    while len(out) < W:
        t = data[i]
        if t < 128:
            out += [data[i + 1]] * t
            i += 2
        else:
            out += data[i + 1:i + 1 + t - 128]
            i += 1 + t - 128
    return out


def carr(name, ctype, vals, per=24):
    s = f"static const {ctype} {name}[{len(vals)}] = {{\n"
    for i in range(0, len(vals), per):
        s += "    " + ", ".join(str(v) for v in vals[i:i + per]) + ",\n"
    return s + "};\n"


def main():
    p = build()
    if DRAWN:
        drawn(p)
    pal, rows = index_image(p)
    data, off = [], []
    for r in rows:
        off.append(len(data))
        e = rle(r)
        assert unrle(e) == r
        data += e
    rr = random.Random(7)
    wlit, wthr = [], []
    for g in range(NWIN):           # the lit colour of each group: mostly warm, a few cool screens
        if rr.random() < 0.16:
            c = rr.choice(((196, 210, 236), (178, 196, 230)))
        else:
            c = rr.choice(((246, 206, 128), (240, 190, 110), (250, 218, 150), (236, 176, 104)))
        wlit += list(c)
        wthr.append(0 if g == p.groups[0] else rr.randrange(10, 250))
    lamps = [v for xy in p.lamps for v in xy]
    out = Path(__file__).resolve().parents[1] / "firmware/src/mt_city_data.h"
    s = ("/* SPDX-License-Identifier: GPL-3.0-only\n * Copyright (C) 2026 Alexis Arakis (Bungle Society) */\n"
         "/* generated by tools/gen_city.py: the far shore of Modular Town (rows 0..MT_Q, palette + RLE per row:\n"
         " * byte < 128 = run (count, value), 128 + n = n literals). Do not edit. */\n")
    s += f"#define MT_Q {Q}\n#define MT_HZ {HZ}\n#define MT_SHORE {SHORE}\n#define MT_ROWS {ROWS}\n"
    s += (f"#define MT_I_ISLE {I_ISLE}\n#define MT_I_LAMP {I_LAMP}\n#define MT_I_WIN {I_WIN}\n#define MT_NWIN {NWIN}\n"
          f"#define MT_I_WATER {I_WATER}\n#define MT_I_SKY {I_SKY}\n#define MT_NPAL {len(pal)}\n"
          f"#define MT_NLAMP {len(p.lamps)}\n#define MT_TRAM_WIN {p.groups[0]}\n#define MT_ISLE_X {ISLE_X}\n")
    s += carr("MT_PAL", "uint8_t", [v for c in pal for v in c])
    s += carr("MT_ISLE_PAL", "uint8_t", [v for c in ISLE_RGB for v in c])
    s += carr("MT_WIN_LIT", "uint8_t", wlit)
    s += carr("MT_WIN_THR", "uint8_t", wthr)
    s += carr("MT_LAMPS", "uint8_t", lamps)
    s += carr("MT_ROW", "uint16_t", off, 16)
    s += carr("MT_RLE", "uint8_t", data, 32)
    out.write_text(s)
    print(f"city: {len(pal)} colours, {len(p.lamps)} lamps, RLE {len(data)} B -> {out}")
    if len(sys.argv) > 1:
        d = Path(sys.argv[1])
        d.mkdir(parents=True, exist_ok=True)
        p.im.resize((W * 3, ROWS * 3), Image.NEAREST).save(d / "far_shore.png")


if __name__ == "__main__":
    main()
