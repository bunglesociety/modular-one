#!/usr/bin/env python3
"""Erik Satie, 1ere Gymnopedie (1888, public domain) for the demo mode.

Transcribed from the Mutopia Project LilyPond source (gymnopedie_1.ly, Evin Robertson, public domain,
from the Dover edition). Writes firmware/src/gymno_data.h: one event per note, in quarter notes
(3/4, 39 bars; bars 1..31 played twice, the first then the second ending).
"""
import os

N = {'c': 0, 'd': 2, 'e': 4, 'f': 5, 'g': 7, 'a': 9, 'b': 11}


def p(s):
    """'fis5' -> MIDI note"""
    acc = 1 if 'is' in s else 0
    return 12 * (int(s[-1]) + 1) + N[s[0]] + acc


# each bar: list of (beat, length in quarters, notes); beat 0..2
MEL, MID, BAS = {}, {}, {}


def add(d, bar, beat, ln, *ns):
    d.setdefault(bar, []).append((beat, ln, [p(n) for n in ns]))


# --- melody ---
add(MEL, 5, 1, 1, 'fis5'); add(MEL, 5, 2, 1, 'a5')
add(MEL, 6, 0, 1, 'g5'); add(MEL, 6, 1, 1, 'fis5'); add(MEL, 6, 2, 1, 'cis5')
add(MEL, 7, 0, 1, 'b4'); add(MEL, 7, 1, 1, 'cis5'); add(MEL, 7, 2, 1, 'd5')
add(MEL, 8, 0, 3, 'a4')
add(MEL, 9, 0, 12, 'fis4')
add(MEL, 13, 1, 1, 'fis5'); add(MEL, 13, 2, 1, 'a5')
add(MEL, 14, 0, 1, 'g5'); add(MEL, 14, 1, 1, 'fis5'); add(MEL, 14, 2, 1, 'cis5')
add(MEL, 15, 0, 1, 'b4'); add(MEL, 15, 1, 1, 'cis5'); add(MEL, 15, 2, 1, 'd5')
add(MEL, 16, 0, 3, 'a4')
add(MEL, 17, 0, 3, 'cis5')
add(MEL, 18, 0, 3, 'fis5')
add(MEL, 19, 0, 9, 'e4')
add(MEL, 22, 0, 1, 'a4'); add(MEL, 22, 1, 1, 'b4'); add(MEL, 22, 2, 1, 'c5')
add(MEL, 23, 0, 1, 'e5'); add(MEL, 23, 1, 1, 'd5'); add(MEL, 23, 2, 1, 'b4')
add(MEL, 24, 0, 1, 'd5'); add(MEL, 24, 1, 1, 'c5'); add(MEL, 24, 2, 1, 'b4')
add(MEL, 25, 0, 5, 'd5'); add(MEL, 26, 2, 1, 'd5')
add(MEL, 27, 0, 1, 'e5'); add(MEL, 27, 1, 1, 'f5'); add(MEL, 27, 2, 1, 'g5')
add(MEL, 28, 0, 1, 'a5'); add(MEL, 28, 1, 1, 'c5'); add(MEL, 28, 2, 1, 'd5')
add(MEL, 29, 0, 1, 'e5'); add(MEL, 29, 1, 1, 'd5'); add(MEL, 29, 2, 1, 'b4')
add(MEL, 30, 0, 5, 'd5'); add(MEL, 31, 2, 1, 'd5')

# --- the chords on beats 2-3 ---
G7, D7 = ('fis4', 'd4', 'b3'), ('fis4', 'cis4', 'a3')
for b in range(1, 17):
    add(MID, b, 1, 2, *(G7 if b % 2 else D7))
add(MID, 17, 1, 2, *D7); add(MID, 18, 1, 2, *G7)
add(MID, 19, 1, 2, 'b3', 'g3'); add(MID, 20, 1, 2, 'g4', 'd4', 'b3'); add(MID, 21, 1, 2, 'd4', 'a3', 'f3')
for b, c in zip(range(22, 32), [('e4', 'c4', 'a3'), ('e4', 'b3', 'g3'), ('e4', 'b3', 'g3', 'd3'),
                                ('d4', 'a3', 'e3', 'c3'), ('d4', 'a3', 'fis3', 'c3'), ('f4', 'c4', 'a3'),
                                ('e4', 'c4', 'a3'), ('e4', 'b3', 'g3', 'd3'), ('d4', 'a3', 'e3', 'c3'),
                                ('d4', 'a3', 'fis3', 'c3')]):
    add(MID, b, 1, 2, *c)

# --- the bass ---
for b in range(1, 17):
    add(BAS, b, 0, 3, 'g2' if b % 2 else 'd2')
for b, n in zip(range(17, 32), ['fis2', 'b1', 'e2', 'e2', 'd2', 'a1'] + ['d2'] * 9):
    add(BAS, b, 0, 3, n)

# --- the two endings (bars 32..39) ---
def ending(second):
    m, c, s = {}, {}, {}
    if not second:
        add(m, 32, 0, 3, 'g5'); add(m, 33, 0, 3, 'fis5')
        add(m, 34, 0, 1, 'b4'); add(m, 34, 1, 1, 'a4'); add(m, 34, 2, 1, 'b4')
        for b in (35, 36):
            add(m, b, 0, 1, 'cis5'); add(m, b, 1, 1, 'd5'); add(m, b, 2, 1, 'e5')
        add(m, 37, 0, 3, 'fis4')
        add(m, 38, 0, 3, 'c5', 'a4', 'e4', 'c4'); add(m, 39, 0, 6 if second else 3, 'd5', 'a4', 'fis4', 'd4')
        for b, ch in zip(range(32, 37), [('g4', 'e4', 'b3'), ('fis4', 'cis4', 'a3'), ('fis4', 'd4', 'b3'),
                                          ('a4', 'e4', 'cis4'), ('a4', 'fis4', 'cis4', 'a3')]):
            add(c, b, 1, 2, *ch)
        for b, n in zip(range(32, 37), ['e2', 'fis2', 'b1', 'e2', 'e2']):
            add(s, b, 0, 3, n)
    else:
        add(m, 32, 0, 3, 'g5'); add(m, 33, 0, 3, 'f5')
        add(m, 34, 0, 1, 'b4'); add(m, 34, 1, 1, 'c5'); add(m, 34, 2, 1, 'f5')
        for b in (35, 36):
            add(m, b, 0, 1, 'e5'); add(m, b, 1, 1, 'd5'); add(m, b, 2, 1, 'c5')
        add(m, 37, 0, 3, 'f4')
        add(m, 38, 0, 3, 'c5', 'a4', 'e4', 'c4'); add(m, 39, 0, 6, 'd5', 'a4', 'f4', 'd4')
        for b, ch in zip(range(32, 37), [('g4', 'e4', 'b3'), ('a4', 'f4', 'd4', 'a3'), ('f4', 'c4', 'a3'),
                                          ('a4', 'e4', 'c4'), ('a4', 'f4', 'c4', 'a3')]):
            add(c, b, 1, 2, *ch)
        for b in range(32, 37):
            add(s, b, 0, 3, 'e2')
    add(c, 37, 1, 1, 'd4', 'a3'); add(c, 37, 2, 1, 'g4', 'd4', 'b3')
    add(s, 37, 0, 3, 'e2'); add(s, 37, 1, 1, 'b2'); add(s, 37, 2, 1, 'e3')
    add(s, 38, 0, 3, 'g2', 'a1'); add(s, 39, 0, 6 if second else 3, 'd2', 'a1', 'd1')
    return m, c, s


VEL = {'m': 92, 'c': 52, 's': 70}
ev = []


def emit(parts, bar0, bars):
    for key, d in parts:
        for b in bars:
            for beat, ln, ns in d.get(b, []):
                for n in ns:
                    ev.append(((bar0 + b - 1) * 3 + beat, n, ln, VEL[key]))


body = [('m', MEL), ('c', MID), ('s', BAS)]
e1, e2 = ending(False), ending(True)
emit(body, 0, range(1, 32))
emit(list(zip('mcs', e1)), 0, range(32, 40))
emit(body, 39, range(1, 32))
emit(list(zip('mcs', e2)), 39, range(32, 40))
ev.sort()
total = 78 * 3

out = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'src', 'gymno_data.h')
with open(out, 'w') as f:
    f.write('/* Erik Satie, 1ere Gymnopedie (1888, public domain), from the Mutopia Project source.\n'
            ' * Generated by tools/gen_gymno.py: {start quarter, note, length in quarters, velocity} */\n')
    f.write('#define GY_N %d\n#define GY_LEN %d   /* quarters */\n' % (len(ev), total))
    f.write('static const uint8_t GY_EV[GY_N][5] = {\n')
    for i in range(0, len(ev), 6):
        f.write('    ' + ' '.join('{%d, %d, %d, %d, %d},' % (q & 255, q >> 8, n, ln, v)
                                  for q, n, ln, v in ev[i:i + 6]) + '\n')
    f.write('};\n')
print(len(ev), 'events,', total, 'quarters')
