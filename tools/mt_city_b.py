#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Town FM: direction B ("la fenetre") in the style of Modular Town V2, from its
screenshots: a dusty peach sky with grain, Dutch brick and cream clapboard houses with navy
cornices, a grey flat-roofed building with navy slab, rooftop units and an orange module of
knobs, the cream and yellow tram 81, utility poles, wires and string lights, tall grass and
wild flowers at the foot of the facades, the pavement, the reflection in the water. ILE
travels to the island: sand, a curved palm, Nick and the traveller sitting, the tram on the sea.
   python3 tools/mt_city_b.py OUT_DIR"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mt_city_mockup import H, W, font, mix  # noqa: E402
from mt_city_concepts import SPRITE_COL, SPRITES  # noqa: E402

INK = (40, 34, 30)
NAVY = (44, 52, 74)
NAVY2 = (60, 70, 96)
BRICK = (156, 74, 58)
BRICK2 = (130, 60, 48)
CREAM = (218, 208, 190)
CREAM2 = (196, 186, 168)
CONCRETE = (178, 172, 166)
CONCRETE2 = (156, 150, 146)
WALL = (172, 150, 138)
PANE = (70, 82, 98)
PANE_LIT = (232, 196, 128)
ORANGE = (214, 152, 66)
TRAM_Y = (222, 178, 70)
TRAM_C = (236, 228, 208)
TRAM_R = (196, 74, 58)
GRASS = (96, 122, 70)
GRASS2 = (124, 146, 86)
PAVE = (184, 174, 160)
PAVE2 = (162, 152, 140)
SAND = (226, 210, 168)
PALM = (88, 132, 84)
TRUNK = (138, 106, 70)
NIGHT = (30, 26, 56)


class B:
    def __init__(self, night=0.0, seed=4):
        self.n = night
        self.im = Image.new("RGB", (W, H))
        self.d = ImageDraw.Draw(self.im)
        self.f = font()
        self.r = random.Random(seed)

    def c(self, col, k=1.0):
        """darken towards the night, k < 1 for things that keep some light"""
        return mix(col, mix(col, NIGHT, 0.82), self.n * k)

    def rect(self, x0, y0, x1, y1, col, ol=None):
        self.d.rectangle([x0, y0, x1, y1], fill=self.c(col), outline=self.c(ol) if ol else None)

    def px(self, x, y, col):
        if 0 <= x < W and 0 <= y < H:
            self.im.putpixel((x, y), col)

    def grain(self, y0, y1, amt=7):
        for _ in range((y1 - y0) * 24):
            x, y = self.r.randrange(W), self.r.randrange(y0, y1)
            p = self.im.getpixel((x, y))
            k = self.r.randrange(-amt, amt + 1)
            self.im.putpixel((x, y), tuple(max(0, min(255, v + k)) for v in p))

    # ------------------------------------------------------------- sky
    def sky(self, y1, top, bot):
        for y in range(16, y1):
            self.d.line([0, y, W, y], fill=mix(top, bot, (y - 16) / (y1 - 16)))

    def town_sky(self, bril):
        top = mix((172, 152, 146), (22, 18, 44), self.n)
        bot = mix((214, 184, 160), (58, 40, 84), self.n)
        self.sky(170, top, bot)
        if self.n < 0.5:                                     # a hazy sun, brighter with BRIL
            cx, cy = 66, 52
            for r, a in ((26, 0.10), (19, 0.16), (13, 0.25)):
                glow = mix(bot, (240, 170, 110), a * (0.5 + bril))
                self.d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=glow)
        else:
            for _ in range(30):
                self.px(self.r.randrange(W), self.r.randrange(18, 100), (214, 210, 226))
            self.d.ellipse([196, 28, 204, 36], fill=(206, 210, 228))
            for r in (10, 7):
                pass
        for bx, by in ((150, 40), (164, 34), (120, 58)):     # birds, a soft V
            col = self.c(INK)
            self.px(bx - 2, by - 1, col); self.px(bx - 1, by, col); self.px(bx, by, col)
            self.px(bx + 1, by, col); self.px(bx + 2, by - 1, col)

    # ------------------------------------------------------- buildings
    def window(self, x, y, w, h, lit=False, grid=True):
        self.d.rectangle([x, y, x + w, y + h], fill=PANE_LIT if lit else self.c(PANE), outline=self.c(CREAM))
        if grid:
            self.d.line([x + w // 2, y, x + w // 2, y + h], fill=self.c(CREAM))
            self.d.line([x, y + h // 2, x + w, y + h // 2], fill=self.c(CREAM))

    def lit(self, sus):
        return self.n >= 0.5 and self.r.random() < 0.15 + 0.75 * sus

    def houses(self, sus):
        # red brick townhouse, cut by the left edge
        self.rect(0, 62, 30, 170, BRICK)
        self.rect(-2, 56, 32, 62, NAVY)
        self.rect(16, 48, 22, 56, NAVY)                       # chimney
        for y in (70, 96, 122):
            for x in (3, 17):
                self.window(x, y, 9, 13, self.lit(sus))
            self.d.line([1, y + 15, 29, y + 15], fill=self.c(INK))    # little balconies
        # cream clapboard house
        self.rect(30, 80, 76, 170, CREAM)
        for y in range(84, 150, 3):
            self.d.line([30, y, 76, y], fill=self.c(CREAM2))
        self.rect(28, 74, 78, 80, NAVY)
        self.rect(58, 64, 66, 74, NAVY)
        for y in (88, 116):
            for x in (36, 60):
                self.window(x, y, 11, 15, self.lit(sus))
        # the TERMINUS shop: dark sign, red and white awning, a shopfront
        self.rect(32, 140, 74, 146, (52, 40, 32))
        self.d.text((35, 136), "TERMINUS"[:5], font=self.f, fill=(0, 0, 0, 0)) if False else None
        for k in range(32, 74, 6):
            self.d.polygon([(k, 147), (k + 6, 147), (k + 5, 152), (k + 1, 152)],
                           fill=self.c((200, 70, 60) if (k // 6) % 2 else (236, 228, 214)))
        self.rect(34, 153, 72, 168, (60, 82, 70))
        self.rect(37, 155, 69, 166, PANE_LIT if self.n >= 0.5 else (214, 206, 186))

    def back_wall(self):
        self.rect(78, 98, 172, 170, WALL)
        for y in range(102, 168, 5):                          # the dotted warehouse wall
            for x in range(81 + (y // 5 % 2) * 3, 170, 6):
                self.px(x, y, self.c(mix(WALL, INK, 0.25)))
        self.rect(128, 80, 132, 98, mix(WALL, INK, 0.1))     # a far chimney

    def modern(self, vals, sus):
        x0, x1 = 168, 240
        self.rect(x0, 104, x1, 170, CONCRETE)
        self.rect(x0 - 4, 98, x1, 104, NAVY)                  # the navy slab
        self.rect(x0 + 10, 88, x0 + 26, 98, NAVY2)            # rooftop units
        self.rect(x0 + 40, 90, x0 + 52, 98, NAVY2)
        self.d.line([x0 + 60, 98, x0 + 60, 82], fill=self.c(INK))
        self.px(x0 + 60, 81, (200, 60, 50))                   # the little red light
        for x in range(x0 + 4, x1, 16):                       # panel seams
            self.d.line([x, 104, x, 150], fill=self.c(CONCRETE2))
        # the orange module with the four knobs of the page
        mx0, my0 = x0 + 6, 110
        self.rect(mx0, my0, mx0 + 58, my0 + 30, ORANGE, INK)
        for i, v in enumerate(vals):
            kx, ky = mx0 + 8 + i * 14, my0 + 15
            self.d.ellipse([kx - 6, ky - 6, kx + 6, ky + 6], fill=self.c((150, 104, 44)))
            self.d.ellipse([kx - 5, ky - 5, kx + 5, ky + 5], fill=(26, 24, 22), outline=(8, 8, 8))
            a = -math.pi * 0.75 + v * math.pi * 1.5 - math.pi / 2
            self.d.line([kx, ky, kx + math.cos(a) * 4, ky + math.sin(a) * 4], fill=(236, 232, 222))
        # the ground floor: a café with stools, string lights over it
        self.rect(x0, 150, x1, 170, (226, 222, 214))
        self.rect(x0, 148, x1, 150, NAVY)
        self.rect(x0 + 4, 153, x1 - 30, 164, PANE_LIT if self.n >= 0.5 else (204, 200, 192))
        self.rect(x1 - 24, 152, x1 - 12, 170, (72, 140, 150))  # the teal door

    def pole_and_wires(self):
        px = 156
        self.d.line([px, 170, px, 60], fill=self.c(INK), width=2)
        self.d.line([px - 8, 66, px + 8, 66], fill=self.c(INK))
        self.d.line([px - 6, 72, px + 6, 72], fill=self.c(INK))
        for y0, y1 in ((66, 40), (72, 52)):                   # wires, sagging
            pts = [(px, y0)] + [(px + t, y0 + int(4 * math.sin(math.pi * t / 84))) for t in range(0, 85, 6)]
            self.d.line(pts, fill=self.c(INK))
            pts = [(px, y0)] + [(px - t, y0 - int((y0 - y1) * t / 156) + int(5 * math.sin(math.pi * t / 156)))
                                for t in range(0, 157, 6)]
            self.d.line(pts, fill=self.c(INK))
        # tram catenary over the street
        self.d.line([84, 128, 168, 128], fill=self.c(INK))
        for k, x in enumerate(range(176, 236, 8)):            # string lights over the café
            y = 146 - int(3 * math.sin(math.pi * (x - 176) / 60))
            lamp = (250, 210, 130) if self.n >= 0.5 else self.c((230, 200, 150))
            self.px(x, y, lamp)
            if self.n >= 0.5:
                self.px(x, y + 1, (200, 150, 90))

    def tram(self, x, y, on_sea=False):
        """the 81, cream above, yellow below, a red line, dark doors; y = the rails"""
        x1 = x + 74
        self.d.rounded_rectangle([x, y - 26, x1, y - 2], 4, fill=self.c(TRAM_Y, 0.7), outline=self.c(INK))
        self.d.rectangle([x + 1, y - 25, x1 - 1, y - 15], fill=self.c(TRAM_C, 0.7))
        self.d.line([x + 1, y - 14, x1 - 1, y - 14], fill=self.c(TRAM_R))
        lit = self.n >= 0.5
        for k in range(x + 4, x1 - 6, 8):
            self.d.rectangle([k, y - 23, k + 5, y - 17], fill=PANE_LIT if lit else self.c((92, 100, 110)))
        for dx in (18, 52):
            self.d.rectangle([x + dx, y - 22, x + dx + 6, y - 3], fill=self.c((52, 46, 42)))
        self.d.rectangle([x1 - 12, y - 29, x1 - 4, y - 26], fill=(30, 28, 26))
        self.d.text((x1 - 12, y - 33), "81"[:0], font=self.f)
        self.px(x1 - 9, y - 28, (230, 190, 90)); self.px(x1 - 7, y - 28, (230, 190, 90))
        if not on_sea:
            self.d.line([x + 30, y - 26, x + 40, y - 40, x + 30, y - 52], fill=self.c(INK))  # pantograph
            self.d.line([x + 40, y - 40, x + 30, y - 26], fill=self.c(INK))

    def grass(self, y0=160, y1=172):
        for x in range(0, W, 1):
            h = self.r.randrange(4, 12)
            col = GRASS if self.r.random() < 0.6 else GRASS2
            self.d.line([x, y1, x + self.r.choice((-1, 0, 1)), y1 - h], fill=self.c(col))
        for _ in range(26):                                   # wild flowers: poppies, daisies, cornflowers
            x, y = self.r.randrange(2, W - 2), self.r.randrange(y0 - 4, y1 - 4)
            col = self.r.choice(((200, 70, 54), (236, 232, 214), (222, 196, 90), (110, 120, 190)))
            self.d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=self.c(col))
            self.px(x, y, self.c((90, 60, 40)))

    def pavement(self):
        self.rect(0, 172, W, 182, PAVE)
        for x in range(0, W, 10):
            self.d.line([x, 172, x, 182], fill=self.c(PAVE2))
        self.d.line([0, 182, W, 182], fill=self.c(PAVE2))

    def sprite(self, name, x, y, flip=False, sit=False):
        rows = SPRITES[name]
        for j, row in enumerate(rows):
            for i, ch in enumerate(row[::-1] if flip else row):
                if ch != ".":
                    col = SPRITE_COL[ch]
                    self.px(x + i, y - len(rows) + j, col if ch in "ke" else self.c(col))

    def water(self, y0, reflect_from, base):
        """the reflection: the band above, flipped, washed into the water, with ripples"""
        src = self.im.crop((0, reflect_from, W, y0)).transpose(Image.FLIP_TOP_BOTTOM)
        hgt = min(src.height, H - 18 - y0)
        for y in range(hgt):
            for x in range(W):
                p = src.getpixel((x, y))
                k = 0.42 - 0.3 * y / max(1, hgt)
                self.im.putpixel((x, y0 + y), mix(self.c(base), p, max(0.08, k)))
        for y in range(y0 + hgt, H - 18):
            self.d.line([0, y, W, y], fill=self.c(base))

    def ripples(self, y0, y1, rel, base):
        hl = mix(self.c(base), (236, 232, 222), 0.30)
        for _ in range(int(6 + 18 * rel)):                    # REL: the ripples linger
            x, y = self.r.randrange(0, W - 12), self.r.randrange(y0 + 2, y1)
            self.d.line([x, y, x + 4 + int(10 * rel), y], fill=hl)

    def petals(self, n=8):
        for _ in range(n):
            x, y = self.r.randrange(2, W - 4), self.r.randrange(20, 200)
            self.d.rectangle([x, y, x + 1, y], fill=self.c((236, 170, 150)))

    # ------------------------------------------------------- header, strip
    def header(self, right):
        band = mix((228, 220, 204), (34, 30, 58), self.n)
        tcol = INK if self.n < 0.5 else (228, 222, 206)
        self.d.rectangle([0, 0, W, 15], fill=band)
        self.d.line([0, 15, W, 15], fill=mix(band, INK, 0.5))
        self.d.text((4, 0), "120  T1", font=self.f, fill=tcol)
        self.d.text((W - 4 - 8 * len(right), 0), right, font=self.f, fill=tcol)

    def strip(self, items, hot=-1):
        band = mix((228, 220, 204), (34, 30, 58), self.n)
        tcol = INK if self.n < 0.5 else (228, 222, 206)
        self.d.rectangle([0, H - 18, W, H], fill=band)
        self.d.line([0, H - 18, W, H - 18], fill=mix(band, INK, 0.5))
        for i, (lab, v) in enumerate(items):
            col = (196, 74, 58) if i == hot else tcol
            self.d.text((4 + i * 60, H - 17), f"{lab} {v}"[:7], font=self.f, fill=col)


def town(night, vals, items, preset, hot=-1, seed=4):
    bril, mont, sus, rel = vals
    s = B(night, seed)
    s.town_sky(bril)
    s.grain(16, 170)
    s.back_wall()
    s.houses(sus)
    s.modern(vals, sus)
    s.pole_and_wires()
    s.tram(88, 170)
    s.d.line([0, 170, W, 170], fill=s.c(INK))                 # the rails
    s.grass()
    s.pavement()
    s.sprite("traveller", 20, 182)
    s.sprite("cat", 96, 182)
    s.sprite("woman", 128, 182, flip=True)
    s.water(183, 130, (150, 154, 150))
    s.ripples(184, H - 20, rel, (150, 154, 150))
    s.petals()
    s.header(preset)
    s.strip(items, hot)
    return s.im


def island(night, vals, items, preset, seed=8):
    bril, mont, sus, rel = vals
    s = B(night, seed)
    top = mix((176, 160, 152), (18, 14, 40), night)
    bot = mix((236, 162, 108), (70, 34, 92), night)
    s.sky(134, top, bot)
    s.grain(16, 134, 6)
    if night >= 0.5:                                          # stars and shooting stars
        for _ in range(46):
            s.px(s.r.randrange(W), s.r.randrange(18, 130), (226, 224, 236))
        for _ in range(int(3 + 6 * mont)):                    # more with the Montee
            x, y = s.r.randrange(10, W - 30), s.r.randrange(20, 100)
            for t in range(10):
                s.px(x + t, y - t // 2, mix((90, 80, 140), (250, 250, 250), t / 9))
        s.d.ellipse([140, 26, 148, 34], fill=(204, 208, 226))
    else:
        for bx, by in ((60, 60), (130, 84)):
            s.px(bx - 3, by, INK); s.px(bx - 2, by, INK); s.px(bx - 1, by + 1, INK)
            s.px(bx + 1, by + 1, INK); s.px(bx + 2, by, INK); s.px(bx + 3, by, INK)
    sea = mix((146, 156, 166), (36, 40, 72), night)
    s.d.rectangle([0, 134, W, H - 18], fill=sea)
    s.d.line([0, 134, W, 134], fill=mix(sea, (240, 220, 200), 0.35))
    if night >= 0.5:                                          # the far island with its light
        s.d.ellipse([196, 131, 222, 136], fill=s.c(SAND, 0.6))
        s.px(206, 132, (250, 210, 120))
    # the tram on the sea, at the left
    s.tram(6, 160, on_sea=True)
    s.d.line([80, 159, 110, 160], fill=s.c(mix(sea, INK, 0.4)))     # its rails, under water
    # the island: sand, tufts of grass, the curved palm
    s.d.ellipse([92, 150, 216, 166], fill=s.c(SAND, 0.75), outline=s.c(mix(SAND, INK, 0.3), 0.75))
    for gx in (108, 114, 186, 194):
        s.d.line([gx, 156, gx - 1, 151], fill=s.c(GRASS))
        s.d.line([gx + 2, 156, gx + 3, 152], fill=s.c(GRASS))
    pts = [(176, 158), (178, 140), (179, 124), (178, 108), (174, 94), (168, 84)]
    s.d.line(pts, fill=s.c(TRUNK), width=3)
    tip = (168, 82)
    for ang, ln in ((200, 30), (225, 28), (250, 22), (300, 22), (325, 30), (345, 30), (170, 24)):
        a = math.radians(ang)
        ex, ey = tip[0] + math.cos(a) * ln, tip[1] + math.sin(a) * ln * 0.55 + 10
        s.d.line([tip, ((tip[0] + ex) / 2, tip[1] + math.sin(a) * ln * 0.15 - 2), (ex, ey)], fill=s.c(PALM), width=2)
        for t in range(2, 9):                                 # leaflets
            fx = tip[0] + (ex - tip[0]) * t / 9
            fy = tip[1] + (ey - tip[1]) * t / 9
            s.px(int(fx), int(fy) + 2, s.c(PALM))
    # Nick, white beard, sitting cross-legged; the traveller with his straw hat, sitting
    nx, ny = 150, 156
    s.d.ellipse([nx - 6, ny - 4, nx + 6, ny + 1], fill=s.c((232, 230, 224)))     # crossed legs
    s.d.rectangle([nx - 4, ny - 13, nx + 4, ny - 4], fill=s.c((236, 234, 228)))  # white robe
    s.d.ellipse([nx - 3, ny - 19, nx + 3, ny - 13], fill=s.c((222, 186, 152)))   # head
    s.d.rectangle([nx - 3, ny - 15, nx + 3, ny - 9], fill=s.c((246, 246, 242)))  # the beard
    s.d.line([nx - 3, ny - 17, nx + 3, ny - 17], fill=(30, 26, 24))              # dark glasses
    tx, ty = 124, 156
    s.d.rectangle([tx - 3, ty - 11, tx + 3, ty - 2], fill=s.c((78, 98, 134)))     # blue coat
    s.d.rectangle([tx - 4, ty - 3, tx + 5, ty], fill=s.c((58, 74, 104)))
    s.d.ellipse([tx - 2, ty - 16, tx + 2, ty - 11], fill=s.c((224, 190, 158)))
    s.d.polygon([(tx - 7, ty - 15), (tx, ty - 21), (tx + 7, ty - 15)], fill=s.c((198, 170, 110)))  # straw hat
    s.d.rectangle([tx + 4, ty - 5, tx + 7, ty - 1], fill=s.c((170, 94, 58)))      # the bundle
    # reflections and ripples
    s.water(167, 120, mix((146, 156, 166), (36, 40, 72), night) if False else (146, 156, 166))
    s.ripples(136, H - 20, rel, (146, 156, 166))
    s.header(preset)
    s.strip(items)
    return s.im


def main(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    home = (0.55, 0.25, 0.70, 0.80)
    items = [("BRI", "55"), ("MON", "25"), ("SUS", "70"), ("REL", "2.0")]
    scenes = {
        "B3_rue_jour": town(0.0, home, items, "PRINTEMP", hot=1),
        "B4_rue_nuit": town(1.0, (0.4, 0.3, 0.9, 0.6), [("BRI", "40"), ("MON", "30"), ("SUS", "90"), ("REL", "1.2")],
                            "NUIT"),
        "B5_ile_couchant": island(0.0, (0.5, 0.6, 0.8, 0.9), [("MON", "60"), ("NUI", "0"), ("ILE", "ON"), ("", "")],
                                  "L'ILE"),
        "B6_ile_nuit": island(1.0, (0.5, 0.9, 0.8, 0.9), [("MON", "90"), ("NUI", "100"), ("ILE", "ON"), ("", "")],
                              "L'ILE"),
    }
    for name, im in scenes.items():
        im.resize((W * 3, H * 3), Image.NEAREST).save(out / f"{name}.png")
    print(f"direction B in {out}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build/direction_b")
