/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Keyboard, scale, arpeggiator, sequencer and transport. Runs in the audio
 * ISR, once per CTL-sample block, and ends in trk_note_on / trk_note_off:
 * engines never see where a note came from.
 * Three tracks, one transport: every track's pattern loops on its own LEN / DIV /
 * SWING / GATE (polymeter). The keys play the selected track; MIDI channel 1 too (Modular One),
 * channels 2..3 play parts 2..3, any other channel the selected track. A note into an armed track (song.rec) while
 * the transport runs is recorded into its pattern, quantised to its (swung) steps, with its
 * held length as TIE steps (rec_note, rec_hold, rec_release). */
static const uint16_t SCALE_MASK[] = {
    0xFFF,                                   /* CHR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MAJ */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* MIN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* DOR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* MIX */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                          /* PEN */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                         /* MPEN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),   /* HARM */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* PHRY */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11),   /* LYD */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10),   /* LOC */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MEL (ascending) */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10),              /* BLUES (minor) */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10),              /* WHOLE */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 10), /* DIMHW */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 11), /* DIMWH */
};

#define KB_SILENT 255u
static uint32_t kb_prev;
static uint8_t kb_note[27], kb_trk[27];  /* per key: the note it started and on which track */
static uint8_t last_note = 60;
static volatile uint8_t transport_req;   /* 1 start, 2 stop (from the UI) */
static volatile uint8_t panic_req;       /* bit per track: release every sounding note (preset / engine change) */

static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }

static uint32_t trk_midi_ch(uint32_t i) { return i; }   /* MIDI channel 0..15 of track i (keys -> MIDI out) */

static uint32_t scale_mask(const track_t *t)
{
    return SCALE_MASK[clamp(t->p[P_SCALE], 0, sizeof SCALE_MASK / sizeof SCALE_MASK[0] - 1)];
}

static uint32_t kb_map(const track_t *t, uint32_t k)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t n = 53 + (int32_t)k;
#if FELUCCA_SLICE
    if (ENGINES[t->eng_req % NENGINES] == &ENG_SLICE)   /* SLICE: lowest key = slice 0 (C4 + ROOT), no scale */
        return (uint32_t)clamp(SLC_BASE + t->p[P_ROOT] + 12 * song.octave + (int32_t)k, 0, 127);
#endif
    if (t->p[P_QUANT] == 1) {                    /* SNAP: every key, rounded down to the scale (the old ON) */
        uint32_t mask = scale_mask(t), guard = 12;
        n += 12 * song.octave + t->p[P_TRANS];
        while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
            n--;
        return (uint32_t)clamp(n, 0, 127);
    }
    if (t->p[P_QUANT] == 2) {                    /* WHITE: white keys walk the scale, black keys are silent */
        uint32_t mask = scale_mask(t), i;
        int32_t count = 0, degree = DEGREE[n % 12], oct;
        if (degree < 0)
            return KB_SILENT;
        /* C4 is the root. Walk scale degrees on successive white keys, including
         * below C4; scales with 5, 6, 8 or 12 notes still have no duplicated degrees. */
        degree += (n / 12 - 5) * 7;
        for (i = 0; i < 12u; i++)
            count += (mask >> i) & 1u;
        oct = degree / count;
        degree %= count;
        if (degree < 0) {
            degree += count;
            oct--;
        }
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (!degree)
                    break;
                degree--;
            }
        n = 60 + t->p[P_ROOT] + 12 * oct + (int32_t)i;
    }
    return (uint32_t)clamp(n + 12 * song.octave + t->p[P_TRANS], 0, 127);
}

/* ------------------------------------------------------------- arp --- */
static void arp_add(track_t *t, uint32_t note)
{
    uint32_t i;
    if (t->p[P_AHOLD] && t->arp_phys == 0u)
        t->nheld = 0;                               /* new chord replaces the latched one */
    t->arp_phys++;                                  /* every key-down: arp_remove counts every key-up */
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] == note)
            return;                                 /* repeated note-on: not a new note */
    if (t->nheld < 16u)
        t->held[t->nheld++] = (uint8_t)note;
    if (t->nheld == 1u) {
        t->arp_pos = 0xFFFFFFF;                     /* fire on this block */
        t->arp_idx = 0xFFFFFFFFu;
    }
}

static void arp_remove(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    if (t->arp_phys)
        t->arp_phys--;
    if (t->p[P_AHOLD])
        return;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] != note)
            t->held[k++] = t->held[i];
    t->nheld = (uint8_t)k;
}

static void arp_tick(track_t *t, uint32_t n)
{
    uint32_t period = div_samples((uint32_t)t->p[P_ARATE]), cnt, list[64], len = 0, i, j, o;
    int32_t sw = t->p[P_ASWING] * (int32_t)period / 250;
    if (t->arp_note) {
        if (t->arp_off <= n) {
            trk_note_off(t, t->arp_note);
            t->arp_note = 0;
        } else {
            t->arp_off -= n;
        }
    }
    if (!t->p[P_AMODE] || !t->nheld) {
        if (!t->nheld && t->arp_note) {
            trk_note_off(t, t->arp_note);
            t->arp_note = 0;
        }
        return;
    }
    t->arp_pos += n;
    if (t->arp_pos < period + (uint32_t)((t->arp_idx & 1u) ? sw : -sw) && t->arp_pos != 0xFFFFFFF + n)
        return;
    t->arp_pos = 0;
    /* build the note list: held notes (sorted or as played) over OCT octaves */
    for (i = 0; i < t->nheld; i++)
        list[i] = t->held[i];
    cnt = t->nheld;
    if (!t->p[P_AORDER])
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && list[j - 1] > list[j]; j--) {
                uint32_t x = list[j];
                list[j] = list[j - 1];
                list[j - 1] = x;
            }
    for (o = 0; o < (uint32_t)t->p[P_AOCT]; o++)
        for (i = 0; i < cnt && len < 64u; i++)
            list[len++] = clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    t->arp_idx++;
    switch (t->p[P_AMODE]) {
    case 2:
        j = len - 1u - t->arp_idx % len;
        break;
    case 3: {
        uint32_t cyc = len > 1u ? 2u * len - 2u : 1u, k = t->arp_idx % cyc;
        j = k < len ? k : cyc - k;
        break;
    }
    case 4:
        j = rng() % len;
        break;
    default:
        j = t->arp_idx % len;
        break;
    }
    if (t->arp_note)
        trk_note_off(t, t->arp_note);
    t->arp_note = 0;
    if ((uint32_t)(rng() & 127u) <= (uint32_t)t->p[P_APROB]) {
        t->arp_note = (uint8_t)list[j];
        t->arp_off = period * (uint32_t)t->p[P_AGATE] / 128u;
        trk_note_on(t, t->arp_note, 100);
    }
}

/* -------------------------------------------------------- note input --- */
/* the length of step idx in samples: SWING (the track's + the global) makes the even steps longer
 * and the odd ones shorter, so every odd step starts late */
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx)
{
    int32_t sw = (t->p[P_SSWING] + song.g[G_SWING]) * (int32_t)period / 250;
    return period + (uint32_t)((idx & 1u) ? -sw : sw);
}

/* live recording: the note goes into the nearest step, as swung (the one playing, or
 * the next one when it is past the middle of the playing one). Overdub: a step that
 * holds notes gets this one added (a chord of up to 4; when full, the last note is
 * replaced); MONO / LEGATO / UNISON parts keep one note per step, as step entry does.
 * Held on (synth parts): each further step the sequencer enters while the note is
 * held becomes a TIE (rec_hold), up to the pattern length; a release before the middle
 * of the last one puts that step back (rec_release), so a short note stays one step.
 * A note recorded into another step ends the hold before (the step model ties the
 * notes of one step only). */
static void rec_note(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, idx = t->seq_idx % len, k;
    uint32_t period = div_samples((uint32_t)t->p[P_SDIV]);
    uint32_t next = t->seq_pos > step_samples(t, period, t->seq_idx) / 2u;
    step_t *s;
    if (next)
        idx = (idx + 1u) % len;
    s = &t->step[idx];
    if (s->time != ST_NOTE || !s->n || t->p[P_VOICE] != V_POLY) {
        s->n = 0;                                   /* a fresh step */
        s->flags = 0;
        s->vel = 0;
    }
    for (k = 0; k < s->n && s->note[k] != note; k++)
        ;
    if (k == s->n) {
        if (s->n < 4u)
            s->n++;
        s->note[s->n - 1u] = (uint8_t)note;
    }
    s->time = ST_NOTE;
    if (vel > 110)
        s->flags |= SF_ACCENT;
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    t->seq_active = 1;
    if (next) {                                     /* it sounds now: the step must not trigger it again */
        if (t->rskip_idx != idx)
            t->rskip_n = 0;
        t->rskip_idx = (uint8_t)idx;
        if (t->rskip_n < 4u)
            t->rskip[t->rskip_n++] = (uint8_t)note;
    }
    if (!t->rh_n || t->rh_start != idx) {           /* a new hold (one in another step ends) */
        t->rh_n = 0;
        t->rh_start = (uint8_t)idx;
        t->rh_ties = 0;
    }
    for (k = 0; k < t->rh_n && t->rh_note[k] != note; k++)
        ;
    if (k == t->rh_n && t->rh_n < 4u)
        t->rh_note[t->rh_n++] = (uint8_t)note;      /* a chord: held until its last key is up */
}

/* the sequencer enters step idx (before playing it): a recorded note still held ties into it */
static void rec_hold(track_t *t, uint32_t idx, uint32_t len)
{
    step_t *s;
    uint32_t k;
    if (!t->rh_n)
        return;
    if (!((song.rec >> trk_index(t)) & 1u) || t->rh_ties + 1u >= len) {
        t->rh_n = 0;                                /* disarmed, or the whole pattern is this note */
        return;
    }
    if (idx == t->rh_start)
        return;                                     /* (recorded ahead into the step now starting) */
    s = &t->step[idx];
    t->rh_bak = *s;
    t->rh_last = (uint8_t)idx;
    t->rh_ties++;
    for (k = 0; k < 4u; k++)
        s->note[k] = 0;
    s->n = 0;
    s->time = ST_TIE;
    s->flags = 0;
    s->vel = 0;
}

/* a key of a recorded note is up: the hold ends with the last one */
static void rec_release(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    for (i = 0; i < t->rh_n; i++)
        if (t->rh_note[i] != note)
            t->rh_note[k++] = t->rh_note[i];
    if (k == t->rh_n || (t->rh_n = (uint8_t)k))
        return;                                     /* not one of them, or others still held */
    if (t->rh_ties && t->seq_idx == t->rh_last &&
        t->seq_pos < step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / 2u)
        t->step[t->rh_last] = t->rh_bak;            /* released early in it: not held into this step */
}

static void input_on(track_t *t, uint32_t note, uint32_t vel)
{
    last_note = (uint8_t)note;
    if (((song.rec >> trk_index(t)) & 1u) && song.playing)
        rec_note(t, note, vel);
    if (t->p[P_AMODE])
        arp_add(t, note);
    else
        trk_note_on(t, note, vel);
}

static void input_off(track_t *t, uint32_t note)
{
    rec_release(t, note);
    arp_remove(t, note);                            /* both: the note may have started in the */
    trk_note_off(t, note);                          /* other mode (ARP switched while held) */
}

static void keyboard_block(void)
{
    uint32_t cur = fm1_in.notes, ch, k;
    ch = cur ^ kb_prev;                           /* keys also sound while entering steps */
    if (!ch)
        return;
    for (k = 0; k < 27u; k++) {
        uint32_t mc;
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {                    /* the selected track; the key-up goes to the same one */
            kb_trk[k] = song.sel;
            kb_note[k] = (uint8_t)kb_map(&trk[kb_trk[k]], k);
            if (kb_note[k] == KB_SILENT)
                continue;
            input_on(&trk[kb_trk[k]], kb_note[k], 100);
            mc = trk_midi_ch(kb_trk[k]);
            midi_out_event(0x09u | (0x90u | mc) << 8 | (uint32_t)kb_note[k] << 16 | 100u << 24);
        } else {
            if (kb_note[k] == KB_SILENT)
                continue;
            input_off(&trk[kb_trk[k] % NTRK], kb_note[k]);
            mc = trk_midi_ch(kb_trk[k] % NTRK);
            midi_out_event(0x08u | (0x80u | mc) << 8 | (uint32_t)kb_note[k] << 16);
        }
    }
    kb_prev = cur;
}

/* -------------------------------------------------------- sequencer --- */
static void seq_start(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {                   /* every track from its step 0, together */
        track_t *t = &trk[i];
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFF;                   /* step 0 fires on the first block */
        t->rskip_n = 0;
        t->rh_n = 0;
    }
    song.tick = 0;
    song.playing = 1;
    slicer_start();                                /* slicer.c: its step 0 with the sequencer's */
}

static void seq_release(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++)
        trk_note_off(t, t->seq_notes[i]);
    t->seq_n = 0;
    t->seq_hold = 0;
    t->slide_glide = 0;                             /* live MONO / LEG keys must not glide after it */
}

static void seq_stop(void)
{
    uint32_t i;
    song.playing = 0;
    for (i = 0; i < NTRK; i++) {
        seq_release(&trk[i]);
        trk[i].rh_n = 0;                           /* a recorded note held over the stop: as far as it got */
    }
}

/* play one step: TIE extends, REST releases, NOTE (re)triggers; a SLIDE on
 * the previous step makes this one legato with a glide (acid style). skip: bit k =
 * note k already sounds from live recording (not triggered, not released here). */
static void seq_step(track_t *t, const step_t *s, uint32_t period, uint32_t skip)
{
    uint32_t i, j, gate = period * (uint32_t)t->p[P_SGATE] / 128u;
    uint32_t vel = (s->flags & SF_ACCENT) ? 127u : (s->vel ? s->vel : 96u);
    uint32_t slide_in = t->seq_hold && t->seq_n;
    uint32_t len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    uint32_t next_tie = t->step[(t->seq_idx + 1u) % len].time == ST_TIE;
    if (s->time == ST_TIE) {
        if (t->seq_n) {
            t->seq_off = gate + period / 2u;
            t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie;   /* chains hold at any GATE / swing */
        }
        return;
    }
    if (s->time == ST_REST || !s->n) {
        seq_release(t);
        return;
    }
    t->slide_glide = (uint8_t)slide_in;
    if (!slide_in)
        seq_release(t);
    for (i = 0; i < s->n; i++)
        if (!((skip >> i) & 1u))
            trk_note_on(t, s->note[i], vel);
    if (slide_in)                                   /* release what is not held over */
        for (i = 0; i < t->seq_n; i++) {
            for (j = 0; j < s->n && s->note[j] != t->seq_notes[i]; j++)
                ;
            if (j == s->n)
                trk_note_off(t, t->seq_notes[i]);
        }
    t->seq_n = 0;
    for (i = 0; i < s->n; i++)
        if (!((skip >> i) & 1u))
            t->seq_notes[t->seq_n++] = s->note[i];
    t->seq_off = gate;
    t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie;   /* next step a TIE: keep the notes to it */
}

static void seq_tick(track_t *t, uint32_t n)
{
    uint32_t period = div_samples((uint32_t)t->p[P_SDIV]), len = (uint32_t)t->p[P_SLEN];
    if (t->seq_n && !t->seq_hold) {
        if (t->seq_off <= n)
            seq_release(t);
        else
            t->seq_off -= n;
    }
    if (!song.playing)
        return;
    t->seq_pos += n;
    for (;;) {
        uint32_t cur_len = step_samples(t, period, t->seq_idx);
        if (t->seq_pos < cur_len && t->seq_pos != 0x7FFFFFFFu + n)
            break;
        t->seq_pos = t->seq_pos >= 0x7FFFFFFFu ? 0 : t->seq_pos - cur_len;
        t->seq_idx = (uint16_t)((t->seq_idx + 1u) % (len ? len : 1u));
        rec_hold(t, t->seq_idx, len ? len : 1u);
        {
            const step_t *s = &t->step[t->seq_idx];
            uint32_t skip = 0, i, k;
            if (t->rskip_n && t->rskip_idx == t->seq_idx) {
                for (i = 0; i < s->n; i++)
                    for (k = 0; k < t->rskip_n; k++)
                        if (s->note[i] == t->rskip[k])
                            skip |= 1u << i;
                t->rskip_n = 0;
            }
            seq_step(t, s, period, skip);
        }
    }
}

/* MIDI in: the track a channel plays (0..15) */
static track_t *midi_track(uint32_t ch)
{
    return ch >= 1u && ch < NPART ? &trk[ch] : TSEL;      /* Modular One: channel 1 follows the selection */
}

/* a channel that plays the selected track: its note-off goes to the track its note-on went to,
 * even when another track was selected in between (else that note would hang) */
static uint8_t midi_sel_on[16][128];                  /* per channel and note: track + 1, 0 = none */
static track_t *midi_route(uint32_t ch, uint32_t note, int on)
{
    track_t *t = midi_track(ch);
    if (ch >= 1u && ch < NPART)
        return t;                                     /* a part's own channel (2, 3) */
    if (on)
        midi_sel_on[ch & 15u][note & 127u] = (uint8_t)(song.sel + 1u);
    else if (midi_sel_on[ch & 15u][note & 127u]) {
        t = &trk[(midi_sel_on[ch & 15u][note & 127u] - 1u) % NTRK];
        midi_sel_on[ch & 15u][note & 127u] = 0;
    }
    return t;
}

/* DEMO, on the selected part (OCT- + OCT+ held, ui_input.c): Erik Satie's 1st Gymnopedie
 * (public domain, album_data.h from tools/gen_gymno_album.py), with a sound of its own (soft and warm, short notes) on the
 * selected part; the part's own sound comes back after */
#include "album_data.h"
#define DEMO_HELD 32u
static volatile uint8_t demo_req;        /* 1 play, 2 stop (from the UI) */
static volatile uint8_t demo_on;         /* playing (read by the UI) */
static uint32_t demo_pos, demo_i, demo_rd, demo_at;
static uint8_t demo_t;
static int16_t demo_save[P_COUNT];       /* the part's sound, given back at the end */
#ifndef AL_SOUND
#define AL_SOUND 20, 0, 0, 0, 0, 90, 0, 20      /* BRIL GRAIN TIMB(1:1) CLOCH MONT NUIT ILE SUB: near a sine, warm */
#endif
#ifndef AL_ENV
#define AL_ENV 30, 92, 25, 80                   /* ATK ~9 ms, DEC ~0.7 s, SUS low, REL ~0.3 s */
#endif
#ifndef AL_FX
#define AL_FX 0, 35, 0, 40                      /* DIST CHORUS DELAY REVERB */
#endif
#ifndef AL_MAXLEN
#define AL_MAXLEN 52920u                        /* no note longer than 1.2 s (44.1 kHz) */
#endif
static struct { uint32_t off; uint8_t note; } demo_held[DEMO_HELD];

static void demo_end(void)
{
    track_t *t = &trk[demo_t];
    uint32_t k;
    for (k = 0; k < DEMO_HELD; k++)
        if (demo_held[k].note) {
            trk_note_off(t, demo_held[k].note);
            demo_held[k].note = 0;
        }
    for (k = 0; k < P_COUNT; k++)
        t->p[k] = demo_save[k];
    demo_on = 0;
}

static uint32_t al_var(void)
{
    uint32_t v = AL_DATA[demo_rd++];
    if (v & 0x80u)
        v = ((v & 0x7Fu) << 8) | AL_DATA[demo_rd++];
    return v;
}

/* the next note of the album: 0 at the end; at, len in samples */
static int demo_next(uint32_t *at, uint32_t *note, uint32_t *len, uint32_t *vel)
{
    if (demo_i >= AL_N)
        return 0;
    demo_i++;
    demo_at += al_var() * 441u;                  /* 10 ms */
    *at = demo_at;
    *note = AL_DATA[demo_rd++];
    *vel = AL_DATA[demo_rd++];
    *len = al_var() * 882u;                      /* 20 ms */
    if (*len > AL_MAXLEN)
        *len = AL_MAXLEN;
    return 1;
}

static void demo_hold(uint32_t note, uint32_t off)
{
    uint32_t k, f = DEMO_HELD, old = DEMO_HELD;
    for (k = 0; k < DEMO_HELD; k++) {
        if (demo_held[k].note == note) {         /* the same key again: one note, the longer end */
            if (off > demo_held[k].off)
                demo_held[k].off = off;
            return;
        }
        if (!demo_held[k].note && f == DEMO_HELD)
            f = k;
        if (demo_held[k].note && (old == DEMO_HELD || demo_held[k].off < demo_held[old].off))
            old = k;
    }
    if (f == DEMO_HELD) {                        /* full: the one ending first lets go now */
        trk_note_off(&trk[demo_t], demo_held[old].note);
        f = old;
    }
    demo_held[f].note = (uint8_t)note;
    demo_held[f].off = off;
}

static void demo_block(uint32_t n)
{
    static uint32_t nx_at, nx_note, nx_len, nx_vel;
    static int nx_ok;
    track_t *t;
    uint32_t k, r = demo_req;
    demo_req = 0;
    if (r) {
        if (demo_on)
            demo_end();
        if (r == 1u) {
            demo_t = song.sel;
            t = &trk[demo_t];
            trk_all_off(t);
            {                                          /* the album's own sound: soft and warm, POLY, */
                static const int16_t E[8] = {AL_SOUND}, V[4] = {AL_ENV}, X[4] = {AL_FX};   /* no envelope */
                for (k = 0; k < P_COUNT; k++)          /* on the brightness, no LFO */
                    demo_save[k] = t->p[k];
                for (k = 0; k < 8u; k++)
                    t->p[P_E0 + k] = E[k];
                t->p[P_ATK] = V[0];
                t->p[P_DEC] = V[1];
                t->p[P_SUS] = V[2];
                t->p[P_REL] = V[3];
                for (k = 0; k < 4u; k++)
                    t->p[P_DIST + k] = X[k];
                t->p[P_ED_FLT] = t->p[P_ED_PIT] = t->p[P_ED_SHP] = 0;
                t->p[P_LD_FLT] = t->p[P_LD_PIT] = t->p[P_LD_SHP] = t->p[P_LD_AMP] = 0;
                t->p[P_VOICE] = V_POLY;
                t->p[P_GLIDE] = 0;
            }
            demo_pos = demo_i = demo_rd = demo_at = 0;
            demo_on = 1;
            nx_ok = demo_next(&nx_at, &nx_note, &nx_len, &nx_vel);
        }
    }
    if (!demo_on)
        return;
    t = &trk[demo_t];
    for (k = 0; k < DEMO_HELD; k++)              /* the notes whose time is up */
        if (demo_held[k].note && demo_pos >= demo_held[k].off) {
            trk_note_off(t, demo_held[k].note);
            demo_held[k].note = 0;
        }
    while (nx_ok && nx_at <= demo_pos) {         /* the notes starting in this block */
        trk_note_on(t, nx_note, nx_vel);
        demo_hold(nx_note, nx_at + nx_len);
        nx_ok = demo_next(&nx_at, &nx_note, &nx_len, &nx_vel);
    }
    demo_pos += n;
    if (!nx_ok) {                                /* the album is over once the last note ends */
        for (k = 0; k < DEMO_HELD && !demo_held[k].note; k++)
            ;
        if (k == DEMO_HELD)
            demo_end();
    }
}

/* everything that happens between two rendered blocks */
static void events_block(uint32_t n)
{
    uint32_t i, pr;
    if (transport_req == 1u) {
        seq_start();
        transport_req = 0;
    } else if (transport_req == 2u) {
        seq_stop();
        transport_req = 0;
    }
    pr = panic_req;
    panic_req = 0;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if ((pr >> i) & 1u) {
            trk_all_off(t);
            t->nheld = 0;
            t->arp_phys = 0;
            t->arp_note = 0;
        }
        if (i < NPART)
            engine_block(t);                          /* engine switch: fade, then switch (voice.c) */
        /* ARP turned off, or HOLD released with no key down: drop the latched chord */
        if ((t->armp && !t->p[P_AMODE]) || (t->aholdp && !t->p[P_AHOLD] && !t->arp_phys)) {
            t->nheld = 0;
            if (!t->p[P_AMODE])
                t->arp_phys = 0;
            if (t->arp_note) {
                trk_note_off(t, t->arp_note);
                t->arp_note = 0;
            }
        }
        t->armp = t->p[P_AMODE];
        t->aholdp = t->p[P_AHOLD];
    }
    keyboard_block();
    demo_block(n);
    while (mi_r != mi_w) {                            /* USB-MIDI (and TRS) in */
        uint32_t pkt = midi_in_q[mi_r % MQ], st = (pkt >> 8) & 0xF0u, ch = (pkt >> 8) & 0x0Fu;
        uint32_t d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        mi_r++;
        if (st == 0x90u && d2)
            input_on(midi_route(ch, d1, 1), d1, d2);
        else if (st == 0x80u || st == 0x90u)
            input_off(midi_route(ch, d1, 0), d1);
    }
    for (i = 0; i < NTRK; i++)
        seq_tick(&trk[i], n);
    for (i = 0; i < NPART; i++)
        arp_tick(&trk[i], n);
    if (song.playing)
        song.tick++;
}
