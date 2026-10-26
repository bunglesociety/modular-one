# Modular One

**A film-music firmware for the M-VAVE FM-1**, by Alexis Arakis ([Bungle Society](https://bunglesociety.com)).

Modular One turns the FM-1 into a small writing companion: a soft FM engine taken from the
Modular Town plugin, its delay, plate reverb and "island", a minimal interface in four
handheld-console greens, a window on Modular Town that follows the sound, and a MIDI player
that plays a whole piece inside the machine.

Modular One is a modified version of [Felucca](https://github.com/hugelton/Felucca) by
Leo Kuroshita (Hügelton Instruments). It is not made, endorsed or supported by
Hügelton Instruments or by M-VAVE.

> **Beta.** It runs on the author's FM-1 and has not been tested on other units yet.
> Installing custom firmware is at your own risk. Read "Before installing" first.

## What's inside

- **TOWN engine**: carrier, modulator with feedback, detunable second modulator, soft low-pass.
  40 presets in five families, made for writing film music: SOUFFLES (pads), KEYS (soft electric
  pianos), LUMIERES (felted bells), OMBRES (lows and tension), NEON (80's film synths).
- **Modular Town effects**: stereo ping-pong delay, "80's" plate reverb, and the island
  (a shimmering cloud on the whole mix).
- **Controls**: BRIL (brightness), MONT (rise), NUIT (night: darker, longer tails), ILE (island on/off).
- **The window**: Modular Town across the water; the hour, the light and the water follow the sound.
  Also used as a screen saver.
- **The tape player**: hold OCT- and OCT+ together for 1 second. It plays your songs (MIDI files
  sent from the computer) and your sketches, one after the other and round again, on the current sound, with a
  reel-to-reel tape recorder on screen. KNOB 1 pops up the list to choose a song or a sketch, OCT+ next, OCT- back to the
  start, OCT- twice the previous one. You can play over it. Same gesture or HOME to stop.
  A first song to try: `examples/01_satie_gymnopedie_1.mid` (Erik Satie, public domain).
- **The sketchbook**: the FM-1 always listens. Liked what you just played? Hold REC: it keeps your
  last phrase (from the first note after 4 s of silence) as a sketch. Hold REC again: one phrase
  further back. Sketches stay after power-off and join the player.
- **The Carnet app** ([online](https://bunglesociety.github.io/modular-one/carnet/), or `web/carnet.html`, in Chrome): drop MIDI files, order and rename them, send
  them to the FM-1; listen to your sketches, export them as MIDI, erase them, turn them into songs.
- **`tools/fm1_songs.py`**: the same from the command line: send your songs (`load folder/`), get your sketches back as MIDI files
  (`sketches folder/`).
- Three synth tracks. Felucca's drum track is removed.

Full details (pages, knobs, technical notes): [MODULAR_TOWN.md](MODULAR_TOWN.md).

## Installing

**The easy way: [bunglesociety.github.io/modular-one](https://bunglesociety.github.io/modular-one/)**,
in Chrome or Edge on a computer. Connect the FM-1 by USB, click Install, wait about two minutes.
The same site has the **Carnet** app for your songs and sketches.

Before installing:

1. Plug the USB cable **directly** into the computer, no hub. Keep the computer awake.
2. Your own presets from the official firmware are lost (they cannot be exported).
3. If an install fails and the FM-1 no longer starts, the recovery tool is
   [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

**From a terminal** instead: download `modular-one-0.2-beta.fwsc` (Releases page, or
`docs/firmware/` in this repository), then:

```
pip3 install mido python-rtmidi
python3 tools/fm1_install.py --info
python3 tools/fm1_install.py modular-one-0.2-beta.fwsc
```

`--info` only finds the FM-1 and writes nothing.

**Going back**: reinstall the official firmware with M-VAVE's M-UPGRADE tool (m-vave.com, Downloads).

SHA-256 of `modular-one-0.2-beta.fwsc`:
`1dac495c068e8a220598920bbcd79b03ce39d7d260966ae1b591ab6d54cc274c`

## Building from source

Same as Felucca: see [BUILDING.md](BUILDING.md). A French guide for macOS is in
[docs/GUIDE_FR.md](docs/GUIDE_FR.md).

## Licence and credits

- Code: **GPL-3.0-only**, as Felucca (see `LICENSE` and `LICENSING.md`). The complete source of
  this firmware is in this repository.
- Felucca: Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments. Felucca's own
  README is kept in [README_FELUCCA.md](README_FELUCCA.md).
- Modular One changes: Copyright (C) 2026 Alexis Arakis, released under GPL-3.0-only.
- Third-party material keeps its own licence, listed in `LICENSING.md`.
- "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.
  "M-VAVE" and "FM-1" are trademarks of their respective owners.

Français : [LISEZMOI.md](LISEZMOI.md)
