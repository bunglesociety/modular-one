#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular One: mock-ups of the new FM-1 interface (240 x 240), for review before the firmware.

One visual per page, the four knobs always in the same four colours (sky blue, sage, cream,
module orange) and always in the same strip at the bottom; the page graphic colours what each
knob moves with that knob's colour. Fonts (OFL): Space Grotesk (names), DM Mono (values),
Silkscreen (labels).
   python3 tools/m1_mockup.py OUT_DIR"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W = H = 240
UI = Path(__file__).resolve().parents[1] / "assets" / "fonts" / "ui"

BG = (14, 13, 17)
PANEL = (24, 23, 29)
LINE = (46, 44, 54)
DIM = (92, 88, 100)
GREY = (150, 144, 150)
TEXT = (238, 232, 220)
K = [(110, 168, 236), (138, 200, 130), (240, 226, 196), (240, 146, 70)]   # knob 1..4
REC = (232, 82, 70)
FAM = [("NAPPES", "PAD", (170, 150, 230)), ("CORDES", "STR", (230, 186, 110)), ("CLOCHES", "BEL", (120, 210, 220)),
       ("KEYS", "KEY", (236, 222, 190)), ("BASSES", "BAS", (240, 120, 80)), ("LEADS", "LEA", (240, 140, 170)),
       ("PLUCKS", "PLK", (150, 210, 120)), ("TEXTURES", "TEX", (150, 160, 200))]


def font(name, px):
    return ImageFont.truetype(str(UI / name), px)


F_LAB = font("silkscreen-latin-400-normal.woff", 8)
F_VAL = font("dm-mono-latin-500-normal.woff", 13)
F_VALS = font("dm-mono-latin-500-normal.woff", 11)
F_NAME = font("space-grotesk-latin-700-normal.woff", 26)
F_MID = font("space-grotesk-latin-500-normal.woff", 15)
F_SM = font("space-grotesk-latin-500-normal.woff", 12)


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


class Screen:
    def __init__(self):
        self.im = Image.new("RGB", (W, H), BG)
        self.d = ImageDraw.Draw(self.im)

    def text(self, x, y, s, f, c, anchor="la"):
        self.d.text((x, y), s, font=f, fill=c, anchor=anchor)

    def top(self, track, page, bpm=120, playing=True, rec=False):
        d = self.d
        d.rounded_rectangle([4, 4, 17, 15], 3, fill=K[track - 1] if track <= 3 else GREY)
        self.text(10.5, 10, str(track), F_LAB, BG, "mm")
        self.text(23, 6, page, F_LAB, TEXT)
        if playing:
            d.polygon([(170, 5), (170, 14), (177, 9.5)], fill=TEXT)
        else:
            d.rectangle([170, 6, 176, 13], fill=DIM)
        if rec:
            d.ellipse([181, 6, 188, 13], fill=REC)
        self.text(214, 4, f"{bpm}", F_VALS, GREY, "ra")
        d.rectangle([219, 6, 233, 13], outline=DIM)
        d.rectangle([234, 8, 235, 11], fill=DIM)
        d.rectangle([221, 8, 229, 11], fill=GREY)
        d.line([0, 19, 239, 19], fill=LINE)

    def knobs(self, cells, hot=None):
        d = self.d
        d.line([0, 190, 239, 190], fill=LINE)
        for i, (lab, val, ratio) in enumerate(cells):
            x = i * 60
            if i:
                d.line([x, 196, x, 234], fill=LINE)
            c = K[i]
            d.ellipse([x + 6, 197, x + 10, 201], fill=c)
            self.text(x + 14, 196, lab, F_LAB, GREY if hot != i else TEXT)
            self.text(x + 6, 207, val, F_VAL, c if hot != i else TEXT)
            if ratio is not None:
                d.line([x + 6, 231, x + 53, 231], fill=LINE)
                d.line([x + 6, 231, x + 6 + int(47 * ratio), 231], fill=c, width=1)
                d.rectangle([x + 5 + int(47 * ratio), 229, x + 6 + int(47 * ratio), 233], fill=c)


def fm_wave(n, r1=1, r2=3, i1=2.2, i2=0.8, fb=0.4, cyc=2):
    out, f = [], 0.0
    for k in range(n):
        t = k / n * cyc
        m1 = math.sin(2 * math.pi * r1 * t + fb * f)
        f = m1
        m2 = math.sin(2 * math.pi * r2 * t * 1.0)
        out.append(math.sin(2 * math.pi * t + i1 * m1 + i2 * m2))
    return out


def wave(s, y0, amp, pts, col, fill=True, x0=0, x1=239):
    d = s.d
    n = x1 - x0 + 1
    prev = None
    for k in range(n):
        v = pts[int(k * len(pts) / n)]
        y = int(y0 - v * amp)
        if fill:
            a, b = sorted((y, y0))
            for yy in range(a, b + 1):
                t = abs(yy - y0) / max(1, amp)
                s.im.putpixel((x0 + k, yy), mix(BG, col, 0.10 + 0.35 * t))
        if prev:
            d.line([prev, (x0 + k, y)], fill=col)
        prev = (x0 + k, y)


# --------------------------------------------------------------------- screens
def splash():
    s = Screen()
    d = s.d
    # a low sun over a line of water: the Modular Town sunset, reduced to signs
    cx, cy = 120, 104
    for r in range(30, 0, -1):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=mix(BG, K[3], 0.15 + 0.85 * (1 - r / 30) ** 0.6))
    d.rectangle([60, cy, 180, cy + 34], fill=BG)
    for k, y in enumerate(range(cy + 3, cy + 30, 5)):
        w = 50 - k * 7
        d.line([cx - w, y, cx + w, y], fill=mix(BG, K[3], 0.6 - k * 0.1))
    d.line([40, cy, 200, cy], fill=TEXT)
    s.text(120, 150, "MODULAR ONE", font("space-grotesk-latin-700-normal.woff", 24), TEXT, "mm")
    s.text(120, 174, "BY BUNGLE SOCIETY", F_LAB, GREY, "mm")
    return s


def home():
    s = Screen()
    s.top(1, "HOME")
    fam = FAM[2]
    s.d.rounded_rectangle([6, 26, 36, 37], 3, fill=fam[2])
    s.text(21, 32, fam[1], F_LAB, BG, "mm")
    s.text(42, 27, "3 / 8", F_LAB, GREY)
    for v in range(8):                              # the voices sounding
        s.d.rectangle([186 + v * 6, 28, 189 + v * 6, 35], fill=K[0] if v < 3 else LINE)
    s.text(6, 44, "VERRE", F_NAME, TEXT)
    s.text(7, 74, "cloches de verre, longue queue", F_SM, GREY)
    pts = fm_wave(400, 1, 3.5, 2.6, 1.1, 0.2, 3)
    wave(s, 140, 38, pts, K[0])
    s.knobs([("BRIL", "90%", 0.71), ("MONT", "10%", 0.08), ("SUS", "31%", 0.31), ("REL", "4.2s", 0.85)])
    return s


def fm_page():
    s = Screen()
    s.top(1, "FM")
    d = s.d
    # operators: M2 -> C <- M1 (with its feedback loop)
    ops = {"M2": (46, 62), "M1": (120, 62), "C": (120, 128)}
    for (a, b, c, wdt) in (("M2", "C", K[3], 2), ("M1", "C", K[0], 3)):
        (x0, y0), (x1, y1) = ops[a], ops[b]
        d.line([x0, y0, x1, y1], fill=c, width=wdt)
    x, y = ops["M1"]
    d.arc([x + 6, y - 28, x + 34, y + 2], 200, 520 - 360 + 160, fill=K[1], width=2)    # feedback
    d.polygon([(x + 12, y - 4), (x + 18, y - 9), (x + 19, y - 1)], fill=K[1])
    for name, (x, y) in ops.items():
        d.ellipse([x - 15, y - 15, x + 15, y + 15], fill=PANEL, outline=TEXT if name == "C" else GREY)
        s.text(x, y - 3, name, F_LAB, GREY, "mm")
        r = {"M2": "3.5", "M1": "1", "C": "1"}[name]
        s.text(x, y + 6, r, F_VALS, K[2] if name != "C" else TEXT, "mm")
    s.text(46, 86, "+7 st", F_VALS, K[3], "ma")
    d.line([120, 143, 120, 152], fill=TEXT)
    # one cycle of what comes out
    pts = fm_wave(240, 1, 3.5, 2.6, 1.1, 0.25, 1)
    wave(s, 170, 14, pts, K[0], x0=160, x1=232)
    d.rectangle([158, 152, 234, 186], outline=LINE)
    s.text(162, 154, "1 CYCLE", F_LAB, DIM)
    s.text(6, 154, "MODULATION", F_LAB, DIM)
    for i, (lab, v) in enumerate((("INDEX", 0.71), ("FEEDBACK", 0.12))):
        y = 166 + i * 11
        s.text(6, y, lab, F_LAB, GREY)
        d.rectangle([62, y + 2, 62 + 80, y + 5], fill=LINE)
        d.rectangle([62, y + 2, 62 + int(80 * v), y + 5], fill=K[i])
    s.knobs([("BRIL", "90%", 0.71), ("GRAIN", "5%", 0.04), ("TIMB", "1:3.5", 8 / 11), ("CLOCH", "+7", 0.6)], hot=0)
    return s


def env_page():
    s = Screen()
    s.top(1, "ENV")
    d = s.d
    top, bot = 36, 172
    a, dec, sus, rel = 0.02, 0.45, 0.31, 0.75
    xa, xd, xs, xr = 8, 8 + 10, 18 + 70, 150
    ysus = bot - sus * (bot - top)
    segs = [(K[0], [(xa, bot), (xd, top)]),
            (K[1], [(xd, top)] + [(xd + k, ysus + (top - ysus) * math.exp(-k / 18)) for k in range(0, 71, 2)]),
            (K[2], [(xs, ysus), (xr, ysus)]),
            (K[3], [(xr, ysus)] + [(xr + k, bot - (bot - ysus) * math.exp(-k / 26)) for k in range(0, 82, 2)])]
    for col, pts in segs:
        poly = pts + [(pts[-1][0], bot), (pts[0][0], bot)]
        d.polygon(poly, fill=mix(BG, col, 0.22))
        d.line(pts, fill=col, width=2)
    d.line([4, bot + 1, 236, bot + 1], fill=LINE)
    for x, lab in ((xa, "A"), (xd, "D"), (xs, "S"), (xr, "R")):
        d.line([x, top - 6, x, bot], fill=LINE)
        s.text(x + 2, 24, lab, F_LAB, DIM)
    s.d.ellipse([xd + 30, ysus + 16, xd + 34, ysus + 20], fill=TEXT)          # the note playing
    s.knobs([("ATK", "2ms", 0.0), ("DEC", "1.4s", 0.75), ("SUS", "31%", 0.31), ("REL", "4.2s", 0.85)], hot=3)
    return s


def fx_page():
    s = Screen()
    s.top(1, "FX")
    d = s.d
    vals = [0.0, 0.0, 0.28, 0.45]
    names = ["DIST", "CHORUS", "DELAY", "REVERB"]
    for i in range(4):
        x0, x1, y0, y1 = i * 60 + 6, i * 60 + 54, 30, 178
        d.rounded_rectangle([x0, y0, x1, y1], 4, fill=PANEL)
        c = K[i]
        cx = (x0 + x1) // 2
        v = vals[i]
        if i == 0:      # the transfer curve
            prev = None
            for k in range(41):
                t = k / 40 * 2 - 1
                y = math.tanh(t * (1 + 6 * v)) / math.tanh(1 + 6 * v)
                p = (x0 + 4 + k, 104 - int(y * 30))
                if prev:
                    d.line([prev, p], fill=c if v else DIM)
                prev = p
        elif i == 1:    # two voices beating
            for ph, cc in ((0, c), (1.3, mix(c, BG, 0.5))):
                prev = None
                for k in range(41):
                    y = math.sin(k / 40 * 2 * math.pi * 1.5 + ph * (0.2 + v))
                    p = (x0 + 4 + k, 104 - int(y * 18))
                    if prev:
                        d.line([prev, p], fill=cc if v else DIM)
                    prev = p
        elif i == 2:    # echoes going down
            for k in range(7):
                h = int(70 * v * 2.6 * (0.62 ** k)) + 2
                x = x0 + 6 + k * 6
                d.rectangle([x, 150 - h, x + 3, 150], fill=mix(BG, c, 1 - k * 0.11))
        else:           # the tail
            for k in range(140):
                import random
                r = random.Random(k)
                xx = x0 + 4 + r.randrange(40)
                yy = 150 - int(abs(r.gauss(0, 1)) * 30 * v * 2)
                if yy > 40:
                    s.im.putpixel((xx, yy), mix(BG, c, 0.4 + 0.6 * r.random()))
        s.text(cx, 160, names[i], F_LAB, GREY, "mm")
        s.text(cx, 170, f"{int(v * 100)}%", F_VALS, c if v else DIM, "mm")
    s.knobs([("DIST", "0%", 0), ("CHOR", "0%", 0), ("DLY", "28%", 0.28), ("REV", "45%", 0.45)])
    return s


def presets_page():
    s = Screen()
    s.top(1, "PRESETS")
    d = s.d
    sel = 2
    for i, (_, code, col) in enumerate(FAM):
        x = 4 + i * 29
        if i == sel:
            d.rounded_rectangle([x, 25, x + 27, 37], 3, fill=col)
            s.text(x + 14, 31, code, F_LAB, BG, "mm")
        else:
            d.rounded_rectangle([x, 25, x + 27, 37], 3, outline=mix(BG, col, 0.6))
            s.text(x + 14, 31, code, F_LAB, mix(BG, col, 0.8), "mm")
    s.text(6, 44, "CLOCHES", F_LAB, FAM[sel][2])
    names = ["CLOCHES", "VERRE", "CARILLON", "CELESTA", "GAMELAN", "TUBULAR", "BOL", "GLACE"]
    for k, nm in enumerate(names):
        y = 56 + k * 16
        if k == 1:
            d.rectangle([0, y - 1, 239, y + 13], fill=PANEL)
            d.rectangle([0, y - 1, 2, y + 13], fill=FAM[sel][2])
        s.text(8, y, f"{k + 1}", F_VALS, DIM if k != 1 else FAM[sel][2])
        s.text(24, y - 1, nm, F_SM, TEXT if k == 1 else GREY)
    s.knobs([("FAM", "BEL", 2 / 7), ("SON", "2/8", 1 / 7), ("", "", None), ("", "", None)])
    return s


def seq_page():
    s = Screen()
    s.top(2, "SEQ", rec=True)
    d = s.d
    notes = [60, 0, 63, 67, 0, 70, 67, 0, 60, 0, 65, 0, 63, 62, 0, 58]
    play, cur = 5, 10
    for i, n in enumerate(notes):
        col, row = i % 8, i // 8
        x, y = 6 + col * 29, 30 + row * 76
        on = n > 0
        c = K[1]
        d.rounded_rectangle([x, y, x + 25, y + 66], 4, fill=PANEL if not on else mix(BG, c, 0.18),
                            outline=TEXT if i == cur else (LINE if not on else mix(BG, c, 0.6)))
        if on:
            hy = y + 60 - (n - 56) * 4
            d.rounded_rectangle([x + 4, hy, x + 21, hy + 4], 2, fill=c)
        if i == play:
            d.rectangle([x, y + 69, x + 25, y + 71], fill=K[3])
        s.text(x + 13, y + 54 if not on else y + 54, str(i + 1), F_LAB, DIM, "mm")
    s.knobs([("STEP", "11/16", 10 / 15), ("NOTE", "F4", 0.5), ("TIME", "NOTE", None), ("FLAG", "-", None)])
    return s


def espace_page():
    s = Screen()
    s.top(1, "ESPACE")
    d = s.d
    vals = [0.45, 0.8, 1, 0.3]
    for i in range(4):
        x0 = i * 60
        cx, cy = x0 + 30, 92
        c = K[i]
        v = vals[i]
        if i == 0:      # the rise: steps climbing
            for k in range(6):
                h = 8 + k * 9
                cc = c if k / 5 <= v + 0.01 else LINE
                d.rectangle([x0 + 8 + k * 7, 140 - h, x0 + 12 + k * 7, 140], fill=cc)
        elif i == 1:    # the moon: night
            d.ellipse([cx - 20, cy - 20, cx + 20, cy + 20], fill=c)
            off = int(40 * (1 - v)) + 4
            d.ellipse([cx - 20 + off, cy - 24, cx + 20 + off, cy + 16], fill=BG)
        elif i == 2:    # the island
            d.ellipse([cx - 22, 128, cx + 22, 140], fill=c if v else LINE)
            d.line([cx + 4, 130, cx + 7, 104, cx + 2, 86], fill=c if v else LINE, width=2)
            for a in (-60, -20, 20, 60):
                d.line([cx + 2, 86, cx + 2 + int(16 * math.cos(math.radians(a + 180) if a < 0 else math.radians(a))),
                        86 + int(8 * abs(math.sin(math.radians(a))))], fill=c if v else LINE, width=2)
        else:           # the sub: an octave below
            prev = None
            for k in range(45):
                y = math.sin(k / 44 * 2 * math.pi)
                p = (x0 + 8 + k, cy + 30 - int(y * 26 * v))
                if prev:
                    d.line([prev, p], fill=c)
                prev = p
            prev = None
            for k in range(45):
                y = math.sin(k / 44 * 4 * math.pi)
                p = (x0 + 8 + k, cy - 20 - int(y * 10))
                if prev:
                    d.line([prev, p], fill=DIM)
                prev = p
    s.knobs([("MONT", "45%", 0.45), ("NUIT", "80%", 0.8), ("ILE", "ON", 1.0), ("SUB", "30%", 0.3)])
    return s


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "build/m1_mockup")
    out.mkdir(parents=True, exist_ok=True)
    screens = [("1_demarrage", splash()), ("2_home", home()), ("3_fm", fm_page()), ("4_espace", espace_page()),
               ("5_env", env_page()), ("6_fx", fx_page()), ("7_presets", presets_page()), ("8_seq", seq_page())]
    sheet = Image.new("RGB", (4 * 250 + 10, 2 * 250 + 10), (40, 40, 44))
    for i, (n, s) in enumerate(screens):
        s.im.save(out / f"{n}.png")
        sheet.paste(s.im, (10 + (i % 4) * 250, 10 + (i // 4) * 250))
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(out / "planche.png")
    print(out / "planche.png")


if __name__ == "__main__":
    main()
