# Modular One (formerly Modular Town FM)

FM synthesizer firmware for the M-VAVE FM-1, made for film music.
By Alexis Arakis ([Bungle Society](https://bunglesociety.com)), built on
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita, Hügelton Instruments.

The sound comes from the Modular Town V2 plugin (Amorph / Cmajor): its winter FM engine,
its BBD ping-pong delay, its "80's" plate reverb and the island (Dream Mode), ported to
integer arithmetic for the FM-1 (no floating point unit). Felucca gives everything else:
the hardware layer, voices, sequencer, presets, USB MIDI, the editor and the updater.

Status: **beta**, runs on the author's FM-1, not yet tested on other units. See "Install" before flashing.

## What is new

| Where | What |
| --- | --- |
| `firmware/src/eng_mtfm.c` | **TOWN**, engine 9: carrier, modulator 1 with feedback, modulator 2 (detunable), soft low-pass per voice; 16 presets (SOUFFLES, KEYS) |
| `firmware/src/mt_fx.c` | the Modular Town delay (stereo ping-pong, right line 1.38x, gliding time), plate (pre-delay, diffusers, 4 breathing combs per side) and the island on the master |
| `firmware/src/mt_tables.h` | the Cmajor curves, sampled (`tools/gen_mt_tables.py`) |
| `firmware/src/fx.c` | Felucca's delay and reverb replaced by `mt_fx.c` (stereo wet); the chorus and DIST stay |
| `firmware/src/ui.c` | the three parts start on TOWN (SOUFFLE, PRINTEMP, VELOURS EP) |
| `firmware/src/params.c` | FX defaults from the patch: FDBK 45, COLR 127 (no tone filter), SIZE 76, DAMP 70 |
| `tests/mt_test.c` | the film scene through the whole mix, with checks |
| `firmware/src/ui_draw.c` | the Modular One interface: one picture per page, one screen (the four greens of the first handheld console, gfx.c gb_quantize), the 5 s opening, the screen saver; Silkscreen (OFL, assets/fonts/ui) at 1x and 2x |
| `firmware/src/mt_city.c` | the window: on HOME, EDIT and ENV of a TOWN track, Modular Town across the water; the hour, the light, the island and the water follow the sound; the picture is rebuilt a few rows per frame |
| `firmware/src/mt_city_data.h` | the far shore, palette-indexed and RLE (`tools/gen_city.py`, about 5 KB); the sky and the water are computed |
| `tests/mt_city_host.c` | the window on the host: a day to night story into frames, the cost per frame |

All the other Felucca engines are still there. Felucca's fourth track, the GM drum track, is gone
(`firmware/src/drums.c`, the PERC sample set): three tracks, the parts; stored projects keep Felucca's
four-track layout, the fourth is written empty and ignored.

## TOWN

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| HOME | BRIL | MONT | SUS | REL |
| EDIT 1 "FM" | BRIL brightness (FM index) | GRAIN (feedback) | TIMB (ratios, 12 notches) | CLOCH (modulator 2 detune, +-12 st) |
| EDIT 2 "VILLE" | MONT (Montee) | NUIT (night) | ILE (island on / off) | - |
| ENV | ATK | DEC | SUS | REL |

- **MONT** opens the FM index (up to x2), adds a 5.2 Hz vibrato (up to 20 cents), more
  reverb (+0.35 send) and, on the island, a longer cloud and more octave shimmer.
- **NUIT** makes the FM darker and softer (less index and feedback, the low-pass closes),
  the delay longer (+0.18 feedback) and the plate larger (+0.22 size). It glides (~1.5 s).
- **ILE** turns the island on for the whole mix; it fades in over 3.5 s.

Presets: SOUFFLES (SOUFFLE, MAREE, ZEPHYR, PRINTEMP, VELOURS, NUIT, BRUINE, CREPUSCULE) and KEYS (VELOURS EP, SALON, LAMPE, FEUTRE, VEILLEUSE, COTON, BOUGIE, DIMANCHE).

## Differences with the plugin

- The amplitude envelope is Felucca's ADSR (linear attack, exponential decay and release,
  up to 10 s) with velocity, instead of the patch's linear one. The sustain is free.
- The delay time follows the tempo (TIME divisions); the left line is at most 0.557 s.
- The delay and reverb are sends (per part DLY / REV) into shared stereo buses.
- The night does not lengthen the attack.
- Measured against a float transcription of the patch: within 1 dB below 5 kHz, the noise
  floor near -90 dBFS; the wet / dry balance of the delay and the plate within 0.3 dB.

## Memory

The Modular Town lines take about 200 KB of the 344 KB RAM pool (delay 115 KB, plate
31 KB, island 54 KB; Felucca's delay and reverb are gone): about 27 KB stay spare, the
build requires 8 KB (the window takes 1.5 KB more). Every line has exactly the
length it needs, and every feedback loop rounds towards zero, so the tails die out to
exact silence and the effects sleep when nothing plays.

## Build, test, install

As Felucca: see [BUILDING.md](BUILDING.md). `tests/run_tests.sh` also renders the film
scene for every TOWN preset into `build/mt_demo/`.

The firmware runs on the author's FM-1; it has not been tested on other units yet. Check the CPU load on the INFO page with chords
and the island on. Recovery, if an install fails: [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Licence

Code: GPL-3.0-only, as Felucca (`LICENSE`, `LICENSING.md`). Modular One has no drum track: the
Hügelton Sample Pack drum sounds are not built in (`tools/gen_samples.py`). The other Felucca Assets
(icons, panel image) are not GPL: read `LICENSING.md` before distributing a build that contains them.
"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; Modular One is
not made or endorsed by them. "M-VAVE" and "FM-1" are trademarks of their owners.
