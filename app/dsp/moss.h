/* SPDX-License-Identifier: MPL-2.0 */
/* ms_render runs in the audio interrupt. Every other call posts a command that the render takes at the start of
 * its next block, so the main loop never touches engine state directly. Float only, no libm (fastmath.h). */
#pragma once
#include <stdint.h>

#define MS_SR 44100.0f

enum { MODE_LOFI, MODE_SCAPES, MODE_DRIFT, MS_NMODES };
#define MS_MAX_SCENES 7

enum { MOOD_MAJOR, MOOD_MINOR, MOOD_DORIAN, MOOD_LYDIAN, MOOD_MIXO, MS_NMOODS };

/* knob values are 0..100 */
#define MS_NMACRO 4
#define MS_NMIX 4

typedef struct {
    const char *name;
    uint8_t macro[MS_NMACRO];
    uint8_t mix[MS_NMIX];
    uint8_t tempo;                     /* bpm */
    uint8_t mood;
    uint8_t kit, pattern, beat_level;  /* the beat the scene starts with; level 0 keeps the BEAT page's */
} ms_scene_t;

extern const char *const MS_MODE_NAME[MS_NMODES];
extern const char *const MS_MOOD_NAME[MS_NMOODS];
extern const char *const MS_NOTE_NAME[12];
extern const char *const MS_MACRO_NAME[MS_NMODES][MS_NMACRO];
extern const char *const MS_MIX_NAME[MS_NMODES][MS_NMIX];
extern const uint8_t MS_NSCENES[MS_NMODES];
const ms_scene_t *ms_scene(int mode, int scene);
uint16_t ms_scale_mask(int mood, int root);   /* the mood's pitch classes from root, bit 0 = C */

void ms_init(void);
void ms_mode(int mode, int scene);
void ms_macro(int i, int v);
void ms_mix(int i, int v);
void ms_tempo(int bpm);
void ms_root(int pc);
void ms_mood(int mood);
void ms_play(int on);
void ms_freeze(int on);
void ms_new(void);                     /* reseeds the chord, loops and weather */
void ms_note(int note, int vel);       /* vel 0: off */
void ms_note_now(int note, int vel);   /* for callers already in the audio interrupt (MIDI in) */
void ms_sleep_fade(float level);       /* 0..1, applied over the master */
void ms_panic(void);
void ms_shed(void);                    /* the render ran late: drop a voice */
void ms_seed(uint32_t s);
/* i: 0 snap (FREE, SCALE, CHORD), 1 harmony 0..100, 2 voice (AUTO, EP, BELL, PAD, KALIMBA), 3 PLAY, LISTEN, HOLD (a tap plants a note that plays by itself) */
void ms_keys(int i, int v);
void ms_loop(int clear);               /* 0: advance record, play, overdub, play; 1: clear */
void ms_loop_keep(void);               /* REC: start recording, or keep what is playing and make what comes next a
                                          new layer */
void ms_loop_set(int i, int v);        /* 0 fade, 1 mutate, 2 smear: 0..100; 3 speed 0..4 */
void ms_loop_fx(int i, int v);         /* the loop's tape: 0 warble, 1 dirt, 2 age, 3 send, 0..100 */
void ms_motion(int i, int v);          /* MOTION: 0 mod, 1 random, 2 tone, 3 spread, 0..100 */
/* CHORDS: 0 chord (0 AUTO, 1..7 the degree to stay on), 1 change (STILL, SLOW, NORMAL, OFTEN), 2 path (SCENE,
 * CIRCLE, DREAM, MOODY, SONG, DRONE, MINE, BUILT), 3 color (LUSH, PLAIN, SUS, OPEN); 4 MINE's chords, n << 12 | four
 * degrees 0..6 of 3 bits each; 5 one BUILT chord, slot << 11 | length << 9 | kind << 4 | root pitch class (length
 * 0..3: half, 1, 2, 4 times the scene's chord); 6 BUILT's chord count 1..MS_NBUILT | (the slot to hear soon + 1)
 * << 4 | (the slot removed + 1) << 8, sent before the slots that moved; PLAY's live chords: 7 type (0 the key's
 * own chord on the note, else MS_CHORD_* + 1), 8 voicing (CLOSE, OPEN, WIDE, SMOOTH), 9 strum 0..100, 10 follow 0 or 1
 * (the music moves to the chord played) */
void ms_chords(int i, int v);
/* EVOLVE, 0..100: 0 amount (how far the HOME knobs wander), 1 pace, 2 chords (a chance start, now and then a
 * neighbour), 3 beat (busier and calmer) */
void ms_evolve(int i, int v);
void ms_chord_note(int note, int vel);   /* PLAY: a key's whole chord, vel 0 lets it go */
void ms_fx(int i, int v);              /* 0 delay send, 2 repeats, 3 film: 0..100; 1 time: MS_DIV_NAME */
void ms_space(int sp);                 /* MS_SPACE_NAME */
#define MS_NBUILT 8                    /* chords in a BUILT progression */
#define MS_NLAYERS 8
/* loop layer 0..MS_NLAYERS - 1: v 0..100 its level, -1 mute, -3 unmute, -2 clear, -4/-5 shift later/earlier,
 * -6 the next phrase replaces it, -7 not (unused by the UI now) */
void ms_track(int t, int v);
/* a loop as saved: times in ms, 64 notes at most (the loudest), vt = velocity 1..31 << 3 | layer 0..7; rsv[0] the
 * format, MS_SLOT_FORMAT (0: an older one, velocity 1..63 << 2 | layer 0..3) */
#define MS_SLOT_FORMAT 2
#define MS_SLOT_EVENTS 64
typedef struct {
    uint16_t t_ms, dur_ms;
    uint8_t note, vt;
} ms_slot_ev_t;
typedef struct {
    uint16_t len_ms;
    uint8_t n, mute, tempo, rsv[3];
    ms_slot_ev_t ev[MS_SLOT_EVENTS];
} ms_slot_t;
/* one transfer at a time through ms_slot_xfer: the UI waits for ms_vis.xfer_seq to change before the next */
extern ms_slot_t ms_slot_xfer;
int ms_loop_snap(void);               /* the loop playing now into ms_slot_xfer (n 0: none ready) */
int ms_loop_load(void);               /* ms_slot_xfer becomes the loop */
/* 0 kit, 1 pattern, 3 bass: by their names below; 2 level 0..100; 4 swing (0 the pattern's own, 1..9 straight to a
 * triplet feel); the bass's 5 tone, 6 glide 0..100, 7 length in sixteenths 1..8; MINE: 10 + drum its steps (a bit
 * each, as int16), 20 + drum its level | chance << 7 (0..100 each), 30 + step the bass note there (MS_BN_*), 46
 * its length (12 or 16), 47 its own swing 0..40 (%, for SWING AUTO);
 * 8 hear a drum now, 9 hear a bass note (MS_BN_*) on the chord now */
void ms_beat(int i, int v);
enum { MS_DRUM_KICK, MS_DRUM_SNARE, MS_DRUM_GHOST, MS_DRUM_HAT, MS_DRUM_OPEN, MS_DRUM_PERC, MS_NDRUMS };
enum { MS_BN_OFF, MS_BN_ROOT, MS_BN_3RD, MS_BN_5TH, MS_BN_7TH, MS_BN_OCT, MS_BN_STEP, MS_NBN };
/* a pattern's steps per drum, for MINE to start from (the scene's own for AUTO): its length, 12 or 16, and swing */
int ms_pattern_rows(int mode, int scene, int pattern, uint16_t rows[MS_NDRUMS], float *swing);
#define MS_NKITS 7
#define MS_NPATTERNS 11                 /* the last is MINE */
#define MS_NBASS 5                     /* the last is MINE */
extern const char *const MS_KIT_NAME[MS_NKITS];
extern const char *const MS_PATTERN_NAME[MS_NPATTERNS];
extern const char *const MS_BASS_NAME[MS_NBASS];
#define MS_NSPACES 13
extern const char *const MS_SPACE_NAME[MS_NSPACES];
extern const char *const MS_DIV_NAME[6];
enum { MS_LOOP_EMPTY, MS_LOOP_REC, MS_LOOP_PLAY, MS_LOOP_DUB };

void ms_render(int32_t *lr, uint32_t n, uint32_t gain_q12);

/* written by the render, read by the UI */
typedef struct {
    volatile uint32_t step;            /* sixteenths since start */
    volatile uint32_t chord_mask;      /* the chord's pitch classes, bit 0 = C */
    volatile int8_t chord_root;        /* pitch class, -1 none */
    volatile uint8_t chord_kind;       /* MS_CHORD_* */
    volatile int8_t chord_slot;        /* the BUILT chord playing, -1 none */
    volatile int8_t live_root;         /* PLAY: the last chord played, pitch class, -1 none */
    volatile uint8_t live_kind;        /* its MS_CHORD_* */
    volatile uint8_t playing, frozen;
    volatile float level;              /* slow output peak 0..1 */
    volatile float tape_ph;            /* reel rotation, turns */
    volatile float pad[4];             /* DRIFT: each pad voice's swell 0..1 */
    volatile float loop_pos[4];        /* DRIFT: each bell loop's playhead 0..1 */
    volatile uint32_t loop_notes[4];   /* DRIFT: bit per step that holds a note (<= 32 steps) */
    volatile uint8_t loop_len[4];
    volatile float wave;               /* SCAPES: surf swell 0..1 */
    volatile float gust;               /* 0..1 */
    volatile float rain;               /* 0..1 */
    volatile float fire;               /* flicker 0..1 */
    volatile uint32_t flashes;         /* counters: the UI reacts when they change */
    volatile uint32_t calls;
    volatile float call_pan;           /* last call's pan, -1..1 */
    volatile uint32_t chirps;
    volatile uint32_t notes;           /* engine notes and keys */
    volatile uint16_t recent[16];      /* recent[notes % 16] = note | layer << 8, layer 4 = the keys */
    volatile uint16_t drums[3];        /* the beat: kick, snare, hat steps, bit per sixteenth */
    volatile uint8_t beat_len, beat_step;   /* the pattern's length in sixteenths (12 or 16), where it is */
    volatile uint32_t hold[4];         /* HOLD: the notes planted, bit per MIDI note */
    volatile uint32_t lp_dots[MS_NLAYERS];   /* each loop layer's notes in 32 time bins */
    volatile int8_t lp_redo;           /* the layer armed for REDO, -1 none */
    volatile uint8_t lp_adding;        /* a phrase is going onto the playing loop as a new layer */
    volatile uint8_t lp_tracks, lp_mute, lp_track;   /* tracks holding notes and muted (bit each), the one recording */
    volatile uint32_t xfer_seq;
    volatile uint8_t lp_keep;          /* the loop is kept: it does not fade */
    volatile uint32_t lp_gen;          /* bumped when a new loop begins or one is loaded */        /* bumped when a snap or load is done */
    volatile uint8_t lp_state, lp_n;   /* MS_LOOP_*, notes held */
    volatile float lp_pos;             /* recording: seconds so far; playing: the playhead 0..1 */
    volatile float lp_len;             /* seconds */
} ms_vis_t;
extern ms_vis_t ms_vis;

enum { MS_CHORD_MAJ7, MS_CHORD_MIN7, MS_CHORD_DOM7, MS_CHORD_HALFDIM, MS_CHORD_MAJ, MS_CHORD_MIN, MS_CHORD_SUS,
       MS_CHORD_SUS2, MS_CHORD_DIM, MS_CHORD_6, MS_CHORD_MIN6, MS_CHORD_9, MS_CHORD_MIN9, MS_CHORD_MAJ9, MS_CHORD_ADD9,
       MS_CHORD_7SUS, MS_CHORD_DIM7, MS_CHORD_AUG, MS_CHORD_7B5, MS_NCHORDS };
extern const char *const MS_CHORD_SUFFIX[];
