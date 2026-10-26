#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular One: the songs of the player and the sketchbook, over USB-MIDI.

  fm1_songs.py info                         the songs on the FM-1, the sketches
  fm1_songs.py load FILE.mid|FOLDER ...     send these songs (they replace the ones there;
                                            a folder: its .mid files in name order, e.g. 01_xxx.mid)
  fm1_songs.py clear                        no songs
  fm1_songs.py sketches FOLDER              copy every sketch to FOLDER as sketch_01.mid ...
  fm1_songs.py clear-sketches               erase every sketch on the FM-1
  fm1_songs.py build FILE.mid|FOLDER ... OUT.bin   (no device) the song bank as written to flash

The songs play on the sound of the selected part: every channel of a file plays on it (8 voices).
Up to 20 songs, about 78 KiB in all (several hours of piano music). A song name is its file name,
12 characters, upper case. Protocol: firmware/src/editor.c, cmds 40..46; flash: m1_book.c.
Needs mido (and a backend such as python-rtmidi).
"""
import struct
import sys
import time
import unicodedata
import zlib
from pathlib import Path

HDR = [0x7D, 0x46, 0x4C]
SONG_MAX, SONG_SIZE, SONG_DATA = 20, 0x14000, 512
SONG_MAGIC, SK_MAGIC = 0x314E5346, 0x314B5346
SK_NAMED = 0xFFFFFFFE
SK_SECTORS = 20
RC = {1: "arguments", 2: "erase", 3: "write", 4: "order"}


def pack7(b):
    """groups of up to 7 bytes, each preceded by their top bits"""
    out = []
    for i in range(0, len(b), 7):
        g = b[i:i + 7]
        out.append(sum(((x >> 7) & 1) << j for j, x in enumerate(g)))
        out += [x & 0x7F for x in g]
    return out


def unpack7(a):
    out = []
    i = 0
    while i < len(a):
        m = a[i]
        for j, x in enumerate(a[i + 1:i + 8]):
            out.append(x | ((m >> j) & 1) << 7)
        i += 8
    return bytes(out)


def var(v):
    v = max(0, min(int(v), 0x7FFF))
    return [v] if v < 128 else [0x80 | (v >> 8), v & 255]


def encode(notes):
    """notes: (start s, note, vel, length s) -> piece bytes (seq.c: dt 10 ms, note, vel, len 20 ms)"""
    out, prev = [], 0
    for s, n, v, d in sorted(notes):
        t = int(round(s * 100))
        out += var(t - prev) + [n & 127, max(1, min(127, v))] + var(max(1, int(round(d * 50))))
        prev = t
    return bytes(out)


def decode(b, count):
    notes, i, t = [], 0, 0

    def rd():
        nonlocal i
        v = b[i]
        i += 1
        if v & 0x80:
            v = (v & 0x7F) << 8 | b[i]
            i += 1
        return v
    for _ in range(count):
        t += rd()
        n, v = b[i], b[i + 1]
        i += 2
        notes.append((t / 100, n, v, rd() / 50))
    return notes


def read_mid(path):
    import mido
    mf = mido.MidiFile(path)
    notes, held, now = [], {}, 0.0
    for msg in mf:                                  # merged tracks, times in seconds (tempo map applied)
        now += msg.time
        if msg.type == "note_on" and msg.velocity:
            k = (msg.channel, msg.note)
            if k in held:
                s, v = held.pop(k)
                notes.append((s, msg.note, v, now - s))
            held[k] = (now, msg.velocity)
        elif msg.type in ("note_off", "note_on"):
            k = (msg.channel, msg.note)
            if k in held:
                s, v = held.pop(k)
                notes.append((s, msg.note, v, max(now - s, 0.02)))
    for (ch, n), (s, v) in held.items():
        notes.append((s, n, v, max(now - s, 0.02)))
    if not notes:
        raise ValueError(f"{path}: no notes")
    t0 = min(s for s, *_ in notes)
    return [(s - t0, n, v, d) for s, n, v, d in notes]


def song_name(path):
    s = unicodedata.normalize("NFKD", Path(path).stem).encode("ascii", "ignore").decode()
    s = "".join(c if 32 <= ord(c) < 127 else " " for c in s.replace("_", " ")).strip().upper()
    if len(s) > 3 and s[:2].isdigit() and s[2] == " ":   # "01 night walk" -> "NIGHT WALK"
        s = s[3:]
    return s[:12] or "SONG"


def midi_files(args):
    files = []
    for a in args:
        p = Path(a)
        files += sorted(p.glob("*.mid")) + sorted(p.glob("*.midi")) if p.is_dir() else [p]
    if not files:
        sys.exit("no .mid file")
    if len(files) > SONG_MAX:
        sys.exit(f"{len(files)} songs: {SONG_MAX} at most")
    return files


def bank(files):
    """-> header (420 B), data, report lines"""
    data, ents, lines = b"", [], []
    for f in files:
        notes = read_mid(f)
        b = encode(notes)
        ents.append((len(data), len(notes), song_name(f)))
        lines.append(f"  {song_name(f):12}  {len(notes):5} notes  {max(s + d for s, _, _, d in notes) / 60:5.1f} min  {len(b)} B")
        data += b
    if len(data) > SONG_SIZE - SONG_DATA:
        sys.exit(f"{len(data)} B of songs: {SONG_SIZE - SONG_DATA} B at most")
    hdr = struct.pack("<IHHIII", SONG_MAGIC, 1, len(ents), len(data), zlib.crc32(data), 0)
    for off, n, name in ents:
        hdr += struct.pack("<II12s", off, n, name.encode())
    hdr += b"\0" * (420 - len(hdr))
    return hdr, data, lines


def midi_out(path, notes):
    import mido
    mf = mido.MidiFile(ticks_per_beat=480)
    tr = mido.MidiTrack()
    mf.tracks.append(tr)
    tr.append(mido.MetaMessage("set_tempo", tempo=500000))      # 120 BPM: 1 s = 960 ticks
    ev = []
    for s, n, v, d in notes:
        ev.append((s, 1, n, v))
        ev.append((s + d, 0, n, 0))
    last = 0
    for t, on, n, v in sorted(ev):
        tick = int(round(t * 960))
        tr.append(mido.Message("note_on" if on else "note_off", note=n, velocity=v if on else 0, time=tick - last))
        last = tick
    mf.save(path)


class Link:
    def __init__(self):
        try:
            import mido
        except ImportError:
            sys.exit("needs mido: pip3 install mido python-rtmidi")
        self.mido = mido
        outs = [n for n in mido.get_output_names() if "Felucca" in n]
        ins = [n for n in mido.get_input_names() if "Felucca" in n]
        if not outs or not ins:
            sys.exit("no MIDI port named Felucca: connect the FM-1 (running Modular One) by USB")
        self.o = mido.open_output(outs[0])
        self.i = mido.open_input(ins[0])
        for _ in range(3):                          # the first frame after opening the port can be lost
            try:
                self.req(1, [], 1.0)
                break
            except TimeoutError:
                pass

    def req(self, cmd, args, timeout=3.0):
        for _ in self.i.iter_pending():
            pass
        self.o.send(self.mido.Message("sysex", data=HDR + [cmd] + list(args)))
        t0 = time.time()
        while time.time() - t0 < timeout:
            for m in self.i.iter_pending():
                if m.type == "sysex" and list(m.data[:4]) == HDR + [cmd]:
                    return list(m.data[4:])
            time.sleep(0.002)
        raise TimeoutError(f"no reply to cmd {cmd} (is it Modular One 0.2 or later?)")


def send_bank(link, hdr, data):
    if link.req(40, [], 5)[0]:
        sys.exit("begin failed")
    for off in range(0, len(data), 256):
        a = SONG_DATA + off
        r = link.req(41, [a & 0x7F, (a >> 7) & 0x7F, (a >> 14) & 0x7F] + pack7(data[off:off + 256]))
        if r[3]:
            sys.exit(f"write at {a:#x} failed ({RC.get(r[3], r[3])})")
        print(f"\r  {min(off + 256, len(data))} / {len(data)} B", end="", flush=True)
    r = link.req(42, pack7(hdr), 5)
    print("\n  done" if r[0] == 0 else f"\n  FAILED ({r[0]})")


def read_sketches(link):
    used = link.req(44, [])[1]
    area = b""
    for off in range(0, used * 4096, 256):
        r = link.req(45, [off & 0x7F, (off >> 7) & 0x7F, (off >> 14) & 0x7F])
        area += unpack7(r[3:])[:256]
    out, s = [], 0
    while s < used:
        magic, nsect, num, notes, ln, dur10, crc, flags = struct.unpack_from("<IHHIIIII", area, s * 4096)
        body = area[s * 4096 + 32:s * 4096 + 32 + ln]
        if magic != SK_MAGIC or not nsect or zlib.crc32(body) != crc:
            break
        name = ""
        if flags == SK_NAMED:                       # the body starts with its name (12 bytes, 0-padded)
            name = body[:12].split(b"\0")[0].decode("ascii", "replace").strip()
            body = body[12:]
        out.append((num, decode(body, notes), name))
        s += nsect
    return out


def main():
    a = sys.argv[1:]
    if not a or a[0] not in ("info", "load", "clear", "sketches", "clear-sketches", "build"):
        sys.exit(__doc__)
    if a[0] == "build":
        if len(a) < 3:
            sys.exit(__doc__)
        hdr, data, lines = bank(midi_files(a[1:-1]))
        Path(a[-1]).write_bytes(hdr + b"\xff" * (SONG_DATA - len(hdr)) + data)
        print("\n".join(lines) + f"\n{a[-1]}: {len(data)} B of songs")
        return
    if a[0] == "load":
        if len(a) < 2:
            sys.exit(__doc__)
        hdr, data, lines = bank(midi_files(a[1:]))
        print("\n".join(lines))
    link = Link()
    if a[0] == "info":
        r = link.req(43, [])
        i = 1
        print(f"songs: {r[0] or 'none'}")
        for _ in range(r[0]):
            n = r[i] | r[i + 1] << 7 | r[i + 2] << 14
            j = r.index(0, i + 3)
            print(f"  {bytes(r[i + 3:j]).decode():12}  {n} notes")
            i = j + 1
        s = link.req(44, [])
        print(f"sketches: {s[0]} ({s[1]} of {s[2]} sectors of 4 KiB used)")
    elif a[0] == "load":
        send_bank(link, hdr, data)
    elif a[0] == "clear":
        print("clear:", "ok" if not link.req(40, [], 5)[0] else "FAILED")
    elif a[0] == "sketches":
        if len(a) < 2:
            sys.exit(__doc__)
        d = Path(a[1])
        d.mkdir(parents=True, exist_ok=True)
        sk = read_sketches(link)
        for num, notes, name in sk:
            safe = "".join(c if c.isalnum() else "_" for c in name.lower()).strip("_")
            p = d / (f"sketch_{num:02}_{safe}.mid" if safe else f"sketch_{num:02}.mid")
            midi_out(p, notes)
            print(f"  {p}  {len(notes)} notes, {max(s + l for s, _, _, l in notes):.1f} s")
        print(f"{len(sk)} sketches")
    elif a[0] == "clear-sketches":
        print("clear:", "ok" if not link.req(46, [], 10)[0] else "FAILED")


if __name__ == "__main__":
    try:
        main()
    except TimeoutError as e:
        sys.exit(f"{e}: is Modular One running, and no other app using its MIDI port?")
