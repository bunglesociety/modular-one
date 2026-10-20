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
  16 presets in two families: SOUFFLES (pads) and KEYS (soft electric pianos).
- **Modular Town effects**: stereo ping-pong delay, "80's" plate reverb, and the island
  (a shimmering cloud on the whole mix).
- **Controls**: BRIL (brightness), MONT (rise), NUIT (night: darker, longer tails), ILE (island on/off).
- **The window**: Modular Town across the water; the hour, the light and the water follow the sound.
  Also used as a screen saver.
- **The MIDI player**: hold OCT- and OCT+ together for 1 second to play Erik Satie's
  1st Gymnopedie (public domain, about 3 minutes) on the current track. You can play over it. Same gesture or HOME to stop.
- Three synth tracks. Felucca's drum track is removed.

Full details (pages, knobs, technical notes): [MODULAR_TOWN.md](MODULAR_TOWN.md).

## Before installing

1. Go back to the **official M-VAVE firmware (V15 or newer)** first, with M-VAVE's M-UPGRADE tool
   (m-vave.com, Downloads).
2. Plug the USB cable **directly** into the computer, no hub. Keep the computer awake.
3. If an install fails and the FM-1 no longer starts, the recovery tool is
   [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Installing

Download `modular-one-0.1-beta.fwsc` from the Releases page, then, from a terminal in this repository:

```
pip3 install mido python-rtmidi
python3 tools/fm1_install.py --info
python3 tools/fm1_install.py modular-one-0.1-beta.fwsc
```

`--info` only finds the FM-1 and writes nothing.

**Going back**: reinstall the official firmware with M-VAVE's M-UPGRADE.

SHA-256 of `modular-one-0.1-beta.fwsc`:
`beab4e0e28ff2ff3ef5073d6d63fa257c466c0fca25ba984f0f59335665184ca`

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
