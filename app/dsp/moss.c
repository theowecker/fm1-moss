/* SPDX-License-Identifier: MPL-2.0 */
#include "moss.h"
#include "fastmath.h"
#include "plat.h"

/* The render runs from RAM (flash reads through the XIP cache stall it), and RAM code may call only RAM code: its
 * helpers are folded into it. */
#define HOT static inline __attribute__((always_inline))

#define CTL 32                         /* frames per control tick (sequencers, events, coefficient updates) */
#define TWO_PI_SR (FM_TWO_PI / MS_SR)

const char *const MS_MODE_NAME[MS_NMODES] = {"LOFI", "SCAPES", "DRIFT"};
const char *const MS_MOOD_NAME[MS_NMOODS] = {"MAJOR", "MINOR", "DORIAN", "LYDIAN", "MIXOLYD"};
const char *const MS_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const MS_CHORD_SUFFIX[MS_NCHORDS] = {"maj7", "m7", "7", "m7b5", "", "m", "sus4", "sus2", "dim", "6", "m6", "9", "m9",
                                                "maj9", "add9", "7sus4", "dim7", "aug", "7b5"};
const char *const MS_MACRO_NAME[MS_NMODES][MS_NMACRO] = {
    {"TONE", "DUST", "WOBBLE", "REVERB"},
    {"WEATHER", "LIFE", "TONE", "REVERB"},
    {"DENSITY", "TONE", "DRIFT", "REVERB"},
};
const char *const MS_MIX_NAME[MS_NMODES][MS_NMIX] = {
    {"KEYS", "BEAT", "BASS", "RAIN"},
    {"NATURE", "LIFE", "PAD", "NOTES"},
    {"PAD", "BELLS", "BASS", "AIR"},
};
const uint8_t MS_NSCENES[MS_NMODES] = {4, 7, 5};

/*                                     macro                 mix             bpm  mood */
static const ms_scene_t SCENES[MS_NMODES][MS_MAX_SCENES] = {
    {
        {"STUDY", {55, 45, 35, 40}, {70, 65, 70, 0}, 82, MOOD_MAJOR, 2, 0, 70},
        {"RAINY", {45, 40, 30, 50}, {70, 55, 65, 42}, 72, MOOD_MINOR, 2, 0, 70},
        {"NIGHT", {32, 30, 40, 72}, {72, 38, 60, 0}, 62, MOOD_MAJOR, 2, 0, 55},
        {"TAPE", {50, 70, 70, 45}, {70, 65, 70, 0}, 86, MOOD_MAJOR, 2, 0, 70},
    },
    {
        {"FOREST", {50, 60, 60, 55}, {50, 50, 50, 55}, 70, MOOD_LYDIAN, 0, 0, 0},
        {"RAIN", {55, 30, 50, 55}, {52, 40, 52, 50}, 70, MOOD_MINOR, 0, 0, 0},
        {"SHORE", {50, 45, 55, 60}, {55, 42, 50, 50}, 70, MOOD_MIXO, 0, 0, 0},
        {"NIGHT", {40, 60, 45, 65}, {45, 48, 52, 45}, 70, MOOD_DORIAN, 0, 0, 0},
        {"STORM", {65, 0, 40, 50}, {60, 0, 50, 35}, 70, MOOD_MINOR, 0, 0, 0},
        {"HEARTH", {50, 20, 45, 45}, {52, 30, 50, 55}, 70, MOOD_MAJOR, 0, 0, 0},
        {"CREEK", {50, 55, 60, 55}, {48, 48, 48, 55}, 70, MOOD_LYDIAN, 0, 0, 0},
    },
    {
        {"HALO", {45, 55, 40, 70}, {65, 65, 55, 30}, 72, MOOD_MAJOR, 0, 0, 0},
        {"TIDE", {35, 40, 35, 75}, {70, 55, 65, 40}, 60, MOOD_DORIAN, 0, 0, 0},
        {"GLASS", {60, 65, 55, 65}, {55, 70, 45, 25}, 84, MOOD_LYDIAN, 0, 0, 0},
        {"DUSK", {30, 30, 25, 80}, {75, 50, 60, 35}, 52, MOOD_MINOR, 0, 0, 0},
        {"GARDEN", {60, 65, 60, 55}, {55, 70, 45, 50}, 96, MOOD_MAJOR, 1, 4, 45},
    },
};

const ms_scene_t *ms_scene(int mode, int scene)
{
    mode = clamp(mode, 0, MS_NMODES - 1);
    return &SCENES[mode][clamp(scene, 0, MS_NSCENES[mode] - 1)];
}

static const uint8_t SCALE[MS_NMOODS][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10},
};

uint16_t ms_scale_mask(int mood, int root)
{
    uint16_t m = 0;
    int i;
    for (i = 0; i < 7; i++)
        m |= (uint16_t)(1u << ((SCALE[clamp(mood, 0, MS_NMOODS - 1)][i] + root) % 12));
    return m;
}

enum { V_EP, V_PAD, V_BELL, V_SOFTBELL, V_KALIMBA };
enum { BIRD_NONE, BIRD_SONG, BIRD_GULL, BIRD_OWL };
enum { SNARE_FULL, SNARE_RIM };
#define B(n) (1u << (n))

typedef struct {
    uint16_t kick, snare, ghost, hat, ohat;
    uint8_t snare_kind, bars_per_chord, mel_kind;
    float swing, mel, hat16;
    int8_t prog[4];
    float pad;                         /* a warm pad under the piano at this level, 0 none */
} lofi_style_t;

static const lofi_style_t LOFI_STYLE[] = {
    {B(0) | B(7) | B(10), B(4) | B(12), B(6) | B(15), 0x5555u, 0, SNARE_FULL, 1, V_EP, 0.22f, 0.10f, 0.25f, {1, 4, 0, 5}, 0.0f},
    {B(0) | B(10), B(4) | B(12), B(11), 0x5555u, 0, SNARE_RIM, 1, V_EP, 0.18f, 0.06f, 0.10f, {0, 5, 2, 6}, 0.0f},
    {B(0) | B(10), B(8), B(3) | B(14), 0x5555u, B(6), SNARE_RIM, 2, V_EP, 0.25f, 0.06f, 0.05f, {3, 2, 5, 1}, 0.7f},
    {B(0) | B(3) | B(8) | B(10), B(4) | B(12), B(7) | B(15), 0x5555u, 0, SNARE_FULL, 1, V_KALIMBA, 0.28f, 0.16f, 0.45f,
     {3, 2, 1, 0}, 0.0f},
};

typedef struct {
    float wind, rain, rain_tone, surf, creek, fire, thunder;   /* intensities 0..1; rain_tone Hz; thunder per minute */
    uint8_t bird[2];
    float bird_rate;                   /* songs per minute, each bird */
    uint8_t crickets;
    uint8_t note_kind;
    float note_rate;                   /* notes per minute */
    float pad_bright, chord_secs;
    int8_t prog[4];
} scape_style_t;

static const scape_style_t SCAPE_STYLE[] = {
    {0.35f, 0.0f, 7000.0f, 0.0f, 0.15f, 0.0f, 0.0f, {BIRD_SONG, BIRD_SONG}, 5.0f, 0, V_KALIMBA, 6.0f, 0.6f, 24.0f,
     {0, 3, 5, 4}},
    {0.2f, 0.7f, 6500.0f, 0.0f, 0.0f, 0.0f, 0.4f, {BIRD_SONG, BIRD_NONE}, 1.5f, 0, V_EP, 5.0f, 0.35f, 28.0f, {0, 5, 3, 6}},
    {0.45f, 0.0f, 7000.0f, 0.8f, 0.0f, 0.0f, 0.0f, {BIRD_GULL, BIRD_GULL}, 2.0f, 0, V_SOFTBELL, 4.0f, 0.5f, 30.0f,
     {0, 6, 3, 0}},
    {0.15f, 0.0f, 7000.0f, 0.0f, 0.0f, 0.0f, 0.0f, {BIRD_OWL, BIRD_NONE}, 1.2f, 3, V_KALIMBA, 3.0f, 0.3f, 32.0f,
     {0, 3, 0, 4}},
    {0.8f, 1.0f, 6000.0f, 0.0f, 0.0f, 0.0f, 3.0f, {BIRD_NONE, BIRD_NONE}, 0.0f, 0, V_SOFTBELL, 1.5f, 0.25f, 36.0f,
     {0, 5, 6, 4}},
    {0.1f, 0.0f, 1600.0f, 0.0f, 0.0f, 0.8f, 0.0f, {BIRD_OWL, BIRD_NONE}, 0.6f, 2, V_EP, 5.0f, 0.45f, 26.0f, {0, 3, 1, 4}},
    {0.2f, 0.0f, 7000.0f, 0.0f, 0.8f, 0.0f, 0.0f, {BIRD_SONG, BIRD_SONG}, 4.0f, 0, V_KALIMBA, 5.0f, 0.55f, 26.0f,
     {0, 1, 0, 4}},
};

typedef struct {
    uint8_t bell_kind;
    float pad_bright;
    uint8_t loop_len[4];               /* in steps */
    uint8_t step;                      /* sixteenths per step: 4 a quarter, 2 an eighth */
    uint8_t bird;
    uint8_t lush;                      /* whole chords that crossfade, in place of single breathing notes */
    float chord_time;                  /* how long a chord lasts, against DRIFT's own */
} drift_style_t;

static const drift_style_t DRIFT_STYLE[] = {
    {V_BELL, 0.45f, {11, 14, 19, 23}, 4, BIRD_NONE, 0, 1.0f},
    {V_SOFTBELL, 0.35f, {13, 17, 21, 29}, 4, BIRD_NONE, 0, 1.0f},
    {V_KALIMBA, 0.55f, {7, 10, 13, 17}, 4, BIRD_NONE, 0, 1.0f},
    {V_EP, 0.38f, {15, 19, 26, 31}, 4, BIRD_NONE, 1, 0.3f},
    {V_KALIMBA, 0.65f, {6, 7, 9, 11}, 2, BIRD_SONG, 0, 0.35f},
};

#define SN 4096                        /* big enough to read without interpolating */
static float sint[SN + 1];
static uint32_t rng = 0x9E3779B9u;

HOT uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
HOT float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }   /* [0, 1) */
HOT float frand2(void) { return frand() * 2.0f - 1.0f; }                      /* [-1, 1) */
HOT int irand(int n) { return (int)(rnd() % (uint32_t)n); }

HOT float h_maxf(float a, float b) { return a > b ? a : b; }
HOT float h_minf(float a, float b) { return a < b ? a : b; }
HOT float h_fabsf(float x) { return x < 0.0f ? -x : x; }
HOT float h_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
HOT float h_flush(float x) { return h_fabsf(x) < 1e-20f ? 0.0f : x; }

HOT float soft_tanh(float x)
{
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

HOT float sin_t(float ph)   /* phase in turns, any value */
{
    /* the mask wraps any phase, negative ones too (truncation there is off by one entry: 1/4096 of a turn) */
    return sint[(uint32_t)(int32_t)(ph * (float)SN) & (SN - 1u)];
}

HOT float wrap1(float ph) { return ph >= 1.0f ? ph - 1.0f : ph; }
static float mtof(float n) { return 440.0f * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
static float decay_mul(float secs) { return fm_expf(-6.9f / (fm_maxf(secs, 0.0005f) * MS_SR)); }   /* -60 dB in secs */
static float onepole_a(float fc) { return 1.0f - fm_expf(-FM_TWO_PI * fc / MS_SR); }

typedef struct {
    float ic1, ic2, a1, a2, a3, k;
} svf_t;

static void svf_set(svf_t *f, float fc, float q)
{
    float g = fm_tanf(FM_PI * fm_clampf(fc, 20.0f, 18000.0f) / MS_SR);
    f->k = 1.0f / q;
    f->a1 = 1.0f / (1.0f + g * (g + f->k));
    f->a2 = g * f->a1;
    f->a3 = g * f->a2;
}
HOT float svf_run(svf_t *f, float v0, float *bp)
{
    float v3 = v0 - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    *bp = v1;
    return v2;
}
HOT float svf_lp(svf_t *f, float v0)
{
    float bp;
    return svf_run(f, v0, &bp);
}
HOT float svf_bp(svf_t *f, float v0)
{
    float bp;
    svf_run(f, v0, &bp);
    return bp;
}

static void pan_gains(float pan, float *l, float *r)   /* equal power, pan -1..1 */
{
    float a = (pan + 1.0f) * 0.125f;
    *l = sin_t(a + 0.25f);
    *r = sin_t(a);
}

enum { C_MODE, C_MACRO, C_MIX, C_TEMPO, C_ROOT, C_MOOD, C_PLAY, C_FREEZE, C_NEW, C_NOTE, C_PANIC, C_SEED, C_SHED, C_KEYSET,
       C_LOOP, C_LOOPSET, C_FX, C_SPACE, C_BEAT, C_TRACK, C_SNAP, C_LOAD, C_LOOPFX, C_MOTION, C_CHORDS, C_CHORDNOTE,
       C_EVOLVE };
typedef struct {
    uint8_t type;
    int16_t a, b;
} cmd_t;
#define NCMD 256
static cmd_t cmdq[NCMD];
static volatile uint32_t cmd_head, cmd_tail;

static int post(int type, int a, int b)
{
    uint32_t h = cmd_head;
    if (h - cmd_tail >= NCMD)
        return -1;
    cmdq[h % NCMD].type = (uint8_t)type;
    cmdq[h % NCMD].a = (int16_t)a;
    cmdq[h % NCMD].b = (int16_t)b;
    __asm__ volatile("" ::: "memory");  /* the slot must be written before head publishes it to the render */
    cmd_head = h + 1;
    return 0;
}

ms_vis_t ms_vis;

static struct {
    int mode, scene, next_mode, next_scene;
    int macro[MS_NMACRO], mix[MS_NMIX];
    int tempo, root, mood;
    int playing, frozen, switching;
    float gen_fade, sleep_target, sleep;
    int fx[4];                         /* delay send, time (MS_DIV_NAME index), repeats, film */
    int space;                         /* SP_* */
    float layer[6];                    /* gain per layer; [SRC_KEYS], [SRC_LOOP] are never faded */
    float layer_to[MS_NMIX];           /* where the MIX layers glide to, so a change never steps */
} st;

enum { SNAP_FREE, SNAP_SCALE, SNAP_CHORD };
enum { KV_AUTO, KV_EP, KV_BELL, KV_PAD, KV_KALIMBA };
static struct {
    int snap, voice, listen, hold;
    int8_t seed[8];                    /* HOLD: the notes planted, -1 none */
    uint32_t seed_age[8], seed_clock;  /* when each was planted, to find the oldest */
    float seed_t[8];                   /* seconds until each plays again */
    float harmony;
    float energy;                      /* 0..1, raised by playing the keys, makes the generators busier */
    int8_t held[128][3];               /* per held key: sounding note and up to two harmony notes, -1 none */
} kb;


enum { SP_HALL, SP_PLATE, SP_CLOUD, SP_SHIMMER, SP_TAPE, SP_REVERSE, SP_DEEP, SP_DUB, SP_DREAM, SP_RADIO, SP_WATER,
       SP_GLITCH, SP_PULSE, SP_N };
const char *const MS_SPACE_NAME[SP_N] = {"HALL", "PLATE", "CLOUD", "SHIMMER", "TAPE", "REVERSE", "DEEP", "DUB", "DREAM",
                                          "RADIO", "WATER", "GLITCH", "PULSE"};
const char *const MS_DIV_NAME[6] = {"1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
static const float DIV_BEATS[6] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f};

/* derived from the knobs by apply() */
static struct {
    float tone_fc, rev_mix, rev_rt, rev_size, echo_mix, echo_fb;
    float dust, wobble;
    float weather, life;
    float density, drift;
    float wind, rain, surf, creek, fire, thunder_rate, bird_rate, note_rate;
    float rain_tone;
    float pad_bright;
    float film;
    float crush_bits;                  /* LOFI's DUST past 0.4: the levels the bit crusher keeps, 0 off */
    float shimmer, echo_wow, echo_to_rev, rev_mod, damp_hz, reverse;
    /* what a space does past the reverb: the whole mix high-passed, chorused, pulsed, squeezed into a radio, wobbled;
     * the shimmer an octave down; random delay jumps; a cap on the echoes' brightness and a floor on their thinness */
    float hp_a, chorus, chorus_rate, trem, radio, wob_add, alg_rand, echo_lp_max, echo_hp_min;
    int sh_down;
} k;

static struct {
    int degree;                        /* 0..6: of the key, or for a BUILT chord its root's place in hm.scale */
    int pending;                       /* chord id asked for by the player's notes or a new chord, -1 none */
    int prog_i, bar_in_chord;
    int tones[5];                      /* semitones above the key root: root, 3rd, 5th, 7th, 9th */
    int slot;                          /* the BUILT chord playing, -1 a chord of the key */
    int live;                          /* the chord playing is the one PLAY played */
    uint8_t scale[7];                  /* the scale the music plays now, semitones above the key root, rising */
    uint16_t scale_mask;               /* the same as pitch classes */
} hm;

/* CHORDS: the player's say over the harmony. lock: -1 the scene picks, else the degree it stays on; rate: how often
 * chords move (STILL, SLOW, NORMAL, OFTEN); path: the scene's own progression or one of PATHS; color: LUSH, PLAIN,
 * SUS, OPEN */
static struct {
    int lock, rate, path, color;
    int8_t mine[4];                    /* PATH MINE: the player's own progression, mine_n chords */
    int mine_n;
    uint8_t b_root[MS_NBUILT], b_kind[MS_NBUILT], b_len[MS_NBUILT];   /* PATH BUILT: any chord, of any length */
    int b_n;
} hc = {-1, 2, 0, 0, {0, 3, 4, 0}, 1, {0}, {MS_CHORD_MAJ7}, {1}, 1};
enum { PATH_MINE = 6, PATH_BUILT = 7 };
/* chord ids: 0..6 a degree of the key, CH_BUILT + slot a BUILT chord, CH_LIVE the last chord PLAY played */
#define CH_BUILT 16
#define CH_LIVE (CH_BUILT + MS_NBUILT)
/* each kind's root, 3rd, 5th, 7th and 9th roles, as the scenes voice them; a triad doubles its root and 5th */
static const int8_t KIND_TONES[MS_NCHORDS][5] = {
    {0, 4, 7, 11, 14}, {0, 3, 7, 10, 14}, {0, 4, 7, 10, 19}, {0, 3, 6, 10, 12}, {0, 4, 7, 12, 19}, {0, 3, 7, 12, 19},
    {0, 5, 7, 12, 19}, {0, 2, 7, 12, 19}, {0, 3, 6, 12, 18}, {0, 4, 7, 9, 14}, {0, 3, 7, 9, 14}, {0, 4, 7, 10, 14},
    {0, 3, 7, 10, 14}, {0, 4, 7, 11, 14}, {0, 4, 7, 14, 12}, {0, 5, 7, 10, 14}, {0, 3, 6, 9, 12}, {0, 4, 8, 12, 16},
    {0, 4, 6, 10, 14},
};
static const float LEN_MUL[4] = {0.5f, 1.0f, 2.0f, 4.0f};
static const int8_t PATHS[5][4] = {
    {5, 1, 4, 0},                      /* CIRCLE: vi ii V I */
    {0, 5, 3, 4},                      /* DREAM: I vi IV V */
    {5, 3, 0, 4},                      /* MOODY: vi IV I V */
    {0, 4, 5, 3},                      /* SONG: I V vi IV */
    {0, 3, 0, 3},                      /* DRONE: I IV */
};
/* EVOLVE: over minutes the HOME knobs wander a little around where they are set, the scene's chords now and then
 * take a neighbour, a scene starts on a chord of its path picked by chance, and the beat gets busier and calmer */
static struct {
    float amount, pace, chords, beat;
    float off[MS_NMACRO], target[MS_NMACRO];   /* knob points added to the HOME knobs, and where each is heading */
    float moff[MS_NMIX], mtarget[MS_NMIX];     /* the same for the MIX levels */
    int8_t applied[MS_NMACRO + MS_NMIX];       /* the points the sound has now */
    float t, apply_t;                  /* seconds to the next new target, and to the next apply */
    float busy, busy_target;
} evo = {0.3f, 0.4f, 0.3f, 0.3f, {0}, {0}, {0}, {0}, {0}, 20.0f, 0.0f, 0.0f, 0.0f};

static const float RATE_MUL[4] = {0.0f, 2.0f, 1.0f, 0.5f};   /* times the scene's chord length; 0: never by itself */

static int deg_semis(int deg)   /* any octave, including negative degrees */
{
    int oct = deg >= 0 ? deg / 7 : -((6 - deg) / 7);
    return hm.scale[deg - oct * 7] + 12 * oct;
}

static void scale_from(uint16_t mask)
{
    int o, n = 0;
    hm.scale_mask = mask;
    for (o = 0; o < 12 && n < 7; o++)
        if (mask >> ((st.root + o) % 12) & 1u)
            hm.scale[n++] = (uint8_t)o;
}

/* a BUILT chord: its tones, and the key bent just enough to hold them, so the melodies, the snapped keys and
 * HARMONY follow it. Each tone the key lacks takes the place of the nearest neighbour not in the chord, the one above
 * first: a note a semitone above a chord tone is the one that grates */
static void chord_any(int root_pc, int kind, int slot)
{
    int i, r = (root_pc - st.root + 12) % 12;
    uint16_t key = ms_scale_mask(st.mood, st.root), tones = 0;
    uint32_t mask = 0;
    hm.slot = slot;
    hm.live = slot < 0;
    for (i = 0; i < 5; i++) {
        hm.tones[i] = r + KIND_TONES[kind][i];
        tones |= (uint16_t)(1u << ((st.root + hm.tones[i]) % 12));
    }
    for (i = 0; i < 12; i++) {
        int d;
        if (!(tones >> i & 1u) || key >> i & 1u)
            continue;
        for (d = 1; d < 6; d++) {
            int lo = (i + 12 - d) % 12, hi = (i + d) % 12;
            if (key >> hi & 1u && !(tones >> hi & 1u)) {
                key &= (uint16_t)~(1u << hi);
                break;
            }
            if (key >> lo & 1u && !(tones >> lo & 1u)) {
                key &= (uint16_t)~(1u << lo);
                break;
            }
        }
        key |= (uint16_t)(1u << i);
    }
    scale_from(key);
    for (i = 0; i < 7; i++)
        if (hm.scale[i] == r)
            hm.degree = i;
    for (i = 0; i < 4; i++)
        mask |= 1u << ((st.root + hm.tones[i]) % 12);
    ms_vis.chord_kind = (uint8_t)kind;
    ms_vis.chord_mask = mask;
    ms_vis.chord_root = (int8_t)root_pc;
    ms_vis.chord_slot = (int8_t)slot;
}

static int chord_id(void) { return hm.live ? CH_LIVE : hm.slot >= 0 ? CH_BUILT + hm.slot : hm.degree; }

/* PLAY: the live chords. Each key held has its chord's notes, started one by one when STRUM spreads them */
#define NLIVE 4
static struct {
    int type, voicing, follow;
    float strum;
    int root, kind;                    /* the last chord played, for FOLLOW */
    float centre;                      /* where SMOOTH keeps the voices */
    int8_t key[NLIVE], note[NLIVE][6], n[NLIVE], started[NLIVE];
    float wait[NLIVE], vel[NLIVE];
} cp;

static void chord_set(int degree)
{
    int i, third, fifth, seventh;
    uint32_t mask = 0;
    if (degree == CH_LIVE) {
        chord_any(cp.root, cp.kind, -1);
        return;
    }
    if (degree >= CH_BUILT) {
        int j = (degree - CH_BUILT) % hc.b_n;
        chord_any(hc.b_root[j], hc.b_kind[j], j);
        return;
    }
    hm.slot = -1;
    hm.live = 0;
    ms_vis.chord_slot = -1;
    scale_from(ms_scale_mask(st.mood, st.root));
    hm.degree = ((degree % 7) + 7) % 7;
    for (i = 0; i < 5; i++)
        hm.tones[i] = deg_semis(hm.degree + 2 * i);
    third = hm.tones[1] - hm.tones[0];
    fifth = hm.tones[2] - hm.tones[0];
    seventh = hm.tones[3] - hm.tones[0];
    if (hc.color == 1) {                 /* PLAIN: the 7th and 9th become the root and 5th an octave up */
        hm.tones[3] = hm.tones[0] + 12;
        hm.tones[4] = hm.tones[2] + 12;
        ms_vis.chord_kind = fifth == 6 ? MS_CHORD_DIM : third == 4 ? MS_CHORD_MAJ : MS_CHORD_MIN;
    } else if (hc.color >= 2 && fifth == 7
               && ms_scale_mask(st.mood, st.root) >> ((st.root + hm.tones[0] + (hc.color == 2 ? 5 : 2)) % 12) & 1u) {
        /* SUS and OPEN: the 3rd moves to a perfect 4th or a whole tone, where the key has it; else the chord is kept */
        hm.tones[1] = hm.tones[0] + (hc.color == 2 ? 5 : 2);
        ms_vis.chord_kind = hc.color == 2 ? MS_CHORD_SUS : MS_CHORD_SUS2;
    } else if (third == 4 && seventh == 11)
        ms_vis.chord_kind = MS_CHORD_MAJ7;
    else if (third == 4)
        ms_vis.chord_kind = MS_CHORD_DOM7;
    else if (fifth == 6)
        ms_vis.chord_kind = MS_CHORD_HALFDIM;
    else
        ms_vis.chord_kind = MS_CHORD_MIN7;
    for (i = 0; i < 4; i++)
        mask |= 1u << ((st.root + hm.tones[i]) % 12);
    ms_vis.chord_mask = mask;
    ms_vis.chord_root = (int8_t)((st.root + hm.tones[0]) % 12);
}

/* the chord id the progression goes to next, from the scene's own (prog, NULL for DRIFT's wandering) or CHORDS' */
static int chord_next(const int8_t *prog, int peek)
{
    static const int8_t MOVE[] = {3, 4, -3, 2, 5, -2, 3, 4};   /* mostly fourths and fifths, sometimes thirds */
    int i = hc.path == PATH_MINE ? (hm.prog_i + 1) % hc.mine_n
          : hc.path == PATH_BUILT ? (hm.prog_i + 1) % hc.b_n : (hm.prog_i + 1) & 3;
    if (hc.lock >= 0)
        return hc.lock;
    if (!peek && hm.pending >= 0)
        return hm.pending;
    if (!hc.rate)                        /* STILL: the chord comes round again unchanged */
        return chord_id();
    if (!peek)
        hm.prog_i = i;
    if (hc.path == PATH_BUILT)
        return CH_BUILT + i;
    if (hc.path == PATH_MINE)
        return hc.mine[i];
    if (hc.path > 0)
        return PATHS[hc.path - 1][i];
    if (prog && !peek && frand() < 0.25f * evo.chords)   /* EVOLVE: now and then a neighbour sharing two notes */
        return (prog[i] + (frand() < 0.5f ? 2 : 5)) % 7;
    return prog ? prog[i] : hm.degree + MOVE[irand(8)];
}

/* how long until the next chord, from the scene's own length: CHANGE's STILL waits until CHORD asks for one */
static float chord_len(void) { return hm.slot >= 0 && !hm.live ? LEN_MUL[hc.b_len[hm.slot]] : 1.0f; }
static float chord_wait(float secs) { return hc.rate == 0 ? 1e9f : secs * RATE_MUL[hc.rate] * chord_len(); }

/* the first chord a scene starts on */
static int chord_first(int scene_first)
{
    return hc.lock >= 0 ? hc.lock : hc.path == PATH_BUILT ? CH_BUILT : hc.path == PATH_MINE ? hc.mine[0]
         : hc.path > 0 ? PATHS[hc.path - 1][0] : scene_first;
}

/* where a scene's path starts: its first chord, or with EVOLVE's CHORDS now and then another of the four, so a scene
 * is not the same each time. The player's own paths, and a held chord, start where they are */
static int start_index(void)
{
    return hc.lock < 0 && hc.path < PATH_MINE && frand() < evo.chords * 1.5f ? irand(4) : 0;
}

static int chord_at(const int8_t *prog, int i)
{
    static const int8_t WANDER[4] = {0, 3, 5, 4};   /* DRIFT has no path of its own: I, IV, vi or V */
    return hc.lock >= 0 || hc.path >= PATH_MINE ? chord_first(prog ? prog[0] : 0)
         : hc.path > 0 ? PATHS[hc.path - 1][i] : prog ? prog[i] : WANDER[i];
}

/* the root of chord id, semitones above the key root */
static int chord_root_semis(int id)
{
    int pc = id == CH_LIVE ? cp.root : id >= CH_BUILT ? hc.b_root[(id - CH_BUILT) % hc.b_n] : -1;
    return pc >= 0 ? (pc - st.root + 12) % 12 : SCALE[st.mood][((id % 7) + 7) % 7];
}

static int chord_note_near(int i, int centre)
{
    int n = st.root + hm.tones[i % 5];
    while (n < centre - 6)
        n += 12;
    while (n > centre + 5)
        n -= 12;
    return n;
}

static int scale_note_near(int deg, int centre)
{
    int n = st.root + deg_semis(deg);
    while (n < centre - 6)
        n += 12;
    while (n > centre + 5)
        n -= 12;
    return n;
}

enum { ST_OFF, ST_ATK, ST_DEC, ST_REL };
#define NV 14
#define SRC_KEYS 4
#define SRC_LOOP 5
typedef struct {
    uint8_t kind, stage, layer;
    int8_t note;
    int32_t gate;                      /* samples until release, -1 held until a note off */
    float pc, ic, pm, im, pc2, ic2, fb1, fb2;
    float env, a_inc, d_mul, s_lvl, r_mul;
    float idx, idx_mul, idx_floor;
    float amp, pl, pr, lp, lp2, lp_a;
    float rev, echo;
    float out_l, out_r;                /* the last sample it gave: a cut hands it to the declick */
    uint32_t age;
} voice_t;
static voice_t vo[NV];
static uint32_t vo_clock;

/* a sound cut short leaves a step in the output; its last sample is handed here and fades over a few ms */
static float dk_l, dk_r;
static float dkl_l, dkl_r;             /* the loop's own: its voices sound through its tape, so their cut must too */

static void voice_cut(voice_t *v)
{
    if (v->stage != ST_OFF) {
        if (v->layer == SRC_LOOP) {
            dkl_l += v->out_l;
            dkl_r += v->out_r;
        } else {
            dk_l += v->out_l;
            dk_r += v->out_r;
        }
    }
    v->stage = ST_OFF;
    v->out_l = v->out_r = 0.0f;
}

/* the loop's own tape: WARBLE bends its pitch, DIRT drives it, AGE wears it like a copy of a copy (darker, thinner,
 * narrower, quieter), SEND feeds the reverb and delay */
static struct {
    float warble, dirt, age, send;
    float wmul, wph, wph2;             /* the pitch bend now, its two slow phases */
    float age_a, age_hp, width, wear, drive, makeup;
    float lp_l, lp_r, hp_l, hp_r;
} lfx = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

static void lfx_set(int i, int v)
{
    float x = (float)v * 0.01f;
    if (i == 0)
        lfx.warble = x;
    else if (i == 1)
        lfx.dirt = x;
    else if (i == 2)
        lfx.age = x;
    else
        lfx.send = 2.0f * x;
    lfx.age_a = onepole_a(14000.0f * fm_exp2f(-4.5f * lfx.age));
    lfx.age_hp = onepole_a(20.0f + 380.0f * lfx.age * lfx.age);
    lfx.width = 1.0f - 0.85f * lfx.age;
    lfx.wear = 1.0f - 0.5f * lfx.age;
    lfx.drive = 1.0f + 7.0f * lfx.dirt;
    lfx.makeup = 1.0f / (1.0f + 2.5f * lfx.dirt);
}

#define ENGINE_MAX 10                  /* the music's own voices at most: the rest are kept for the player */

static voice_t *voice_alloc(int layer)
{
    int i, best = -1, busy = 0;
    if (layer < SRC_KEYS) {              /* past its share, the music takes over its own quietest voice */
        for (i = 0; i < NV; i++)
            if (vo[i].stage != ST_OFF && vo[i].layer < SRC_KEYS)
                busy++;
        if (busy >= ENGINE_MAX) {
            for (i = 0; i < NV; i++)
                if (vo[i].stage != ST_OFF && vo[i].layer < SRC_KEYS
                    && (best < 0 || vo[i].env * vo[i].amp < vo[best].env * vo[best].amp))
                    best = i;
            voice_cut(&vo[best]);
            return &vo[best];
        }
    }
    for (i = 0; i < NV; i++)
        if (vo[i].stage == ST_OFF)
            return &vo[i];
    for (i = 0; i < NV; i++)            /* prefer the quietest released voice */
        if (vo[i].stage == ST_REL && (best < 0 || vo[i].env < vo[best].env))
            best = i;
    if (best >= 0) {
        voice_cut(&vo[best]);
        return &vo[best];
    }
    /* steal the quietest, not the oldest: the oldest is often a loud pad */
    for (i = 0; i < NV; i++)
        if (best < 0 || vo[i].env * vo[i].amp < vo[best].env * vo[best].amp)
            best = i;
    voice_cut(&vo[best]);
    return &vo[best];
}

/* attack and release in seconds; gate in samples (-1 held) */
static voice_t *voice_start(int kind, int note, float vel, int layer, int32_t gate, float atk, float rel, float pan)
{
    voice_t *v;
    v = voice_alloc(layer);
    float f = mtof((float)note), bright;
    v->kind = (uint8_t)kind;
    v->note = (int8_t)note;
    v->layer = (uint8_t)layer;
    v->gate = gate;
    v->ic = f / MS_SR;
    v->pc = v->pm = v->pc2 = 0.0f;
    v->fb1 = v->fb2 = 0.0f;
    v->env = 0.0f;
    v->stage = ST_ATK;
    v->a_inc = 1.0f / (fm_maxf(atk, 0.001f) * MS_SR);
    v->r_mul = decay_mul(rel);
    v->age = ++vo_clock;
    v->lp = v->lp2 = 0.0f;
    pan_gains(pan, &v->pl, &v->pr);
    switch (kind) {
    case V_EP:
        v->im = v->ic;
        v->idx = 0.06f + 0.22f * vel;
        v->idx_floor = 0.025f;
        v->idx_mul = decay_mul(1.2f);
        v->s_lvl = 0.0f;
        v->d_mul = decay_mul(3.5f - 1.2f * vel);
        v->amp = 0.17f * (0.35f + 0.65f * vel);
        v->lp_a = onepole_a(2400.0f + 3500.0f * vel);
        v->rev = 0.35f;
        v->echo = 0.25f;
        break;
    case V_PAD:
        v->ic2 = v->ic * 1.0059f;
        v->pc2 = 0.37f;
        v->s_lvl = 1.0f;
        v->d_mul = 1.0f;
        v->amp = 0.085f * (0.5f + 0.5f * vel);
        bright = k.pad_bright;
        v->lp_a = onepole_a(fm_minf(500.0f + 3200.0f * bright * bright + f * 2.0f, 12000.0f));
        v->rev = 0.85f;
        v->echo = 0.04f;
        break;
    case V_BELL:
    case V_SOFTBELL:
        v->im = v->ic * (kind == V_BELL ? 3.5f : 2.0f);
        v->idx = (kind == V_BELL ? 0.06f : 0.04f) + (kind == V_BELL ? 0.16f : 0.08f) * vel;
        v->idx_floor = 0.0f;
        v->idx_mul = decay_mul(kind == V_BELL ? 1.6f : 2.2f);
        v->s_lvl = 0.0f;
        v->d_mul = decay_mul(kind == V_BELL ? 4.5f : 6.0f);
        v->amp = 0.13f * (0.3f + 0.7f * vel);
        v->lp_a = onepole_a(6000.0f);
        v->rev = 0.7f;
        v->echo = 0.4f;
        break;
    case V_KALIMBA:
        v->im = v->ic * 3.0f;
        v->idx = 0.08f + 0.18f * vel;
        v->idx_floor = 0.0f;
        v->idx_mul = decay_mul(0.18f);
        v->s_lvl = 0.0f;
        v->d_mul = decay_mul(1.3f);
        v->amp = 0.16f * (0.35f + 0.65f * vel);
        v->lp_a = onepole_a(5000.0f);
        v->rev = 0.5f;
        v->echo = 0.35f;
        break;
    }
    ms_vis.recent[(ms_vis.notes + 1u) & 15u] = (uint16_t)(note | layer << 8);
    ms_vis.notes++;
    return v;
}

static void voice_release_note(int note, int layer)
{
    int i;
    for (i = 0; i < NV; i++)
        if (vo[i].stage != ST_OFF && vo[i].stage != ST_REL && vo[i].note == note && vo[i].layer == layer)
            vo[i].stage = ST_REL;
}

static void voices_release(int keep_keys)
{
    int i;
    for (i = 0; i < NV; i++)
        if (vo[i].stage != ST_OFF && !(keep_keys && vo[i].layer >= SRC_KEYS))
            vo[i].stage = ST_REL;
}

HOT float voice_tick(voice_t *v, float wm)
{
    float s, m;
    switch (v->stage) {
    case ST_ATK:
        v->env += v->a_inc;
        if (v->env >= 1.0f) {
            v->env = 1.0f;
            v->stage = ST_DEC;
        }
        break;
    case ST_DEC:
        v->env = v->s_lvl + (v->env - v->s_lvl) * v->d_mul;
        if (v->s_lvl == 0.0f && v->env < 0.0005f)
            v->stage = ST_OFF;
        break;
    case ST_REL:
        v->env *= v->r_mul;
        if (v->env < 0.0005f)
            v->stage = ST_OFF;
        break;
    default:
        return 0.0f;
    }
    v->pc = wrap1(v->pc + v->ic * wm);
    if (v->kind == V_PAD) {
        /* two detuned feedback-FM oscillators, a soft saw */
        v->pc2 = wrap1(v->pc2 + v->ic2 * wm);
        v->fb1 = sin_t(v->pc + 0.11f * v->fb1);
        v->fb2 = sin_t(v->pc2 + 0.11f * v->fb2);
        s = 0.5f * (v->fb1 + v->fb2);
    } else {
        v->pm = wrap1(v->pm + v->im * wm);
        v->idx = v->idx_floor + (v->idx - v->idx_floor) * v->idx_mul;
        m = sin_t(v->pm);
        s = sin_t(v->pc + v->idx * m);
    }
    v->lp += v->lp_a * (s - v->lp);
    v->lp2 += v->lp_a * (v->lp - v->lp2);
    return v->lp2 * v->env * v->amp;
}

/* monophonic, gliding */
static struct {
    float ph, inc, target, glide, env, env_target, env_rate, amp;
    float fm;                          /* TONE: how much the sine bends itself, round to growl */
    int32_t gate;
} bass = {.fm = 0.035f};

static void bass_note(int note, float vel, int32_t gate, float glide_secs, float atk)
{
    bass.target = mtof((float)note) / MS_SR;
    if (bass.env < 0.01f || glide_secs <= 0.0f)
        bass.inc = bass.target;
    bass.glide = onepole_a(1.0f / (FM_TWO_PI * fm_maxf(glide_secs, 0.002f)));
    bass.env_target = 1.0f;
    bass.env_rate = onepole_a(1.0f / (FM_TWO_PI * atk));
    bass.amp = 0.32f * (0.5f + 0.5f * vel);
    bass.gate = gate;
}

HOT float bass_tick(void)
{
    if (bass.env < 0.0002f && bass.env_target == 0.0f)
        return 0.0f;
    bass.inc += bass.glide * (bass.target - bass.inc);
    bass.env += bass.env_rate * (bass.env_target - bass.env);
    bass.ph = wrap1(bass.ph + bass.inc);
    return sin_t(bass.ph + bass.fm * sin_t(bass.ph)) * bass.env * bass.amp;
}

enum { EV_NOTE, EV_KICK, EV_SNARE, EV_GHOST, EV_HAT, EV_OHAT, EV_PERC };
typedef struct {
    int32_t t;
    uint8_t type, kind, layer;
    int8_t note;
    float vel, pan;
    int32_t gate;
} evt_t;
#define NEVT 32
static evt_t evq[NEVT];
static uint8_t ev_used[NEVT];

static void ev_add(int32_t delay, int type, int kind, int note, float vel, int layer, int32_t gate, float pan)
{
    int i;
    for (i = 0; i < NEVT; i++)
        if (!ev_used[i]) {
            evq[i].t = delay;
            evq[i].type = (uint8_t)type;
            evq[i].kind = (uint8_t)kind;
            evq[i].note = (int8_t)note;
            evq[i].vel = vel;
            evq[i].layer = (uint8_t)layer;
            evq[i].gate = gate;
            evq[i].pan = pan;
            ev_used[i] = 1;
            return;
        }
}

enum { KIT_OFF, KIT_SOFT, KIT_LOFI, KIT_BRUSH, KIT_PULSE, KIT_TICK, KIT_WOOD, KIT_N };
enum { BASS_AUTO, BASS_OFF, BASS_ROOT, BASS_WALK, BASS_MINE, BASS_N };
#define PAT_N 11
#define PAT_MINE 10
const char *const MS_KIT_NAME[KIT_N] = {"OFF", "SOFT", "LOFI", "BRUSH", "PULSE", "TICK", "WOOD"};
const char *const MS_PATTERN_NAME[PAT_N] = {"AUTO", "BOOMBAP", "HALF", "FOUR", "BOSSA", "HEART", "SHUFFLE", "BROKEN",
                                            "WALTZ", "SPARSE", "MINE"};
const char *const MS_BASS_NAME[BASS_N] = {"AUTO", "OFF", "ROOT", "WALK", "MINE"};

/* each kit is the same four voices tuned differently: kick, snare (or rim, brush, breath, clap), hat (or shaker) and
 * an FM ping (ride, woodblock, blip) */
typedef struct {
    float k_f0, k_sweep, k_pdec, k_dec, k_amp, k_click;
    float s_tone, s_tamp, s_tdec, s_namp, s_ndec, s_bp, s_q;
    float h_amp, h_dec, h_oamp, h_odec;
    float p_f, p_ratio, p_idx, p_dec, p_amp;
    float bus;                         /* the kit's one-pole: lower is duller */
} kit_t;

static const kit_t KITS[KIT_N] = {
    {0},
    {50, 50, 0.06f, 0.35f, 0.45f, 0.03f, 500, 0.06f, 0.04f, 0.05f, 0.05f, 2500, 3.0f, 0.05f, 0.07f, 0.05f, 0.25f,
     0, 0, 0, 0, 0, 0.35f},
    {46, 105, 0.12f, 0.55f, 0.55f, 0.12f, 190, 0.16f, 0.18f, 0.22f, 0.42f, 1900, 0.8f, 0.08f, 0.09f, 0.07f, 0.6f,
     0, 0, 0, 0, 0, 0.6f},
    {55, 40, 0.05f, 0.3f, 0.4f, 0.02f, 0, 0, 0.01f, 0.14f, 0.35f, 3500, 0.6f, 0.03f, 0.05f, 0.04f, 0.3f,
     480, 2.76f, 0.6f, 0.9f, 0.05f, 0.45f},
    {42, 30, 0.08f, 0.9f, 0.5f, 0, 0, 0, 0.01f, 0.07f, 0.5f, 900, 0.7f, 0.02f, 0.05f, 0.02f, 0.2f,
     880, 1.0f, 0.0f, 0.4f, 0.03f, 0.3f},
    {60, 200, 0.01f, 0.12f, 0.25f, 0.12f, 1800, 0.06f, 0.015f, 0.035f, 0.03f, 5000, 2.0f, 0.045f, 0.02f, 0.035f, 0.08f,
     2400, 1.5f, 0.3f, 0.05f, 0.045f, 0.9f},
    {65, 60, 0.03f, 0.25f, 0.3f, 0.04f, 0, 0, 0.01f, 0.09f, 0.12f, 1200, 2.5f, 0.04f, 0.06f, 0.04f, 0.2f,
     900, 1.5f, 0.3f, 0.08f, 0.07f, 0.7f},
};

typedef struct {
    uint16_t kick, snare, ghost, hat, ohat, perc;
    uint8_t len;
    float swing, hat16;
} pattern_t;

static const pattern_t PATTERNS[PAT_N] = {
    {0},
    {B(0) | B(7) | B(10), B(4) | B(12), B(6) | B(15), 0x5555u, 0, 0, 16, 0.2f, 0.2f},
    {B(0) | B(10), B(8), B(3) | B(14), 0x5555u, B(6), 0, 16, 0.15f, 0.05f},
    {B(0) | B(4) | B(8) | B(12), B(4) | B(12), 0, B(2) | B(6) | B(10) | B(14), 0, 0, 16, 0.0f, 0.1f},
    {B(0) | B(3) | B(8) | B(11), 0, B(6) | B(13), 0xFFFFu, 0, B(0) | B(3) | B(6) | B(10) | B(12), 16, 0.05f, 0.0f},
    {B(0) | B(2), 0, 0, 0, 0, B(8), 16, 0.0f, 0.0f},
    {B(0) | B(10), B(4) | B(12), B(7), B(0) | B(3) | B(4) | B(7) | B(8) | B(11) | B(12) | B(15), 0, 0, 16, 0.33f, 0.0f},
    {B(0) | B(6) | B(11), B(4) | B(13), B(9), 0x5555u, 0, B(14), 16, 0.1f, 0.15f},
    {B(0), B(4) | B(8), 0, B(2) | B(6) | B(10), 0, B(0), 12, 0.1f, 0.0f},
    {B(0), B(12), 0, B(4) | B(12), 0, B(6) | B(14), 16, 0.1f, 0.0f},
    {0},                               /* MINE: from ub */
};

static struct {
    int kit, pattern, bass;
    float level;
    float swing;                       /* < 0: the pattern's own */
    float bass_len, bass_glide;        /* in sixteenths, in seconds */
} bt = {0, 0, 0, 1.0f, -1.0f, 3.0f, 0.0f};

/* MINE: the player's own beat and bass line */
static struct {
    uint16_t row[MS_NDRUMS];
    float level[MS_NDRUMS], chance[MS_NDRUMS];
    uint8_t bass[16];
    uint8_t len;                       /* 16, or 12 for a bar of three */
} ub = {{0}, {0.9f, 0.9f, 0.3f, 0.7f, 0.6f, 0.7f}, {1, 1, 1, 1, 1, 1}, {0}, 16};

static struct {
    float k_ph, k_f, k_penv, k_amp, k_pmul, k_amul, k_click;
    float s_ph, s_tamp, s_namp, s_tmul, s_nmul, s_tinc;
    float h_amp, h_mul, h_lp, h_hp;
    float p_pc, p_pm, p_ic, p_im, p_idx, p_amp, p_mul;
    svf_t s_bp;
    float bus_lp;
    int snare_kind;
} dr;

HOT const kit_t *kit(void) { return &KITS[clamp(bt.kit, 1, KIT_N - 1)]; }

static void drum_kick(float vel)
{
    const kit_t *kt = kit();
    dr.k_ph = 0.0f;
    dr.k_penv = 1.0f;
    dr.k_amp = kt->k_amp * vel;
    dr.k_pmul = decay_mul(kt->k_pdec);
    dr.k_amul = decay_mul(kt->k_dec);
    dr.k_click = kt->k_click * vel;
}

static void drum_snare(float vel)
{
    const kit_t *kt = kit();
    int rim = bt.kit == KIT_LOFI && dr.snare_kind == SNARE_RIM;
    dr.s_ph = 0.0f;
    dr.s_tinc = (rim ? 420.0f : kt->s_tone) / MS_SR;
    dr.s_tamp = (rim ? 0.10f : kt->s_tamp) * vel;
    dr.s_namp = (rim ? 0.10f : kt->s_namp) * vel;
    dr.s_tmul = decay_mul(rim ? 0.06f : kt->s_tdec);
    dr.s_nmul = decay_mul(rim ? 0.09f : kt->s_ndec);
    svf_set(&dr.s_bp, rim ? 1300.0f : kt->s_bp, rim ? 3.5f : kt->s_q);
}

static void drum_hat(float vel, int open)
{
    const kit_t *kt = kit();
    dr.h_amp = (open ? kt->h_oamp : kt->h_amp) * vel;
    dr.h_mul = decay_mul(open ? kt->h_odec : kt->h_dec);
}

static void drum_perc(float vel, float pitch)
{
    const kit_t *kt = kit();
    if (kt->p_amp <= 0.0f)
        return;
    dr.p_pc = dr.p_pm = 0.0f;
    dr.p_ic = kt->p_f * pitch / MS_SR;
    dr.p_im = dr.p_ic * kt->p_ratio;
    dr.p_idx = kt->p_idx;
    dr.p_amp = kt->p_amp * vel;
    dr.p_mul = decay_mul(kt->p_dec);
}

HOT void drums_tick(float n, float *l, float *r)
{
    float k = 0.0f, s = 0.0f, h = 0.0f, x;
    if (dr.k_amp > 0.0001f) {
        dr.k_f = kit()->k_f0 + kit()->k_sweep * dr.k_penv;
        dr.k_penv *= dr.k_pmul;
        dr.k_ph = wrap1(dr.k_ph + dr.k_f / MS_SR);
        k = sin_t(dr.k_ph) * dr.k_amp + n * dr.k_click;
        dr.k_amp *= dr.k_amul;
        dr.k_click *= 0.95f;
    }
    if (dr.s_namp > 0.0001f) {
        dr.s_ph = wrap1(dr.s_ph + dr.s_tinc);
        s = sin_t(dr.s_ph) * dr.s_tamp + svf_bp(&dr.s_bp, n) * dr.s_namp * 2.0f;
        dr.s_tamp *= dr.s_tmul;
        dr.s_namp *= dr.s_nmul;
    }
    if (dr.h_amp > 0.0001f) {
        dr.h_lp += 0.55f * (n - dr.h_lp);
        x = n - dr.h_lp;
        dr.h_hp += 0.5f * (x - dr.h_hp);
        h = (x - dr.h_hp) * dr.h_amp * 3.0f;
        dr.h_amp *= dr.h_mul;
    }
    if (dr.p_amp > 0.0001f) {
        dr.p_pc = wrap1(dr.p_pc + dr.p_ic);
        dr.p_pm = wrap1(dr.p_pm + dr.p_im);
        h += sin_t(dr.p_pc + dr.p_idx * sin_t(dr.p_pm)) * dr.p_amp;
        dr.p_amp *= dr.p_mul;
    }
    x = soft_tanh(1.4f * k) * 0.75f;
    dr.bus_lp += kit()->bus * (x + s + h - dr.bus_lp);   /* dull the kit, as if sampled */
    *l = dr.bus_lp - 0.1f * s - 0.25f * h;
    *r = dr.bus_lp + 0.1f * s + 0.25f * h;
}

/* short decaying sines with a pitch sweep: rain drops, creek bubbles */
#define NBLIP 20
typedef struct {
    float ph, inc, inc_mul, amp, dec, pl, pr;
} blip_t;
static blip_t bl[NBLIP];

static void blip(float hz, float sweep_per_sec, float amp, float secs, float pan)
{
    int i, q = 0;
    for (i = 0; i < NBLIP; i++) {
        if (bl[i].amp < 0.0002f) {
            q = i;
            break;
        }
        if (bl[i].amp < bl[q].amp)
            q = i;
    }
    bl[q].ph = 0.0f;
    bl[q].inc = hz / MS_SR;
    bl[q].inc_mul = fm_exp2f(sweep_per_sec / MS_SR);
    bl[q].amp = amp;
    bl[q].dec = decay_mul(secs);
    pan_gains(pan, &bl[q].pl, &bl[q].pr);
}

static struct {
    float gust, gust_target;
    int32_t gust_t;
    svf_t wl, wr;
    float rain_hp_l, rain_hp_r, rain_lp_l, rain_lp_r;
    float surf_ph, surf_period, surf_env, surf_bright, surf_wash;
    svf_t sl, sr;
    float wash_lp_l, wash_lp_r;
    svf_t creek_l, creek_r;
    float fire_rum_l, fire_rum_r, fire_rum2_l, fire_rum2_r, fire_flick, fire_flick_t, crackle, crackle_dec, crackle_pan;
    float fire_hp;
    int32_t burst;
} na;

static struct {
    int active;
    int32_t delay;
    float env, target, att, dec, crack, near, cl, cr;
    int swells;
    svf_t lpl, lpr;
} th;

static void thunder_strike(void)
{
    float dist = frand();               /* 0 near .. 1 far */
    th.dec = fm_expf(-6.9f * (float)CTL / ((4.0f + 5.0f * dist) * MS_SR));
    th.active = 1;
    th.near = 1.0f - dist;
    th.delay = (int32_t)((0.3f + 3.5f * dist) * MS_SR);
    th.env = 0.0f;
    th.target = 0.0f;
    th.att = onepole_a(1.0f / (FM_TWO_PI * (0.05f + 0.4f * dist)));
    th.swells = 2 + irand(4);
    svf_set(&th.lpl, 90.0f + 260.0f * (1.0f - dist), 0.7f);
    svf_set(&th.lpr, 80.0f + 240.0f * (1.0f - dist), 0.7f);
    ms_vis.flashes++;
}

#define NBIRD 2
typedef struct {
    uint8_t species, syl_left, motif;
    int32_t wait, syl_t, syl_n, gap;
    float ph, ph2, inc, dinc, trill_ph, trill_inc, trill_depth, peak, pl, pr, lp, lp_a, f0;
} bird_t;
static bird_t bd[NBIRD];

HOT void bird_syllable(bird_t *b)
{
    float f, f_end, dur;
    int type;
    switch (b->species) {
    case BIRD_GULL:
        f = b->f0 * (1.0f + 0.08f * frand2());
        f_end = f * (0.62f + 0.1f * frand());
        dur = 0.14f + 0.16f * frand();
        b->trill_inc = (11.0f + 6.0f * frand()) / MS_SR;
        b->trill_depth = 0.035f;
        b->gap = (int32_t)((0.05f + 0.1f * frand()) * MS_SR);
        break;
    case BIRD_OWL:
        f = b->f0 * (1.0f + 0.03f * frand2());
        f_end = f * 0.9f;
        dur = 0.3f + 0.25f * frand();
        b->trill_inc = 5.0f / MS_SR;
        b->trill_depth = 0.01f;
        b->gap = (int32_t)((0.18f + 0.35f * frand()) * MS_SR);
        break;
    default:
        /* songbirds mostly repeat a motif seeded per song */
        type = (int)((b->motif + b->syl_left * 3u) % 4u);
        if (frand() < 0.25f)
            type = irand(4);
        f = b->f0 * (0.85f + 0.4f * frand());
        dur = 0.03f + 0.09f * frand();
        b->trill_depth = 0.0f;
        b->trill_inc = 0.0f;
        if (type == 0)
            f_end = f * 0.55f;          /* falling whistle */
        else if (type == 1)
            f_end = f * 1.45f;          /* rising whistle */
        else if (type == 2) {
            f_end = f * 0.9f;           /* trill */
            b->trill_inc = (28.0f + 30.0f * frand()) / MS_SR;
            b->trill_depth = 0.12f;
            dur *= 1.6f;
        } else
            f_end = f * 1.05f;          /* chip */
        b->gap = (int32_t)((0.02f + 0.08f * frand()) * MS_SR);
        break;
    }
    b->syl_n = (int32_t)(dur * MS_SR);
    b->syl_t = 0;
    b->inc = f / MS_SR;
    b->dinc = (f_end - f) / MS_SR / (float)b->syl_n;
}

static void bird_song(bird_t *b)
{
    float pan = frand2() * 0.85f, dist = frand();
    switch (b->species) {
    case BIRD_GULL:
        b->f0 = 1150.0f + 500.0f * frand();
        b->syl_left = (uint8_t)(3 + irand(4));
        b->peak = 0.05f * (1.0f - 0.6f * dist);
        b->lp_a = onepole_a(2500.0f + 2000.0f * (1.0f - dist));
        break;
    case BIRD_OWL:
        b->f0 = 360.0f + 60.0f * frand();
        b->syl_left = (uint8_t)(2 + irand(3));
        b->peak = 0.09f * (1.0f - 0.5f * dist);
        b->lp_a = onepole_a(900.0f);
        break;
    default:
        b->f0 = 2300.0f + 2000.0f * frand();
        b->syl_left = (uint8_t)(3 + irand(7));
        b->motif = (uint8_t)irand(4);
        b->peak = 0.045f * (1.0f - 0.6f * dist);
        b->lp_a = onepole_a(4000.0f + 5000.0f * (1.0f - dist));
        break;
    }
    pan_gains(pan, &b->pl, &b->pr);
    ms_vis.calls++;
    ms_vis.call_pan = pan;
    bird_syllable(b);
}

HOT int32_t bird_wait(void)
{
    float per_min = k.bird_rate * (1.0f + 2.0f * kb.energy), secs;
    if (per_min <= 0.01f)
        return (int32_t)(5.0f * MS_SR);
    secs = 60.0f / per_min * (0.4f + 1.2f * frand());
    return (int32_t)(secs * MS_SR);
}

HOT void bird_tick(bird_t *b, float *l, float *r)
{
    float env, s;
    if (!b->syl_left)
        return;
    if (b->gap > 0 && b->syl_t >= b->syl_n) {
        if (--b->gap == 0) {
            if (--b->syl_left == 0) {
                b->wait = bird_wait();
                return;
            }
            bird_syllable(b);
        }
        return;
    }
    if (b->syl_t >= b->syl_n)
        return;
    env = sin_t(0.5f * (float)b->syl_t / (float)b->syl_n);
    env *= env;
    b->inc += b->dinc;
    b->trill_ph = wrap1(b->trill_ph + b->trill_inc);
    b->ph = wrap1(b->ph + b->inc * (1.0f + b->trill_depth * sin_t(b->trill_ph)));
    if (b->species == BIRD_GULL) {
        b->ph2 = wrap1(b->ph2 + b->inc * 0.5f);
        s = sin_t(b->ph + 0.18f * sin_t(b->ph2) + 0.1f * sin_t(2.0f * b->ph));
    } else {
        s = sin_t(b->ph);
    }
    b->lp += b->lp_a * (s * env * b->peak - b->lp);
    *l += b->lp * b->pl;
    *r += b->lp * b->pr;
    b->syl_t++;
}

#define NCRICKET 3
typedef struct {
    float ph, inc, pp, pinc, amp, pl, pr;
    int32_t pulses, wait, period;
} cricket_t;
static cricket_t cr[NCRICKET];
static int n_crickets;

static void crickets_init(int n)
{
    int i;
    n_crickets = n;
    for (i = 0; i < n; i++) {
        cr[i].inc = (4100.0f + 900.0f * frand()) / MS_SR;
        cr[i].pinc = (26.0f + 10.0f * frand()) / MS_SR;
        cr[i].amp = 0.02f + 0.02f * frand();
        cr[i].period = (int32_t)((0.55f + 0.7f * frand()) * MS_SR);
        cr[i].wait = irand(cr[i].period);
        cr[i].pulses = 0;
        pan_gains(frand2() * 0.9f, &cr[i].pl, &cr[i].pr);
    }
}

HOT void cricket_tick(cricket_t *c, float life, float *l, float *r)
{
    float env, s;
    if (c->pulses <= 0) {
        if (--c->wait <= 0) {
            c->wait = c->period + irand(c->period / 4 + 1);
            if (frand() < 0.25f + 0.75f * life) {
                c->pulses = 3 + irand(3);
                c->pp = 0.0f;
                ms_vis.chirps++;
            }
        }
        return;
    }
    c->pp += c->pinc;
    if (c->pp >= 1.0f) {
        c->pp -= 1.0f;
        c->pulses--;
    }
    env = c->pp < 0.5f ? sin_t(c->pp) : 0.0f;
    c->ph = wrap1(c->ph + c->inc);
    s = sin_t(c->ph) * env * env * c->amp;
    *l += s * c->pl;
    *r += s * c->pr;
}

#define ECHO_N 28672                   /* stored at half rate (darker, like tape, and half the RAM): 1.3 s */
static int16_t echo_l[ECHO_N] PLAT_POOL, echo_r[ECHO_N] PLAT_POOL;
static uint32_t echo_w;
static float ms_echo_len_s, ms_wob_wf_s, ms_rev_m_s;   /* the read heads' wobble, eased per sample */
static float echo_len, echo_len_target, echo_lp_l, echo_lp_r, echo_hp_l, echo_hp_r, echo_hold_l, echo_hold_r;
/* MOTION: MOD wobbles the repeats, RANDOM throws in events, TONE darkens or brightens the repeats, SPREAD widens them */
static struct {
    float mod, random, tone_a, hp_a, spread;
    float mod_ph, mod_m;
    float ev_t;                        /* seconds until chance may throw an event */
    float time_base, time_hold;        /* TIME's own delay length, and how long a jump stays away from it */
    float fb_boost, rev_boost;         /* an event's lift of the feedback and the reverb, easing back to 0 */
} mo = {0.0f, 0.0f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f};
static int echo_half;

#define NLINE 4
static const uint16_t LINE_LEN[NLINE] = {2333, 2879, 3433, 4007};
#define LINE_PAD 40
#define REV_TOTAL (2333 + 2879 + 3433 + 4007 + 4 * LINE_PAD)
static float rev_buf[REV_TOTAL] PLAT_POOL;
static const uint16_t AP_LEN[4] = {142, 107, 379, 277};
static float ap_buf[142 + 107 + 379 + 277] PLAT_POOL;
static struct {
    float *line[NLINE];
    uint32_t w[NLINE], len[NLINE];
    float lp[NLINE], g[NLINE], damp;
    float lenf[NLINE];                 /* the length read now, gliding to len */
    float *ap[4];
    uint32_t apw[4];
    float mod_ph, mod_m;               /* its slow modulation, moved once a control tick */
} rv;

#define WOB_N 1024
static float wob_l[WOB_N] PLAT_POOL, wob_r[WOB_N] PLAT_POOL;
static uint32_t wob_w;

static struct {
    svf_t tone_l, tone_r;
    float crush_hold_l, crush_hold_r;
    int crush_n, crush_i;
    float crackle, crackle_dec, crackle_pan, hiss_lp, hum_ph;
    float peak;
    float sleep;
    float dc_l, dc_r, mode_gain;
    float rev_last, drop_t, drop_g, wob_s, film_s, wob_wf;
    uint32_t sm_quiet;                 /* samples since anything went into the smear */
    float drum_l, drum_r;               /* the kit's last sample, for the declick when KIT goes OFF */
    float hp_l, radio_s, ch_ph, tr_ph, ch_s, tr_s;   /* the spaces' chorus and pulse: phases, and depths eased in */
} ms;

static void fx_init(void)
{
    int i;
    float *p = rev_buf, *q = ap_buf;
    for (i = 0; i < NLINE; i++) {
        rv.line[i] = p;
        p += LINE_LEN[i] + LINE_PAD;
        rv.len[i] = LINE_LEN[i];
        rv.lenf[i] = (float)LINE_LEN[i];
        rv.w[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        rv.ap[i] = q;
        q += AP_LEN[i];
        rv.apw[i] = 0;
    }
    echo_len = echo_len_target = ms_echo_len_s = 0.2f * MS_SR;
}

static void fx_clear(void)
{
    uint32_t i;
    for (i = 0; i < REV_TOTAL; i++)
        rev_buf[i] = 0.0f;
    for (i = 0; i < sizeof ap_buf / sizeof ap_buf[0]; i++)
        ap_buf[i] = 0.0f;
    for (i = 0; i < ECHO_N; i++)
        echo_l[i] = echo_r[i] = 0;
    for (i = 0; i < WOB_N; i++)
        wob_l[i] = wob_r[i] = 0.0f;
}

HOT int16_t to16(float x)
{
    x *= 32767.0f;
    return (int16_t)(x > 32767.0f ? 32767 : x < -32767.0f ? -32767 : (int32_t)x);
}

static float wow_ph, flutter_ph, wob_drift, wob_drift_to, wob_drift_t;

HOT void echo_tick(float in, float *l, float *r)
{
    float len, pos, fr, a, b, dl, dr2, wl, wr;
    int32_t i0;
    echo_len += 0.00005f * (echo_len_target - echo_len);   /* time changes glide, like tape */
    len = h_minf(echo_len * (1.0f + k.echo_wow * sin_t(wow_ph) + mo.mod * mo.mod * 0.025f * mo.mod_m), (float)(ECHO_N - 4));
    /* the wobble's phases move once a control block: eased here, a stair in the read head would be a corner each step */
    ms_echo_len_s += 0.02f * (len - ms_echo_len_s);
    len = ms_echo_len_s;
    pos = (float)echo_w + (echo_half ? 0.5f : 0.0f) - len;
    if (pos < 0.0f)
        pos += (float)ECHO_N;
    i0 = (int32_t)pos;
    fr = pos - (float)i0;                /* the blend first, from the position as it is */
    if (i0 >= ECHO_N || i0 < 0)          /* pos a hair under 0 rounds up to exactly N: that is sample 0 */
        i0 = 0;
    a = (float)echo_l[i0] * (1.0f / 32767.0f);
    b = (float)echo_l[(i0 + 1) % ECHO_N] * (1.0f / 32767.0f);
    dl = a + fr * (b - a);
    a = (float)echo_r[i0] * (1.0f / 32767.0f);
    b = (float)echo_r[(i0 + 1) % ECHO_N] * (1.0f / 32767.0f);
    dr2 = a + fr * (b - a);
    a = h_minf(mo.tone_a, k.echo_lp_max);
    b = h_maxf(h_maxf(mo.hp_a, k.echo_hp_min), 0.0007f);   /* never stops tracking, or a held value stays as DC */
    echo_lp_l += a * (dl - echo_lp_l);
    echo_lp_r += a * (dr2 - echo_lp_r);
    echo_hp_l += b * (echo_lp_l - echo_hp_l);
    echo_hp_r += b * (echo_lp_r - echo_hp_r);
    dl = echo_lp_l - echo_hp_l;
    dr2 = echo_lp_r - echo_hp_r;
    /* ping-pong: input enters left, repeats cross over; pairs of samples are averaged into one stored */
    wl = soft_tanh(in + (k.echo_fb + mo.fb_boost) * dr2);
    wr = (k.echo_fb + mo.fb_boost) * dl;
    if (echo_half) {
        echo_l[echo_w] = to16(0.5f * (echo_hold_l + wl));
        echo_r[echo_w] = to16(0.5f * (echo_hold_r + wr));
        echo_w = (echo_w + 1) % ECHO_N;
    } else {
        echo_hold_l = wl;
        echo_hold_r = wr;
    }
    echo_half ^= 1;
    *l = dl * (0.5f + 0.5f * mo.spread) + dr2 * (0.5f - 0.5f * mo.spread);
    *r = dr2 * (0.5f + 0.5f * mo.spread) + dl * (0.5f - 0.5f * mo.spread);
}

HOT float ap_run(int i, float x, float g)
{
    float *buf = rv.ap[i], d = buf[rv.apw[i]], y = d - g * x;
    buf[rv.apw[i]] = x + g * y;
    if (++rv.apw[i] >= AP_LEN[i])
        rv.apw[i] = 0;
    return y;
}

HOT float line_read(int i, float extra)
{
    float pos, fr, a, b;
    rv.lenf[i] += 0.0002f * ((float)rv.len[i] - rv.lenf[i]);   /* ~0.1 s: a size change bends, never jumps */
    pos = (float)rv.w[i] - rv.lenf[i] - extra;
    int32_t i0, n = LINE_LEN[i] + LINE_PAD;
    if (pos < 0.0f)
        pos += (float)n;
    i0 = (int32_t)pos;
    fr = pos - (float)i0;
    if (i0 >= n || i0 < 0)
        i0 = 0;
    a = rv.line[i][i0];
    b = rv.line[i][(i0 + 1) % n];
    return a + fr * (b - a);
}

HOT void reverb_tick(float inl, float inr, int frozen, float *l, float *r)
{
    float d[NLINE], x, m, h0, h1, h2, h3, in;
    int i;
    ms_rev_m_s += 0.02f * (rv.mod_m - ms_rev_m_s);   /* eased per sample, as the echo's */
    m = ms_rev_m_s;
    d[0] = line_read(0, 8.0f + 7.0f * k.rev_mod * m);
    d[1] = line_read(1, 0.0f);
    d[2] = line_read(2, 8.0f - 7.0f * k.rev_mod * m);
    d[3] = line_read(3, 0.0f);
    in = frozen ? 0.0f : 0.5f * (inl + inr);
    x = ap_run(0, in, 0.7f);
    x = ap_run(1, x, 0.7f);
    x = ap_run(2, x, 0.6f);
    x = ap_run(3, x, 0.6f);
    for (i = 0; i < NLINE; i++)
        rv.lp[i] += rv.damp * (d[i] - rv.lp[i]);
    /* Hadamard mix: energy preserving, every line feeds every other */
    h0 = 0.5f * (rv.lp[0] + rv.lp[1] + rv.lp[2] + rv.lp[3]);
    h1 = 0.5f * (rv.lp[0] - rv.lp[1] + rv.lp[2] - rv.lp[3]);
    h2 = 0.5f * (rv.lp[0] + rv.lp[1] - rv.lp[2] - rv.lp[3]);
    h3 = 0.5f * (rv.lp[0] - rv.lp[1] - rv.lp[2] + rv.lp[3]);
    rv.line[0][rv.w[0]] = h_flush(h0 * rv.g[0] + x + (frozen ? 0.0f : 0.15f * inl));
    rv.line[1][rv.w[1]] = h_flush(h1 * rv.g[1] + x);
    rv.line[2][rv.w[2]] = h_flush(h2 * rv.g[2] + x + (frozen ? 0.0f : 0.15f * inr));
    rv.line[3][rv.w[3]] = h_flush(h3 * rv.g[3] + x);
    for (i = 0; i < NLINE; i++)
        if (++rv.w[i] >= (uint32_t)(LINE_LEN[i] + LINE_PAD))
            rv.w[i] = 0;
    *l = d[0] + 0.5f * d[2];             /* separate lines, no polarity flips, so it sums to mono cleanly */
    *r = d[1] + 0.5f * d[3];
}

static void reverb_params(int frozen)
{
    int i;
    for (i = 0; i < NLINE; i++) {
        rv.len[i] = (uint32_t)((float)LINE_LEN[i] * k.rev_size);
        rv.g[i] = frozen ? 1.0f : fm_exp2f(-9.9658f * (float)rv.len[i] / (k.rev_rt * MS_SR));
    }
    rv.damp = frozen ? 1.0f
                     : onepole_a(k.damp_hz > 0.0f ? k.damp_hz : fm_clampf(2500.0f + 6000.0f * (1.0f - k.rev_rt / 14.0f), 1500.0f, 9000.0f));
}

static struct {
    float acc;                         /* samples until the next step */
    uint32_t step;
    int mel_last;
    float note_acc;
    float chord_t;                     /* SCAPES, DRIFT: seconds until the next chord */
    int8_t mel_pool[4];                /* LOFI: the player's last notes, reused by the melody */
    int mel_n;
} seq;

static float beat_swing(void)
{
    if (bt.swing >= 0.0f)
        return bt.swing;
    if (bt.kit != KIT_OFF && bt.pattern == PAT_MINE)
        return 0.15f;
    if (bt.kit != KIT_OFF && bt.pattern != 0)
        return PATTERNS[bt.pattern].swing;
    return st.mode == MODE_LOFI ? LOFI_STYLE[st.scene].swing : st.mode == MODE_SCAPES ? 0.08f : 0.0f;
}

/* AUTO is the scene's own groove: LOFI's style, or a sparse one elsewhere */
static pattern_t beat_pattern(void)
{
    pattern_t p;
    if (bt.pattern == PAT_MINE) {
        p.kick = ub.row[MS_DRUM_KICK];
        p.snare = ub.row[MS_DRUM_SNARE];
        p.ghost = ub.row[MS_DRUM_GHOST];
        p.hat = ub.row[MS_DRUM_HAT];
        p.ohat = ub.row[MS_DRUM_OPEN];
        p.perc = ub.row[MS_DRUM_PERC];
        p.len = ub.len;
        p.swing = 0.15f;
        p.hat16 = 0.0f;
        return p;
    }
    if (bt.pattern != 0)
        return PATTERNS[bt.pattern];
    if (st.mode != MODE_LOFI)
        return PATTERNS[9];
    p.kick = LOFI_STYLE[st.scene].kick;
    p.snare = LOFI_STYLE[st.scene].snare;
    p.ghost = LOFI_STYLE[st.scene].ghost;
    p.hat = LOFI_STYLE[st.scene].hat;
    p.ohat = LOFI_STYLE[st.scene].ohat;
    p.perc = 0;
    p.len = 16;
    p.swing = LOFI_STYLE[st.scene].swing;
    p.hat16 = LOFI_STYLE[st.scene].hat16;
    return p;
}

static float step_len(void)   /* a sixteenth; swing lengthens even steps and shortens odd ones */
{
    float len = 60.0f / (float)st.tempo / 4.0f * MS_SR, sw = beat_swing();
    return (seq.step & 1u) ? len * (1.0f - sw) : len * (1.0f + sw);
}

/* the pad under LOFI's piano: the root and fifth low, the ninth high, swelling in and fading out under the next */
static void lofi_pad(float level)
{
    static const int8_t TONE[3] = {0, 2, 4};
    static const int8_t CENTRE[3] = {45, 52, 74};
    int i;
    for (i = 0; i < NV; i++)
        if (vo[i].kind == V_PAD && vo[i].layer == 0 && (vo[i].stage == ST_ATK || vo[i].stage == ST_DEC))
            vo[i].stage = ST_REL;
    for (i = 0; i < 3; i++)
        voice_start(V_PAD, chord_note_near(TONE[i], CENTRE[i]), level * (i == 2 ? 0.5f : 0.8f), 0, -1, 1.5f, 3.5f,
                    (float)(i - 1) * 0.5f);
}

static void lofi_chord_hit(float vel, int ntones, int32_t gate)
{
    int i, centre = 62, n;
    float strum = 0.0f;
    for (i = 1; i <= ntones && i < 5; i++) {
        n = chord_note_near(i, centre);
        ev_add((int32_t)(strum * MS_SR), EV_NOTE, V_EP, n, vel * (0.85f + 0.15f * frand()), 0, gate,
               (float)i * 0.3f - 0.75f);
        strum += 0.012f + 0.018f * frand();
    }
}

/* MINE plays exactly what was set: each drum at its level, each hit by its chance, with no fills of its own */
static void mine_step(uint32_t s, int32_t hum)
{
    static const uint8_t EV[MS_NDRUMS] = {EV_KICK, EV_SNARE, EV_GHOST, EV_HAT, EV_OHAT, EV_PERC};
    int d;
    for (d = 0; d < MS_NDRUMS; d++) {
        float v = ub.level[d];
        if (!(ub.row[d] >> s & 1u) || frand() >= ub.chance[d] || v <= 0.0f)
            continue;
        if (d == MS_DRUM_HAT)
            v *= (s & 3u) == 0 ? 1.0f : (s & 1u) ? 0.55f : 0.75f;
        ev_add(hum + (d == MS_DRUM_SNARE ? (int32_t)(0.008f * MS_SR) : 0), EV[d], 0, d == MS_DRUM_PERC && (s & 4u),
               v * (0.9f + 0.1f * frand()), 1, 0, 0.0f);
    }
}

int ms_pattern_rows(int mode, int scene, int pattern, uint16_t rows[MS_NDRUMS], float *swing)
{
    pattern_t p;
    pattern = clamp(pattern, 0, PAT_N - 1);
    if (pattern == 0 && mode == MODE_LOFI) {
        const lofi_style_t *ls = &LOFI_STYLE[clamp(scene, 0, MS_NSCENES[MODE_LOFI] - 1)];
        p.kick = ls->kick, p.snare = ls->snare, p.ghost = ls->ghost, p.hat = ls->hat, p.ohat = ls->ohat, p.perc = 0;
        p.len = 16, p.swing = ls->swing;
    } else {
        p = PATTERNS[pattern == 0 || pattern == PAT_MINE ? 9 : pattern];
    }
    if (swing)
        *swing = p.swing;
    rows[MS_DRUM_KICK] = p.kick, rows[MS_DRUM_SNARE] = p.snare, rows[MS_DRUM_GHOST] = p.ghost;
    rows[MS_DRUM_HAT] = p.hat, rows[MS_DRUM_OPEN] = p.ohat, rows[MS_DRUM_PERC] = p.perc;
    return p.len;
}

static void beat_step(void)
{
    pattern_t p = beat_pattern();
    uint32_t s = seq.step % p.len, bit = 1u << s, bar = seq.step / p.len;
    int32_t hum = (int32_t)(frand() * 0.006f * MS_SR);
    float busy = fm_minf(kb.energy + evo.busy, 1.0f), v;
    ms_vis.drums[0] = p.kick;
    ms_vis.drums[1] = p.snare;
    ms_vis.drums[2] = p.hat;
    ms_vis.beat_len = p.len;
    ms_vis.beat_step = (uint8_t)s;
    if (bt.pattern == PAT_MINE) {
        mine_step(s, hum);
        return;
    }
    if (p.kick & bit)
        ev_add(hum, EV_KICK, 0, 0, s == 0 ? 1.0f : 0.8f, 1, 0, 0.0f);
    else if (s == p.len - 2u && frand() < 0.15f + 0.3f * busy)
        ev_add(hum, EV_KICK, 0, 0, 0.6f, 1, 0, 0.0f);
    if (p.snare & bit)
        ev_add(hum + (int32_t)(0.008f * MS_SR), EV_SNARE, 0, 0, 0.9f, 1, 0, 0.0f);
    else if ((p.ghost & bit) && frand() < 0.5f + 0.4f * busy)
        ev_add(hum, EV_GHOST, 0, 0, 0.25f + 0.15f * frand(), 1, 0, 0.0f);
    else if ((bar & 3u) == 3u && s >= p.len - 4u && frand() < 0.6f * busy)   /* a fill into the next phrase */
        ev_add(hum, EV_GHOST, 0, 0, 0.3f + 0.3f * frand(), 1, 0, 0.0f);
    if (p.ohat & bit)
        ev_add(hum, EV_OHAT, 0, 0, 0.6f, 1, 0, 0.0f);
    else if ((p.hat & bit) || frand() < p.hat16 * (1.0f + 2.0f * busy)) {
        v = (s & 3u) == 0 ? 0.9f : (s & 1u) ? 0.35f : 0.6f;
        ev_add(hum, EV_HAT, 0, 0, v * (0.8f + 0.2f * frand()), 1, 0, 0.0f);
    }
    if (p.perc & bit)
        ev_add(hum, EV_PERC, 0, (s & 4u) ? 1 : 0, 0.7f + 0.3f * frand(), 1, 0, 0.0f);
}

static void backing_bass_step(void)
{
    pattern_t p = beat_pattern();
    uint32_t s = seq.step % p.len;
    float sixteenth = 60.0f / (float)st.tempo / 4.0f * MS_SR;
    int root = 36 + (st.root + hm.tones[0]) % 12, n;
    int32_t gate = (int32_t)(sixteenth * bt.bass_len);
    if (bt.bass == BASS_MINE) {         /* the line follows the chord: its root, 3rd, 5th, 7th */
        static const int8_t TONE[MS_NBN] = {0, 0, 1, 2, 3, 0, 0};
        int b = ub.bass[seq.step & 15u];
        if (!b)
            return;
        n = b == MS_BN_STEP ? root - 1 : 36 + (st.root + hm.tones[TONE[b]]) % 12 + (b == MS_BN_OCT ? 12 : 0);
        if (n < root && b != MS_BN_STEP)
            n += 12;
        bass_note(n, (seq.step & 15u) == 0 ? 0.9f : 0.72f, gate, bt.bass_glide, 0.004f);
        return;
    }
    if (bt.bass == BASS_ROOT) {
        if ((p.kick >> s & 1u) || s == 0)
            bass_note(root, s == 0 ? 0.9f : 0.75f, gate, bt.bass_glide, 0.004f);
        return;
    }
    if (s % 4u)
        return;
    switch (s / 4u) {
    case 0:
        n = root;
        break;
    case 1:
        n = 36 + (st.root + hm.tones[frand() < 0.5f ? 1 : 2]) % 12;
        break;
    case 2:
        n = 36 + (st.root + hm.tones[2]) % 12;
        break;
    default:
        n = root + (frand() < 0.5f ? -1 : 2);   /* step toward the root of the next bar */
        break;
    }
    bass_note(n, s == 0 ? 0.85f : 0.65f, gate, bt.bass_glide, 0.004f);
}

static void lofi_step(void)
{
    const lofi_style_t *ls = &LOFI_STYLE[st.scene];
    uint32_t s = seq.step & 15u;
    float sixteenth = 60.0f / (float)st.tempo / 4.0f * MS_SR;
    int32_t hum = (int32_t)(frand() * 0.006f * MS_SR);
    int next_root, n;
    if (s == 0) {
        int bars = (int)((float)ls->bars_per_chord * RATE_MUL[hc.rate ? hc.rate : 2] * chord_len() + 0.5f);
        bars = bars < 1 ? 1 : bars;
        ++hm.bar_in_chord;
        if (hm.bar_in_chord >= bars || (hc.lock >= 0 && hc.lock != chord_id()) || hm.pending >= 0) {
            hm.bar_in_chord = 0;
            chord_set(chord_next(ls->prog, 0));
            hm.pending = -1;
        }
        if (hm.bar_in_chord == 0) {
            lofi_chord_hit(ls->pad > 0.0f ? 0.6f : 0.75f, 4, (int32_t)(sixteenth * (float)(10 * ls->bars_per_chord)));
            if (ls->pad > 0.0f)
                lofi_pad(ls->pad);
        }
        else
            lofi_chord_hit(0.55f, 3, (int32_t)(sixteenth * 8.0f));
        if (bt.bass == BASS_AUTO)
            bass_note(36 + ((st.root + hm.tones[0]) % 12), 0.9f, (int32_t)(sixteenth * 6.0f), 0.0f, 0.004f);
    }
    if ((s == 6 || s == 10) && frand() < 0.3f)
        lofi_chord_hit(0.45f, 2 + irand(2), (int32_t)(sixteenth * 3.0f));
    if (s == 10 && bt.bass == BASS_AUTO && frand() < 0.6f)
        bass_note(36 + ((st.root + (frand() < 0.5f ? hm.tones[2] : hm.tones[0])) % 12), 0.7f,
                  (int32_t)(sixteenth * 3.0f), 0.0f, 0.004f);
    if (s == 14 && bt.bass == BASS_AUTO && frand() < 0.3f) {   /* approach the next root from a semitone below */
        int nd = chord_next(ls->prog, 1);
        next_root = (st.root + chord_root_semis(nd)) % 12;
        bass_note(36 + ((next_root + 11) % 12), 0.6f, (int32_t)(sixteenth * 1.6f), 0.0f, 0.004f);
    }
    if ((s & 1u) == 0 && s != 0 && frand() < ls->mel * (1.0f + 3.0f * kb.energy)) {
        int deg = seq.mel_last + irand(5) - 2;
        if (deg < 7)
            deg += 2;
        if (deg > 13)
            deg -= 3;
        seq.mel_last = deg;
        n = scale_note_near(deg, 76);
        if (seq.mel_n && frand() < 0.6f) {   /* answer with the player's own notes */
            n = seq.mel_pool[irand(seq.mel_n < 4 ? seq.mel_n : 4)];
            while (n < 70)
                n += 12;
            while (n > 84)
                n -= 12;
        }
        ev_add(hum, EV_NOTE, ls->mel_kind, n, 0.4f + 0.25f * frand(), 0, (int32_t)(sixteenth * 3.0f), 0.4f * frand2());
        if (frand() < 0.35f)
            ev_add(hum + (int32_t)(sixteenth * 2.0f * (1.0f - LOFI_STYLE[st.scene].swing)), EV_NOTE, ls->mel_kind,
                   scale_note_near(deg - 1, 76), 0.35f, 0, (int32_t)(sixteenth * 2.0f), 0.4f * frand2());
    }
}

static void pad_chord(int ntones, float atk, float rel)
{
    int i;
    voices_release(1);
    for (i = 0; i < ntones; i++) {
        int n = chord_note_near(i == 0 ? 0 : i, i == 0 ? 48 : 60);
        voice_start(V_PAD, n, 0.7f, 2, -1, atk, rel, (float)i / (float)ntones * 1.2f - 0.6f);
    }
}

static void scapes_next_chord(void)
{
    const scape_style_t *ss = &SCAPE_STYLE[st.scene];
    chord_set(chord_next(ss->prog, 0));
    hm.pending = -1;
    pad_chord(4, 4.0f, 7.0f);
    seq.chord_t = chord_wait(ss->chord_secs * (0.8f + 0.4f * frand()));
}

static void scapes_note(void)
{
    const scape_style_t *ss = &SCAPE_STYLE[st.scene];
    /* pentatonic-leaning: chord tones plus the 2nd and 6th */
    static const int8_t PICK[] = {0, 2, 4, 1, 5, 7, 9, 11};
    int n = scale_note_near(PICK[irand(8)] + hm.degree, 72 + irand(10)), i, phrase = frand() < 0.3f ? 2 + irand(2) : 1;
    for (i = 0; i < phrase; i++)
        ev_add((int32_t)((float)i * (0.25f + 0.2f * frand()) * MS_SR), EV_NOTE, ss->note_kind,
               i ? scale_note_near(PICK[irand(8)] + hm.degree, n) : n, 0.35f + 0.35f * frand(), 3,
               (int32_t)(1.2f * MS_SR), 0.7f * frand2());
}

#define NLOOP 4
static struct {
    int8_t deg[NLOOP][32];             /* scale degree above the root, -1 rest */
    uint8_t len[NLOOP];
    uint32_t pos[NLOOP];
    float pan[NLOOP];
    float cyc[4], cyc_t[4];            /* per pad voice: cycle length and time left, seconds */
    voice_t *pad[4];
    uint32_t pad_age[4];
} dg;

static void drift_fill_loop(int i)
{
    int j, nnotes = 1 + (int)(k.density * 4.0f + frand() * 1.5f);
    for (j = 0; j < 32; j++)
        dg.deg[i][j] = -1;
    for (j = 0; j < nnotes; j++)
        dg.deg[i][irand(dg.len[i])] = (int8_t)(7 + i * 2 + irand(7));
}

static uint32_t loop_mask(int i)
{
    uint32_t m = 0;
    int j;
    for (j = 0; j < dg.len[i]; j++)
        if (dg.deg[i][j] >= 0)
            m |= 1u << j;
    return m;
}

static void drift_regrow(void)
{
    const drift_style_t *ds = &DRIFT_STYLE[st.scene];
    static const float CYC[4] = {17.0f, 21.5f, 26.0f, 31.5f};
    int i;
    for (i = 0; i < NLOOP; i++) {
        dg.len[i] = ds->loop_len[i];
        dg.pos[i] = (uint32_t)irand(dg.len[i]);
        dg.pan[i] = (float)i * 0.5f - 0.75f;
        drift_fill_loop(i);
        ms_vis.loop_len[i] = dg.len[i];
        ms_vis.loop_notes[i] = loop_mask(i);
    }
    for (i = 0; i < 4; i++) {
        dg.cyc[i] = CYC[i];
        dg.cyc_t[i] = CYC[i] * (0.15f + 0.3f * (float)i);   /* stagger the entries */
        dg.pad[i] = 0;
    }
}

/* a lush chord: root low, then fifth, seventh, third and ninth spread wide; the last one fades out under it */
static void lush_chord(void)
{
    static const int8_t TONE[5] = {0, 2, 3, 1, 4};
    static const int8_t CENTRE[5] = {45, 52, 58, 64, 69};
    int i;
    for (i = 0; i < NV; i++)
        if (vo[i].kind == V_PAD && vo[i].layer == 0 && (vo[i].stage == ST_ATK || vo[i].stage == ST_DEC))
            vo[i].stage = ST_REL;
    for (i = 0; i < 5; i++)
        voice_start(V_PAD, chord_note_near(TONE[i], CENTRE[i]), i == 0 ? 0.6f : 0.45f, 0, -1, 4.0f, 7.0f,
                    (float)(i - 2) * 0.35f);
}

static void drift_next_chord(void)
{
    chord_set(chord_next((const int8_t *)0, 0));
    hm.pending = -1;
    if (bt.bass == BASS_AUTO)
        bass_note(36 + ((st.root + hm.tones[0]) % 12), 0.8f, -1, 1.6f, 2.5f);
    if (DRIFT_STYLE[st.scene].lush)
        lush_chord();
    seq.chord_t = chord_wait((150.0f - 135.0f * k.drift) * (0.7f + 0.6f * frand()) * DRIFT_STYLE[st.scene].chord_time);
}

static void drift_step(void)   /* every quarter note */
{
    const drift_style_t *ds = &DRIFT_STYLE[st.scene];
    int i, d;
    for (i = 0; i < NLOOP; i++) {
        dg.pos[i] = (dg.pos[i] + 1) % dg.len[i];
        if (dg.pos[i] == 0 && frand() < 0.15f + 0.6f * k.drift) {   /* mutate as the loop wraps */
            int j = irand(dg.len[i]);
            if (dg.deg[i][j] >= 0 && frand() < 0.5f)
                dg.deg[i][j] = -1;
            else
                dg.deg[i][irand(dg.len[i])] = (int8_t)(7 + i * 2 + irand(7));
            if (!loop_mask(i))
                drift_fill_loop(i);
            ms_vis.loop_notes[i] = loop_mask(i);
        }
        if (i == 0 && frand() < kb.energy * 0.35f + fm_maxf(k.density - 0.6f, 0.0f) * 0.5f)   /* extra bells: playing, high DENSITY */
            ev_add((int32_t)(frand() * 0.2f * MS_SR), EV_NOTE, ds->bell_kind, chord_note_near(irand(4), 74), 0.35f, 1,
                   (int32_t)(2.0f * MS_SR), frand2() * 0.6f);
        d = dg.deg[i][dg.pos[i]];
        if (d >= 0 && frand() < 0.3f + 0.7f * k.density / 0.6f) {   /* low DENSITY drops notes at once */
            int n = st.root + deg_semis(d) + 48;
            ev_add((int32_t)(frand() * 0.03f * MS_SR), EV_NOTE, ds->bell_kind, n, 0.35f + 0.4f * frand(), 1,
                   (int32_t)(2.0f * MS_SR), dg.pan[i]);
        }
    }
}

static void drift_pads(float dt)
{
    int i;
    if (DRIFT_STYLE[st.scene].lush) {    /* the chord is the pad: it breathes as a whole */
        for (i = 0; i < 4; i++)
            ms_vis.pad[i] = 0.5f;
        return;
    }
    for (i = 0; i < 4; i++) {
        float c;
        dg.cyc_t[i] -= dt;
        if (dg.cyc_t[i] > 0.0f)
            continue;
        c = dg.cyc[i] * (1.5f - 0.8f * k.drift);
        dg.cyc_t[i] = c;
        /* attack and release each take about half the cycle, so consecutive notes overlap */
        dg.pad[i] = voice_start(V_PAD, chord_note_near(i == 3 ? 4 : i, i == 0 ? 50 : 60 + i * 2), 0.75f, 0,
                    (int32_t)(c * 0.5f * MS_SR), c * 0.45f, c * 0.5f, (float)i * 0.6f - 0.9f);
        dg.pad_age[i] = dg.pad[i]->age;
    }
    for (i = 0; i < 4; i++)
        ms_vis.pad[i] = dg.pad[i] && dg.pad[i]->age == dg.pad_age[i] && dg.pad[i]->stage != ST_OFF ? dg.pad[i]->env : 0.0f;
}

static float lvl(int m) { return (float)m * (float)m * (1.0f / 4900.0f); }   /* 70 = unity, 100 = +6 dB */
static float u(int m) { return (float)m * 0.01f; }

/* a HOME knob as it sounds: where it is set, plus EVOLVE's drift */
static float mac(int i) { return u(clamp(st.macro[i] + (int)evo.off[i], 0, 100)); }

static void apply(void)
{
    int i, tone_i = st.mode == MODE_LOFI ? 0 : st.mode == MODE_SCAPES ? 2 : 1;
    float space = mac(3), base;
    k.tone_fc = 350.0f * fm_exp2f(mac(tone_i) * 5.5f);
    k.rev_mix = 0.12f + 0.75f * space;
    k.rev_rt = 1.2f + 11.0f * space * space;
    k.rev_size = 0.55f + 0.45f * space;
    k.film = u(st.fx[3]);
    k.tone_fc = fm_maxf(k.tone_fc * (1.0f - 0.55f * k.film), 300.0f);
    k.echo_mix = u(st.fx[0]) * (0.9f + 1.2f * u(st.fx[0]));   /* gentle low, and at the top the repeats take over */
    k.echo_fb = u(st.fx[2]) * 0.88f;
    base = fm_minf(DIV_BEATS[clamp(st.fx[1], 0, 5)] * 60.0f / (float)st.tempo * MS_SR * 0.5f, (float)(ECHO_N - 4));
    k.shimmer = k.reverse = k.damp_hz = 0.0f;
    k.chorus = k.trem = k.radio = k.wob_add = k.alg_rand = k.echo_hp_min = 0.0f;
    k.hp_a = onepole_a(500.0f);          /* RADIO's; kept so it fades out through the same filter */
    k.chorus_rate = 0.6f;
    k.echo_lp_max = 1.0f;
    k.sh_down = 0;
    k.echo_wow = 0.004f * k.film;
    k.echo_to_rev = 0.4f;
    k.rev_mod = 1.0f;
    switch (st.space) {
    case SP_PLATE:
        k.rev_size *= 0.6f;
        k.rev_rt *= 0.7f;
        k.damp_hz = 9000.0f;
        break;
    case SP_CLOUD:
        k.rev_size = 1.0f;
        k.rev_rt = fm_minf(k.rev_rt * 1.7f, 22.0f);
        k.rev_mod = 3.5f;
        k.damp_hz = 3500.0f;
        break;
    case SP_SHIMMER:                     /* a space is heard even with REVERB low */
        k.shimmer = 0.5f;
        k.rev_mix = fm_maxf(k.rev_mix, 0.4f);
        k.rev_rt = fm_maxf(k.rev_rt, 4.0f);
        k.rev_mod = 1.5f;
        break;
    case SP_TAPE:
        k.rev_rt *= 0.6f;
        k.echo_wow += 0.012f;
        k.echo_to_rev = 1.0f;
        k.wob_add = 0.35f;
        k.echo_lp_max = onepole_a(3500.0f);
        break;
    case SP_REVERSE:
        k.reverse = 0.9f;
        k.rev_mix = fm_maxf(k.rev_mix, 0.35f);
        break;
    case SP_DEEP:                        /* an octave under everything, in a dark cave */
        k.shimmer = 0.4f;
        k.sh_down = 1;
        k.rev_size = 1.0f;
        k.rev_rt = fm_minf(k.rev_rt * 1.4f + 2.0f, 18.0f);
        k.damp_hz = 2200.0f;
        k.tone_fc = fm_minf(k.tone_fc, 4500.0f);
        break;
    case SP_DUB:                         /* dotted echoes that keep going, dark and thin, over a small spring */
        k.echo_mix = fm_maxf(k.echo_mix, 0.9f);
        k.echo_fb = fm_maxf(k.echo_fb, 0.72f);
        base = fm_minf(0.75f * 60.0f / (float)st.tempo * MS_SR * 0.5f, (float)(ECHO_N - 4));
        k.echo_lp_max = onepole_a(1600.0f);
        k.echo_hp_min = onepole_a(280.0f);
        k.echo_to_rev = 0.2f;
        k.rev_size = 0.4f;
        k.rev_rt = 1.6f;
        k.rev_mix *= 0.7f;
        k.damp_hz = 5000.0f;
        break;
    case SP_DREAM:                       /* a lush, wide chorus in a soft shimmering room */
        k.chorus = 0.7f;
        k.chorus_rate = 0.45f;
        k.shimmer = 0.12f;
        k.rev_size = 0.9f;
        k.rev_mod = 2.5f;
        k.damp_hz = 7000.0f;
        break;
    case SP_RADIO:                       /* a small transistor radio: thin, narrow, a little driven */
        k.radio = 1.0f;
        k.tone_fc = fm_minf(k.tone_fc, 3000.0f);
        k.echo_lp_max = onepole_a(2500.0f);
        k.rev_size = 0.35f;
        k.rev_rt = 0.8f;
        k.rev_mix *= 0.5f;
        break;
    case SP_WATER:                       /* under the surface: muffled, slow and swaying */
        k.tone_fc = fm_minf(k.tone_fc, 650.0f);
        k.chorus = 1.0f;
        k.chorus_rate = 0.18f;
        k.wob_add = 0.3f;
        k.rev_size = 1.0f;
        k.rev_rt = fm_minf(k.rev_rt * 1.3f + 1.5f, 16.0f);
        k.damp_hz = 1500.0f;
        break;
    case SP_GLITCH:                      /* the echoes stutter and jump */
        k.alg_rand = 0.9f;
        k.echo_mix = fm_maxf(k.echo_mix, 0.6f);
        k.echo_fb = fm_maxf(k.echo_fb, 0.5f);
        k.rev_size *= 0.5f;
        k.rev_rt *= 0.6f;
        break;
    case SP_PULSE:                       /* the sound throbs and pans in eighths */
        k.trem = 0.75f;
        k.rev_size *= 0.6f;
        k.rev_rt *= 0.7f;
        k.damp_hz = 9000.0f;
        break;
    }
    k.dust = k.wobble = k.weather = k.life = k.density = k.drift = k.crush_bits = 0.0f;
    k.wind = k.rain = k.surf = k.creek = k.fire = k.thunder_rate = k.bird_rate = k.note_rate = 0.0f;
    k.rain_tone = 6500.0f;
    k.pad_bright = 0.4f;
    for (i = 0; i < MS_NMIX; i++)        /* EVOLVE moves a layer that is on, never one turned off */
        st.layer_to[i] = st.mix[i] ? lvl(clamp(st.mix[i] + (int)evo.moff[i], 5, 100)) : 0.0f;
    switch (st.mode) {
    case MODE_LOFI:
        k.dust = mac(1);
        k.wobble = mac(2);
        k.crush_bits = k.dust > 0.4f ? fm_exp2f(15.0f - 6.0f * (k.dust - 0.4f) / 0.6f) : 0.0f;
        k.rain = st.mix[3] ? 0.55f : 0.0f;
        k.rain_tone = 5000.0f;
        break;
    case MODE_SCAPES: {
        const scape_style_t *ss = &SCAPE_STYLE[st.scene];
        float w = 0.3f + 1.4f * mac(0);
        k.weather = mac(0);
        k.life = mac(1);
        k.wind = fm_minf(ss->wind * w, 1.0f);
        k.rain = fm_minf(ss->rain * w, 1.0f);
        k.surf = fm_minf(ss->surf * w, 1.0f);
        k.creek = fm_minf(ss->creek * w, 1.0f);
        k.fire = fm_minf(ss->fire * w, 1.0f);
        k.thunder_rate = ss->thunder * w * w;
        k.bird_rate = ss->bird_rate * 2.0f * k.life;
        k.note_rate = ss->note_rate;
        k.rain_tone = ss->rain_tone;
        k.pad_bright = ss->pad_bright;
        break;
    }
    case MODE_DRIFT:
        k.density = mac(0);
        k.drift = mac(2);
        k.wind = 0.3f;
        k.pad_bright = DRIFT_STYLE[st.scene].pad_bright * (0.5f + mac(1));
        k.bird_rate = DRIFT_STYLE[st.scene].bird ? 4.0f : 0.0f;
        break;
    }
    if (mo.time_hold <= 0.0f || base != mo.time_base) {   /* a MOTION jump stays until its time is up */
        echo_len_target = base;
        mo.time_hold = 0.0f;
    }
    mo.time_base = base;
    svf_set(&ms.tone_l, k.tone_fc, 0.6f);
    svf_set(&ms.tone_r, k.tone_fc, 0.6f);
    reverb_params(st.frozen);
}

static void mode_start(void)
{
    int i;
    voices_release(1);
    bass.env_target = 0.0f;
    for (i = 0; i < NEVT; i++)
        ev_used[i] = 0;
    seq.step = 0;
    seq.acc = 0.0f;
    hm.prog_i = -1;
    hm.pending = -1;
    hm.bar_in_chord = 99;
    seq.mel_last = 9;
    apply();
    switch (st.mode) {
    case MODE_LOFI:
        dr.snare_kind = LOFI_STYLE[st.scene].snare_kind;
        ms_vis.drums[0] = LOFI_STYLE[st.scene].kick;
        ms_vis.beat_len = 16;
        ms_vis.drums[1] = LOFI_STYLE[st.scene].snare;
        ms_vis.drums[2] = LOFI_STYLE[st.scene].hat;
        hm.prog_i = start_index();
        chord_set(chord_at(LOFI_STYLE[st.scene].prog, hm.prog_i));
        break;
    case MODE_SCAPES:
        crickets_init(SCAPE_STYLE[st.scene].crickets);
        for (i = 0; i < NBIRD; i++) {
            bd[i].species = SCAPE_STYLE[st.scene].bird[i];
            bd[i].syl_left = 0;
            bd[i].wait = (int32_t)((2.0f + 6.0f * frand()) * MS_SR);
        }
        i = start_index();               /* the scene's start, which STILL then keeps */
        hm.prog_i = i;
        chord_set(chord_at(SCAPE_STYLE[st.scene].prog, i));
        hm.prog_i = i - 1;               /* so the first change plays it, not the one after */
        seq.chord_t = 0.0f;
        seq.note_acc = 3.0f;
        break;
    case MODE_DRIFT:
        hm.prog_i = start_index();
        chord_set(chord_at((const int8_t *)0, hm.prog_i));
        drift_regrow();
        if (bt.bass == BASS_AUTO)
            bass_note(36 + st.root % 12, 0.8f, -1, 1.6f, 3.0f);
        if (DRIFT_STYLE[st.scene].lush)
            lush_chord();
        seq.chord_t = DRIFT_STYLE[st.scene].lush ? 20.0f : 40.0f;
        break;
    }
    if (st.mode != MODE_SCAPES) {
        crickets_init(0);
        for (i = 0; i < NBIRD; i++) {
            bd[i].species = st.mode == MODE_DRIFT ? DRIFT_STYLE[st.scene].bird : BIRD_NONE;
            bd[i].syl_left = 0;
            bd[i].wait = (int32_t)((2.0f + 6.0f * frand()) * MS_SR);
        }
    }
    th.active = 0;
}

static void new_variation(void)
{
    switch (st.mode) {
    case MODE_LOFI:
        hm.prog_i = irand(4) - 1;
        hm.bar_in_chord = 99;
        seq.step = 0;
        break;
    case MODE_SCAPES:
        na.gust_target = frand();
        if (k.thunder_rate > 0.0f)
            thunder_strike();
        seq.chord_t = 0.0f;
        break;
    case MODE_DRIFT:
        drift_regrow();
        drift_next_chord();
        break;
    }
}

/* shimmer: the reverb input an octave up, two crossfaded grains read at twice the write speed */
#define SH_N 8192
#define SH_G 2048
static int16_t sh_buf[SH_N] PLAT_POOL;
static struct {
    uint32_t w, age[2];
} sh = {0, {0, SH_G / 2}};

HOT float shimmer_tick(float in)
{
    float o = 0.0f;
    int g;
    sh_buf[sh.w] = to16(in);
    for (g = 0; g < 2; g++) {
        uint32_t a = sh.age[g];
        float t = (float)a * (1.0f / (float)SH_G), w = 4.0f * t * (1.0f - t);
        if (k.sh_down) {                 /* read at half speed: an octave down; odd ages between two samples */
            uint32_t i = sh.w - SH_G / 4u - (a >> 1);
            float v = (float)sh_buf[i & (SH_N - 1)];
            if (a & 1u)
                v = 0.5f * (v + (float)sh_buf[(i - 1u) & (SH_N - 1)]);
            o += v * (1.0f / 32767.0f) * w * w;
        } else {
            o += (float)sh_buf[(sh.w - (2u * SH_G - a)) & (SH_N - 1)] * (1.0f / 32767.0f) * w * w;
        }
        if (++sh.age[g] >= SH_G)
            sh.age[g] = 0;
    }
    sh.w = (sh.w + 1) & (SH_N - 1);
    return o;
}

static int key_kind(void)
{
    static const uint8_t KIND[] = {V_EP, V_EP, V_BELL, V_PAD, V_KALIMBA};
    if (kb.voice != KV_AUTO)
        return KIND[clamp(kb.voice, 0, 4)];
    return st.mode == MODE_LOFI ? V_EP : st.mode == MODE_SCAPES ? SCAPE_STYLE[st.scene].note_kind : DRIFT_STYLE[st.scene].bell_kind;
}

/* nearest note in the scale or chord; ties go up */
static int snap_note(int note)
{
    uint32_t mask;
    int d;
    if (kb.snap == SNAP_FREE)
        return note;
    mask = kb.snap == SNAP_CHORD ? ms_vis.chord_mask : hm.scale_mask;
    if (!mask)
        return note;
    for (d = 0; d < 12; d++) {
        if (mask >> ((note + d) % 12) & 1u)
            return note + d;
        if (mask >> ((note - d + 120) % 12) & 1u)
            return note - d;
    }
    return note;
}

static int scale_step(int note, int dir)
{
    uint32_t mask = hm.scale_mask;
    int g;
    note += dir;
    for (g = 0; g < 12 && !(mask >> ((note + 120) % 12) & 1u); g++)
        note += dir;
    return note;
}

/* snaps the note and adds chord tones below it per HARMONY; out: the notes that sound */
static void key_sound(int8_t out[3], int note, float vel, int layer, int32_t gate, float bloom)
{
    int kind = key_kind(), n = clamp(snap_note(note), 24, 108), i, h = n;
    float atk = (kind == V_PAD ? 0.25f : 0.004f) + bloom, rel = kind == V_EP ? 0.3f : kind == V_PAD ? 2.5f : 1.5f;
    voice_t *v = voice_start(kind, n, vel, layer, gate, atk, rel, 0.0f);
    v->rev = fm_minf(v->rev + bloom, 1.0f);
    out[0] = (int8_t)n;
    out[1] = out[2] = -1;
    for (i = 1; i < 3 && kb.harmony > 0.0f; i++) {
        if (frand() >= (i == 1 ? kb.harmony * 1.4f : (kb.harmony - 0.5f) * 2.0f))
            break;
        for (h -= 3; h > n - 15; h--)
            if (h >= 24 && ms_vis.chord_mask >> (h % 12) & 1u)
                break;
        if (h <= n - 15 || h < 24)
            break;
        voice_start(kind, h, vel * 0.55f, layer, gate, atk + 0.03f * (float)i, rel, i == 1 ? -0.35f : 0.35f);
        out[i] = (int8_t)h;
    }
}

enum { LP_EMPTY, LP_REC, LP_PLAY, LP_DUB };
#define NLEV 128
#define LOOP_MAX_S 60.0f
typedef struct {
    float t, dur, vel;
    int8_t note, open;
    uint8_t track;
} lev_t;
static struct {
    int state, n, speed;
    lev_t ev[NLEV];
    float pos, len, u;                 /* samples; pos: clock while recording; u: playhead in [0, len) */
    float fade, mutate, smear;
    int autorec;                       /* LISTEN started the recording, so a pause closes it */
    float quiet;                       /* samples since the last key event */
    int track;                         /* where new notes go, 0..MS_NLAYERS - 1 */
    int redo;                          /* the layer the next phrase replaces, -1 none */
    int adding;                        /* LISTEN: a phrase going onto a playing loop, until its pause */
    int keep;                          /* REC kept it: it no longer fades */
    int synced;                        /* a beat to follow: the loop is whole bars and its notes sit on the groove */
    float rec_off;                     /* where in the bar the recording began, samples */
    float rec_end;                     /* the clock when it closed, before a quiet tail was cut */
    uint32_t mute;                     /* bit per track */
    float tgain[MS_NLAYERS];
} lp;

static int loop_next_track(void)
{
    uint32_t used = 0;
    int i;
    for (i = 0; i < lp.n; i++)
        used |= 1u << lp.ev[i].track;
    for (i = 0; i < MS_NLAYERS; i++)
        if (!(used >> i & 1u)) {
            lp.mute &= ~(1u << i);   /* a stale mute must not silence what is recorded next */
            return i;
        }
    lp.mute &= ~(1u << (MS_NLAYERS - 1));
    return MS_NLAYERS - 1;
}

/* the layer a new phrase goes on: the one armed for REDO, emptied now that its new take begins, else a free one */
static int loop_phrase_track(void)
{
    int t = lp.redo, i, kept = 0;
    if (t < 0)
        return loop_next_track();
    for (i = 0; i < lp.n; i++)
        if (lp.ev[i].track != t || lp.ev[i].open)
            lp.ev[kept++] = lp.ev[i];
    lp.n = kept;
    lp.mute &= ~(1u << t);
    lp.redo = -1;
    return t;
}
static const float LOOP_RATE[5] = {0.5f, 1.0f, 2.0f, 1.0f, 0.5f};   /* speeds 3 and 4 play backwards */

static float loop_now(void) { return lp.state == LP_REC ? lp.pos : lp.u; }

static float sixteenth_len(void) { return 60.0f / (float)st.tempo / 4.0f * MS_SR; }

/* where in the bar the music is now, samples */
static float bar_pos(void)
{
    float six = sixteenth_len();
    uint32_t cur = seq.step ? seq.step - 1u : 0u;
    return (float)(cur & 15u) * six + fm_clampf(six - seq.acc, 0.0f, six);
}

/* the nearest point of the groove's grid: sixteenths in pairs, the second one late by the swing */
static float groove_snap(float t)
{
    float six = sixteenth_len(), pair = 2.0f * six, base = fm_floorf(t / pair) * pair, in = t - base;
    float late = six * (1.0f + beat_swing()), best = 0.0f;
    if (fm_fabsf(in - late) < fm_fabsf(in - best))
        best = late;
    if (fm_fabsf(in - pair) < fm_fabsf(in - best))
        best = pair;
    return base + best;
}

static void loop_add(int note, float vel)
{
    lev_t *e;
    if (lp.n >= NLEV) {                  /* full: replace the quietest closed note */
        int i, q = -1;
        for (i = 0; i < lp.n; i++)
            if (!lp.ev[i].open && (q < 0 || lp.ev[i].vel < lp.ev[q].vel))
                q = i;
        if (q < 0)
            return;
        e = &lp.ev[q];
    } else {
        e = &lp.ev[lp.n++];
    }
    e->t = loop_now();
    if (lp.state == LP_PLAY)
        lp.adding = 1;
    if (lp.synced && lp.state != LP_REC) {   /* an overdub lands on the groove; a recording is placed when it closes */
        e->t = groove_snap(e->t);
        if (e->t >= lp.len)
            e->t -= lp.len;
    }
    e->dur = 0.0f;
    e->vel = vel;
    e->note = (int8_t)note;
    e->open = 1;
    e->track = (uint8_t)lp.track;
}

static void loop_close(int note)
{
    int i;
    for (i = 0; i < lp.n; i++)
        if (lp.ev[i].open && (note < 0 || lp.ev[i].note == note)) {
            float d = loop_now() - lp.ev[i].t;
            if (d < 0.0f)
                d += lp.len;
            lp.ev[i].dur = d;                /* u already runs at the loop's speed, so d is in loop time */
            if (lp.state != LP_REC && lp.speed >= 3) {   /* played backwards: store it where it will sound */
                float t = lp.len - lp.ev[i].t - d;
                lp.ev[i].t = t < 0.0f ? t + lp.len : t;
            }
            lp.ev[i].open = 0;
        }
}

static void loop_tap(void)
{
    int i;
    float t0;
    switch (lp.state) {
    case LP_EMPTY:
        lp.n = 0;
        lp.pos = 0.0f;
        lp.track = 0;
        lp.mute = 0;
        lp.keep = 0;
        ms_vis.lp_keep = 0;
        lp.synced = st.playing && (st.mode == MODE_LOFI || bt.kit != KIT_OFF);
        lp.rec_off = lp.synced ? bar_pos() : 0.0f;
        lp.rec_end = -1.0f;
        lp.state = LP_REC;
        break;
    case LP_REC:
        loop_close(-1);
        if (!lp.n) {
            lp.state = LP_EMPTY;
            break;
        }
        if (lp.synced) {                 /* whole bars from the bar it began in, notes on the groove, in step now */
            float bar = 16.0f * sixteenth_len(), end = lp.pos + lp.rec_off, now = (lp.rec_end >= 0.0f ? lp.rec_end : lp.pos) + lp.rec_off;
            int bars = 1;
            while ((float)bars * bar < end - 0.25f * bar && bars < 8 && (float)(bars * 2) * bar <= LOOP_MAX_S * MS_SR)
                bars *= 2;
            lp.len = (float)bars * bar;
            for (i = 0; i < lp.n; i++) {
                float t = groove_snap(lp.ev[i].t + lp.rec_off);
                lp.ev[i].t = t - fm_floorf(t / lp.len) * lp.len;
            }
            lp.u = now - fm_floorf(now / lp.len) * lp.len;
        } else {
            t0 = lp.ev[0].t;             /* free time: trim the silence before the first note */
            for (i = 0; i < lp.n; i++)
                lp.ev[i].t -= t0;
            lp.len = fm_maxf(lp.pos - t0, (lp.autorec ? 2.5f : 0.3f) * MS_SR);
            lp.u = 0.0f;
        }
        lp.autorec = 0;
        lp.state = LP_PLAY;
        ms_vis.lp_gen++;
        break;
    case LP_PLAY:
        lp.track = loop_phrase_track();
        lp.state = LP_DUB;
        break;
    default:
        loop_close(-1);
        lp.state = LP_PLAY;
        break;
    }
}

static void loop_pass(void)
{
    int i, kept = 0;
    for (i = 0; i < lp.n; i++) {
        lev_t e = lp.ev[i];
        if (e.open) {
            lp.ev[kept++] = e;
            continue;
        }
        if (lp.state == LP_PLAY && !lp.keep && !(lp.mute >> e.track & 1u))
            e.vel *= 1.0f - 0.45f * lp.fade;
        if (frand() < lp.mutate * 0.5f) {
            float r = frand();
            if (r < 0.55f)
                e.note = (int8_t)clamp(scale_step(e.note, frand() < 0.5f ? -1 : 1), 36, 96);
            else if (r < 0.8f)
                e.note = (int8_t)clamp(e.note + (frand() < 0.5f ? -12 : 12), 36, 96);
            else
                e.t = frand() * lp.len;
        }
        if (e.vel >= 0.05f)
            lp.ev[kept++] = e;
    }
    lp.n = kept;
    if (!lp.n && lp.state == LP_PLAY)
        lp.state = LP_EMPTY;
}

/* each track's notes as 32 time bins, for the screen */
static void loop_vis(void)
{
    uint32_t dots[MS_NLAYERS] = {0}, used = 0;
    int i;
    for (i = 0; i < lp.n; i++) {
        float len = lp.state == LP_REC ? lp.pos : lp.len;
        int b = len > 0.0f ? (int)(lp.ev[i].t / len * 32.0f) : 0;
        dots[lp.ev[i].track % MS_NLAYERS] |= 1u << clamp(b, 0, 31);
        used |= 1u << (lp.ev[i].track % MS_NLAYERS);
    }
    if (lp.state == LP_EMPTY)
        lp.redo = -1;
    for (i = 0; i < MS_NLAYERS; i++)
        ms_vis.lp_dots[i] = dots[i];
    ms_vis.lp_redo = (int8_t)lp.redo;
    ms_vis.lp_tracks = (uint8_t)used;
    ms_vis.lp_mute = (uint8_t)lp.mute;
    ms_vis.lp_track = (uint8_t)lp.track;
}

ms_slot_t ms_slot_xfer;

static void loop_snap(void)
{
    ms_slot_t *o = &ms_slot_xfer;
    uint8_t order[NLEV];
    int i, j, n = 0;
    o->n = 0;
    o->len_ms = (uint16_t)fm_minf(lp.len / MS_SR * 1000.0f, 65000.0f);
    o->mute = (uint8_t)lp.mute;
    o->tempo = (uint8_t)st.tempo;
    o->rsv[0] = MS_SLOT_FORMAT;
    if (lp.state != LP_PLAY && lp.state != LP_DUB)
        return;
    for (i = 0; i < lp.n; i++) {         /* by velocity, loudest first: the ones that fit are kept */
        if (lp.ev[i].open)
            continue;
        for (j = n; j > 0 && lp.ev[order[j - 1]].vel < lp.ev[i].vel; j--)
            order[j] = order[j - 1];
        order[j] = (uint8_t)i;
        n++;
    }
    for (i = 0; i < n && i < MS_SLOT_EVENTS; i++) {
        const lev_t *e = &lp.ev[order[i]];
        ms_slot_ev_t *d = &o->ev[i];
        d->t_ms = (uint16_t)fm_minf(e->t / MS_SR * 1000.0f, 65000.0f);
        d->dur_ms = (uint16_t)fm_minf(e->dur / MS_SR * 1000.0f, 65000.0f);
        d->note = (uint8_t)clamp(e->note, 0, 127);
        d->vt = (uint8_t)(clamp((int)(e->vel * 31.0f + 0.5f), 1, 31) << 3 | (e->track & 7u));
    }
    o->n = (uint8_t)i;
}

static void loop_load(void)
{
    const ms_slot_t *o = &ms_slot_xfer;
    int i, n = o->n > MS_SLOT_EVENTS ? MS_SLOT_EVENTS : o->n;
    if (!n || o->len_ms < 50u)
        return;
    for (i = 0; i < n; i++) {
        lev_t *e = &lp.ev[i];
        e->t = (float)o->ev[i].t_ms * (MS_SR / 1000.0f);
        e->dur = (float)o->ev[i].dur_ms * (MS_SR / 1000.0f);
        e->note = (int8_t)clamp(o->ev[i].note, 0, 127);
        e->vel = (float)(o->ev[i].vt >> 3) / 31.0f;
        e->track = o->ev[i].vt & 7u;
        e->open = 0;
    }
    lp.n = n;
    lp.len = (float)o->len_ms * (MS_SR / 1000.0f);
    for (i = 0; i < n; i++)              /* times past the end (a corrupt slot) fold back in */
        if (lp.ev[i].t >= lp.len)
            lp.ev[i].t -= fm_floorf(lp.ev[i].t / lp.len) * lp.len;
    lp.mute = o->mute;
    lp.redo = -1;
    lp.u = 0.0f;
    lp.autorec = 0;
    lp.track = loop_next_track();
    lp.state = LP_PLAY;
    ms_vis.lp_gen++;
}

static int crossed(float t, float u0, float u1, float len)
{
    return u1 < len ? t >= u0 && t < u1 : t >= u0 || t < u1 - len;
}

static void loop_tick(void)
{
    float rate, u1;
    int i, rev;
    lp.quiet += (float)CTL;
    if (lp.quiet > 2.0f * MS_SR || lp.state != LP_PLAY)
        lp.adding = 0;
    ms_vis.lp_adding = (uint8_t)lp.adding;
    if (lp.state == LP_REC) {
        int open = 0;
        lp.pos += (float)CTL;
        for (i = 0; i < lp.n; i++)
            open |= lp.ev[i].open;
        if (lp.autorec && !open && lp.quiet > 2.0f * MS_SR) {   /* a pause ends the phrase */
            lp.rec_end = lp.pos;
            lp.pos -= lp.quiet - 0.8f * MS_SR;
            loop_tap();
        } else if (lp.pos > LOOP_MAX_S * MS_SR) {
            loop_tap();
        }
        ms_vis.lp_pos = lp.pos / MS_SR;
    }
    if ((lp.state == LP_PLAY || lp.state == LP_DUB) && lp.len < 1.0f)
        lp.state = LP_EMPTY;
    if (lp.state != LP_PLAY && lp.state != LP_DUB) {
        ms_vis.lp_state = (uint8_t)lp.state;
        ms_vis.lp_n = (uint8_t)lp.n;
        loop_vis();
        return;
    }
    rate = LOOP_RATE[lp.speed];
    rev = lp.speed >= 3;
    u1 = lp.u + (float)CTL * rate;
    for (i = 0; i < lp.n; i++) {
        lev_t *e = &lp.ev[i];
        float on = rev ? lp.len - e->t - e->dur : e->t;
        int8_t out[3];
        if (e->open || (lp.mute >> e->track & 1u))
            continue;
        if (on < 0.0f)
            on += lp.len;
        if (on < 0.0f)
            on = 0.0f;
        if (lp.tgain[e->track] > 0.01f && crossed(on, lp.u, u1, lp.len))
            key_sound(out, e->note, e->vel * lp.tgain[e->track], SRC_LOOP, (int32_t)(fm_maxf(e->dur, 0.08f * MS_SR) / rate), lp.smear * 0.9f);
    }
    if (u1 >= lp.len) {
        lp.u = u1 - lp.len;
        loop_pass();
    } else {
        lp.u = u1;
    }
    ms_vis.lp_state = (uint8_t)lp.state;
    ms_vis.lp_n = (uint8_t)lp.n;
    ms_vis.lp_pos = lp.len > 0.0f ? lp.u / lp.len : 0.0f;
    ms_vis.lp_len = lp.len / MS_SR;
    loop_vis();
}

/* the degree whose chord contains pc and shares the most tones with the current chord, -1 none */
static int degree_with(int pc)
{
    int d, i, best = -1, best_shared = -1;
    for (d = 0; d < 7; d++) {
        int has = 0, shared = 0;
        for (i = 0; i < 4; i++) {
            int t = (st.root + deg_semis(d + 2 * i)) % 12;
            has |= t == pc;
            shared += (int)(ms_vis.chord_mask >> t & 1u);
        }
        if (has && d != hm.degree && shared > best_shared) {
            best = d;
            best_shared = shared;
        }
    }
    return best;
}

/* a played note pulls the chord toward it, joins DRIFT's loops or LOFI's melody, and can prompt a bird call */
static void plant(int note)
{
    int pc = note % 12, i;
    kb.energy = fm_minf(kb.energy + 0.12f, 1.0f);
    if (st.mode == MODE_LOFI) {
        seq.mel_pool[seq.mel_n++ & 3] = (int8_t)note;
        return;
    }
    if (!(ms_vis.chord_mask >> pc & 1u) && !st.frozen && hc.lock < 0 && hc.path != PATH_BUILT) {
        int d = degree_with(pc);
        if (d >= 0) {
            hm.pending = d;
            if (seq.chord_t > 4.0f)
                seq.chord_t = 2.0f + 2.0f * frand();
        }
    }
    if (st.mode == MODE_DRIFT) {
        int l = irand(NLOOP), j = (int)((dg.pos[l] + 1u + (uint32_t)irand(2)) % dg.len[l]), d, best = 7, bd2 = 99;
        int target = note;
        while (target < 62)
            target += 12;
        while (target > 86)
            target -= 12;
        for (d = 7; d < 21; d++) {
            int dist = st.root + deg_semis(d) + 48 - target;
            if (dist < 0)
                dist = -dist;
            if (dist < bd2) {
                bd2 = dist;
                best = d;
            }
        }
        dg.deg[l][j] = (int8_t)best;
        ms_vis.loop_notes[l] = loop_mask(l);
    } else {
        for (i = 0; i < NBIRD; i++)
            if (bd[i].species != BIRD_NONE && !bd[i].syl_left && bd[i].wait > (int32_t)(1.5f * MS_SR)) {
                bd[i].wait = (int32_t)((0.6f + 0.9f * frand()) * MS_SR);
                break;
            }
    }
}

static int loop_listening(void)
{
    return lp.state == LP_REC || lp.state == LP_DUB || (kb.listen && lp.state == LP_PLAY);
}

/* RANDOM: now and then the delay jumps to another division (its glide bends the repeats' pitch), the repeats swell
 * toward endless for a moment, or the reverb blooms */
static void motion_tick(float dt)
{
    float rnd;
    mo.mod_ph = wrap1(mo.mod_ph + (0.25f + 0.9f * mo.mod) * dt);
    mo.mod_m = sin_t(mo.mod_ph);
    mo.fb_boost *= 1.0f - dt / 1.5f;
    if (mo.time_hold > 0.0f && (mo.time_hold -= dt) <= 0.0f)
        echo_len_target = mo.time_base;
    mo.rev_boost *= 1.0f - dt / 3.0f;
    rnd = fm_maxf(mo.random, k.alg_rand);
    if (rnd <= 0.0f || (mo.ev_t -= dt) > 0.0f)
        return;
    mo.ev_t = (1.0f + 6.0f * frand()) * (1.2f - rnd);
    if (frand() > rnd)
        return;
    switch (irand(3)) {
    case 0:                              /* GLITCH jumps between short times and soon comes back */
        echo_len_target = fm_minf(DIV_BEATS[irand(k.alg_rand > 0.0f ? 3 : 6)] * 60.0f / (float)st.tempo * MS_SR * 0.5f,
                                  (float)(ECHO_N - 4));
        mo.time_hold = k.alg_rand > 0.0f ? 0.4f + 1.2f * frand() : 3.0f + 5.0f * frand();
        break;
    case 1:
        mo.fb_boost = fm_maxf(0.0f, 0.9f - k.echo_fb);
        break;
    default:
        mo.rev_boost = 0.5f;
        break;
    }
}

static void motion_set(int i, int v)
{
    float x = (float)clamp(v, 0, 100) * 0.01f;
    if (i == 0)
        mo.mod = x;
    else if (i == 1)
        mo.random = x;
    else if (i == 2) {                   /* TONE tilts: left of noon darker, right of it thinner */
        mo.tone_a = onepole_a(500.0f * fm_exp2f(4.6f * fm_minf(2.0f * x, 1.0f)));
        mo.hp_a = x > 0.5f ? onepole_a(40.0f * fm_exp2f(5.5f * (2.0f * x - 1.0f))) : 0.0f;
    } else {
        mo.spread = x;
    }
}

static void seeds_vis(void)
{
    uint32_t m[4] = {0, 0, 0, 0};
    int i;
    for (i = 0; i < 8; i++)
        if (kb.seed[i] >= 0)
            m[kb.seed[i] >> 5] |= 1u << (kb.seed[i] & 31);
    for (i = 0; i < 4; i++)
        ms_vis.hold[i] = m[i];
}

static void seeds_clear(void)
{
    int i;
    for (i = 0; i < 8; i++)
        kb.seed[i] = -1;
    seeds_vis();
}

static float beat_sec(void) { return 60.0f / (float)st.tempo; }

/* HOLD: a tap plants a note, a second tap pulls it. Each planted note repeats on its own cycle of whole beats (5, 7,
 * 6, 9...), in the scene's own voice, so together they phase against each other like tape loops of different
 * lengths and always land on the beat. Chance only colours each repeat: now and then an octave up, an answer a
 * fifth above, a low swell or a rest. The eighth seed makes room by taking the place of the oldest */
static const uint8_t SEED_CYCLE[8] = {5, 7, 6, 9, 11, 8, 13, 10};
static const float SEED_PAN[8] = {-0.45f, 0.45f, -0.2f, 0.3f, -0.6f, 0.6f, 0.0f, -0.35f};

static void hold_toggle(int note, float vel)
{
    int i, free = -1;
    float beat = beat_sec(), into;
    for (i = 0; i < 8; i++) {
        if (kb.seed[i] == note) {
            kb.seed[i] = -1;
            seeds_vis();
            return;
        }
        if (kb.seed[i] < 0 && free < 0)
            free = i;
    }
    if (free < 0)
        for (free = 0, i = 1; i < 8; i++)
            if (kb.seed_age[i] < kb.seed_age[free])
                free = i;
    kb.seed[free] = (int8_t)note;
    kb.seed_age[free] = ++kb.seed_clock;
    into = bar_pos() / MS_SR;
    into -= fm_floorf(into / beat) * beat;
    kb.seed_t[free] = (beat - into) + beat * (float)(SEED_CYCLE[free] / 2);   /* first repeat on a beat */
    voice_start(key_kind(), clamp(snap_note(note), 36, 96), 0.3f + 0.2f * vel, SRC_KEYS, (int32_t)(0.6f * MS_SR), 0.005f,
                2.0f, SEED_PAN[free]);
    plant(note);
    seeds_vis();
}

static void seed_play(int i)
{
    int n = clamp(snap_note(kb.seed[i]), 36, 96), kind = key_kind();
    float r = frand(), vel = 0.32f + 0.12f * frand(), pan = SEED_PAN[i];
    int32_t gate = (int32_t)(0.8f * beat_sec() * MS_SR);
    if (r < 0.06f)
        return;                          /* a rest */
    if (r < 0.70f) {
        voice_start(kind, n, vel, SRC_KEYS, gate, 0.004f, 1.8f, pan);
    } else if (r < 0.84f) {
        voice_start(kind, clamp(n + 12, 36, 96), vel * 0.7f, SRC_KEYS, gate, 0.004f, 2.2f, pan);
    } else if (r < 0.94f) {
        voice_start(kind, n, vel, SRC_KEYS, gate, 0.004f, 1.8f, pan);
        ev_add((int32_t)(beat_sec() * MS_SR), EV_NOTE, kind, clamp(scale_step(n + 6, 1), 36, 96), vel * 0.75f, SRC_KEYS,
               gate, -pan);
    } else {
        voice_start(V_PAD, clamp(n - 12, 36, 96), vel * 0.7f, SRC_KEYS, (int32_t)(2.0f * beat_sec() * MS_SR), 1.0f,
                    3.0f, pan);
    }
}

static void seeds_tick(float dt)
{
    int i;
    if (!kb.hold || !st.playing)
        return;
    for (i = 0; i < 8; i++)
        if (kb.seed[i] >= 0 && (kb.seed_t[i] -= dt) <= 0.0f) {
            seed_play(i);
            kb.seed_t[i] += beat_sec() * (float)SEED_CYCLE[i];   /* added, not set, so it stays on the beat */
        }
}

void ms_note_now(int note, int vel)
{
    int8_t *h;
    int i, was;
    note = clamp(note, 0, 127);
    if (kb.hold && (vel || kb.held[note][0] < 0)) {   /* a key held from before HOLD still gets its note-off */
        if (vel)
            hold_toggle(note, (float)vel / 127.0f);
        return;
    }
    h = kb.held[note];
    was = h[0];
    for (i = 0; i < 3; i++)
        if (h[i] >= 0) {
            voice_release_note(h[i], SRC_KEYS);
            h[i] = -1;
        }
    if (vel && kb.listen && lp.state == LP_PLAY && (lp.quiet > 2.0f * MS_SR || lp.redo >= 0))
        lp.track = loop_phrase_track();  /* a new phrase after a pause gets its own track */
    lp.quiet = 0.0f;
    if (vel) {
        key_sound(h, note, (float)vel / 127.0f, SRC_KEYS, -1, 0.0f);
        if (kb.listen && lp.state == LP_EMPTY)
            loop_tap();
        if (kb.listen && lp.state == LP_REC)   /* LISTEN: once you play, a pause closes it, REC-started or not */
            lp.autorec = 1;
        if (loop_listening())
            loop_add(h[0], (float)vel / 127.0f);
        if (kb.listen)
            plant(h[0]);
    } else if (was >= 0 && loop_listening()) {
        loop_close(was);
    }
}

/* the chord the key has on pitch class pc, a 7th; major off the key */
static int key_chord_kind(int pc)
{
    uint16_t key = ms_scale_mask(st.mood, st.root);
    int d = 0, o = (pc - st.root + 12) % 12, third, fifth, seventh;
    if (!(key >> pc & 1u))
        return MS_CHORD_MAJ;
    while (SCALE[st.mood][d] != o)
        d++;
    third = (SCALE[st.mood][(d + 2) % 7] - o + 12) % 12;
    fifth = (SCALE[st.mood][(d + 4) % 7] - o + 12) % 12;
    seventh = (SCALE[st.mood][(d + 6) % 7] - o + 12) % 12;
    return fifth == 6 ? MS_CHORD_HALFDIM : third == 3 ? MS_CHORD_MIN7 : seventh == 11 ? MS_CHORD_MAJ7 : MS_CHORD_DOM7;
}

/* a chord's notes on root note r, as VOICING lays them: CLOSE stacked over the root; OPEN the root an octave down
 * and the rest spread over two octaves; WIDE the root down, then the 3rd, the 7th (a triad's 5th an octave up) and
 * the 9th or the 5th above; SMOOTH the root down and the rest moved by octaves to sit near the last chord, so
 * chords flow into each other */
static int live_voicing(int r, int kind, int8_t *out)
{
    const int8_t *t = KIND_TONES[kind];
    int n = 0, i, tone[5], k = 0;
    uint16_t seen = 0;
    for (i = 0; i < 5; i++)              /* the chord's own notes, a doubled root or 5th left out */
        if (!(seen >> (t[i] % 12) & 1u)) {
            seen |= (uint16_t)(1u << (t[i] % 12));
            tone[k++] = t[i];
        }
    switch (cp.voicing) {
    case 0:
        for (i = 0; i < k; i++)
            out[n++] = (int8_t)(r + tone[i]);
        break;
    case 1:
        out[n++] = (int8_t)(r - 12);
        for (i = 1; i < k; i++)
            out[n++] = (int8_t)(r + tone[i] + (i & 1 ? 12 : 0));
        break;
    case 2:
        out[n++] = (int8_t)(r - 12);
        out[n++] = (int8_t)(r + tone[1]);
        out[n++] = (int8_t)(r + (k > 3 ? tone[3] : tone[2] + 12));
        if (k > 4)
            out[n++] = (int8_t)(r + tone[4] + (tone[4] < 12 ? 12 : 0));
        else if (k > 3)
            out[n++] = (int8_t)(r + tone[2] + 12);
        break;
    default: {
        float sum = 0.0f;
        out[n++] = (int8_t)(r - 12);
        for (i = 1; i < k; i++) {
            int x = r + tone[i];
            while (x < cp.centre - 6.0f)
                x += 12;
            while (x > cp.centre + 5.0f)
                x -= 12;
            out[n++] = (int8_t)x;
            sum += (float)x;
        }
        if (k > 1)
            cp.centre += 0.5f * (sum / (float)(k - 1) - cp.centre) + 0.1f * (64.0f - cp.centre);
        break;
    }
    }
    for (i = 0; i < n; i++) {            /* by octaves, so every note keeps its name */
        int x = out[i];
        while (x < 28)
            x += 12;
        while (x > 100)
            x -= 12;
        out[i] = (int8_t)x;
    }
    return n;
}

static float live_gap(void) { return 0.15f * cp.strum * cp.strum + 0.01f * cp.strum; }

/* another chord held has this note sounding: it stays */
static int live_shared(int s, int note)
{
    int o, i;
    for (o = 0; o < NLIVE; o++)
        if (o != s && cp.key[o] >= 0)
            for (i = 0; i < cp.started[o]; i++)
                if (cp.note[o][i] == note)
                    return 1;
    return 0;
}

static void live_release(int s)
{
    int i;
    for (i = 0; i < cp.started[s]; i++) {
        if (live_shared(s, cp.note[s][i]))
            continue;
        voice_release_note(cp.note[s][i], SRC_KEYS);
        if (loop_listening())
            loop_close(cp.note[s][i]);
    }
    cp.key[s] = -1;
}

static void live_start_note(int s)
{
    int i = cp.started[s]++, kind = key_kind();
    float atk = kind == V_PAD ? 0.25f : 0.004f, rel = kind == V_EP ? 0.4f : kind == V_PAD ? 2.5f : 1.5f;
    float vel = cp.vel[s] * (i ? 0.7f : 0.85f);
    voice_start(kind, cp.note[s][i], vel, SRC_KEYS, -1, atk, rel, (float)i / (float)cp.n[s] * 1.0f - 0.5f);
    if (loop_listening())
        loop_add(cp.note[s][i], vel);
}

/* a PLAY key: its whole chord, strummed upward by STRUM; with FOLLOW the music moves to it */
static void live_chord(int note, int vel)
{
    int s, pc = ((note % 12) + 12) % 12;
    for (s = 0; s < NLIVE; s++)
        if (cp.key[s] == note)
            live_release(s);
    if (!vel)
        return;
    for (s = 0; s < NLIVE && cp.key[s] >= 0; s++)
        ;
    if (s == NLIVE)
        live_release(s = 0);
    cp.kind = cp.type ? cp.type - 1 : key_chord_kind(pc);
    cp.root = pc;
    ms_vis.live_root = (int8_t)pc;
    ms_vis.live_kind = (uint8_t)cp.kind;
    cp.key[s] = (int8_t)note;
    {
        int r = note;                    /* the root by octaves into a range that voices well */
        while (r < 36)
            r += 12;
        while (r > 84)
            r -= 12;
        cp.n[s] = (int8_t)live_voicing(r, cp.kind, cp.note[s]);
    }
    cp.started[s] = 0;
    cp.vel[s] = (float)vel / 127.0f;
    cp.wait[s] = live_gap();
    if (kb.listen && lp.state == LP_PLAY && (lp.quiet > 2.0f * MS_SR || lp.redo >= 0))
        lp.track = loop_phrase_track();
    lp.quiet = 0.0f;
    if (kb.listen && lp.state == LP_EMPTY)
        loop_tap();
    if (kb.listen && lp.state == LP_REC)
        lp.autorec = 1;
    live_start_note(s);
    kb.energy = fm_minf(kb.energy + 0.12f, 1.0f);
    if (cp.follow && hc.lock < 0 && !st.frozen) {
        hm.pending = CH_LIVE;
        if (st.mode != MODE_LOFI)
            seq.chord_t = fm_minf(seq.chord_t, 0.2f);
    }
}

static void evolve_tick(float dt)
{
    int i;
    float rate = dt / (15.0f + 60.0f * (1.0f - evo.pace));   /* how fast the knobs ease to their targets */
    if ((evo.t -= dt) <= 0.0f) {         /* now and then one HOME knob and one MIX level get somewhere new to go */
        evo.t = (20.0f + 140.0f * (1.0f - evo.pace)) * (0.5f + frand());
        evo.target[irand(MS_NMACRO)] = frand2() * 40.0f * evo.amount;
        evo.mtarget[irand(MS_NMIX)] = frand2() * 35.0f * evo.amount;
        evo.busy_target = frand() * 0.6f * evo.beat;
    }
    for (i = 0; i < MS_NMACRO; i++)
        evo.off[i] += (evo.target[i] * (evo.amount > 0.0f) - evo.off[i]) * fm_minf(rate * 4.0f, 1.0f);
    for (i = 0; i < MS_NMIX; i++)
        evo.moff[i] += (evo.mtarget[i] * (evo.amount > 0.0f) - evo.moff[i]) * fm_minf(rate * 4.0f, 1.0f);
    evo.busy += (evo.busy_target * (evo.beat > 0.0f) - evo.busy) * fm_minf(rate * 4.0f, 1.0f);
    if ((evo.apply_t -= dt) <= 0.0f) {   /* the knobs' sound follows once a second, if they moved: apply() is not free */
        int moved = 0;
        evo.apply_t = 1.0f;
        for (i = 0; i < MS_NMACRO + MS_NMIX; i++) {
            int now = (int)(i < MS_NMACRO ? evo.off[i] : evo.moff[i - MS_NMACRO]);
            if (now != evo.applied[i]) {
                evo.applied[i] = (int8_t)now;
                moved = 1;
            }
        }
        if (moved)
            apply();
    }
}

static void live_tick(float dt)
{
    int s;
    float gap = live_gap();
    for (s = 0; s < NLIVE; s++) {
        float d = dt;
        while (cp.key[s] >= 0 && cp.started[s] < cp.n[s] && (cp.wait[s] -= d) <= 0.0f) {
            live_start_note(s);
            cp.wait[s] += gap;
            d = 0.0f;
        }
    }
}

/* smear: keys and loop read backwards in overlapping grains with a little feedback (after Mood's reverse) */
#define SM_N 32768
#define SM_G 16384
static int16_t sm_buf[SM_N] PLAT_POOL;
static struct {
    uint32_t w, start[2], age[2];
    float fb;
} sm;

HOT void smear_tick(float in, float *l, float *r)
{
    float o[2];
    int g;
    sm_buf[sm.w] = to16(in + sm.fb);
    for (g = 0; g < 2; g++) {
        float a = (float)sm.age[g] * (1.0f / (float)SM_G), w = 4.0f * a * (1.0f - a);
        o[g] = (float)sm_buf[(sm.start[g] - sm.age[g]) & (SM_N - 1)] * (1.0f / 32767.0f) * w * w;
        if (++sm.age[g] >= SM_G) {
            sm.age[g] = 0;
            sm.start[g] = sm.w;
        }
    }
    sm.w = (sm.w + 1) & (SM_N - 1);
    *l = o[0] * 0.85f + o[1] * 0.35f;
    *r = o[1] * 0.85f + o[0] * 0.35f;
    sm.fb = (*l + *r) * 0.5f * 0.45f * lp.smear;
}

static void drain(void)
{
    while (cmd_tail != cmd_head) {
        cmd_t c = cmdq[cmd_tail % NCMD];
        cmd_tail++;
        switch (c.type) {
        case C_MODE:
            st.next_mode = c.a;
            st.next_scene = c.b;
            st.switching = 1;
            break;
        case C_MACRO:
            st.macro[c.a & 3] = clamp(c.b, 0, 100);
            apply();
            break;
        case C_MIX:
            st.mix[c.a & 3] = clamp(c.b, 0, 100);
            apply();
            break;
        case C_TEMPO:
            st.tempo = clamp(c.a, 40, 160);
            apply();
            break;
        case C_ROOT:
            st.root = ((c.a % 12) + 12) % 12;
            chord_set(chord_id());
            break;
        case C_MOOD:
            st.mood = clamp(c.a, 0, MS_NMOODS - 1);
            chord_set(chord_id());
            break;
        case C_PLAY:
            st.playing = c.a;
            if (!c.a) {
                voices_release(1);
                bass.env_target = 0.0f;
            } else if (st.mode == MODE_SCAPES) {
                seq.chord_t = 0.0f;
            } else if (st.mode == MODE_DRIFT) {
                if (bt.bass == BASS_AUTO)
                    bass_note(36 + ((st.root + hm.tones[0]) % 12), 0.8f, -1, 1.6f, 2.5f);
            }
            break;
        case C_FREEZE:
            st.frozen = c.a;
            reverb_params(st.frozen);
            break;
        case C_NEW:
            new_variation();
            break;
        case C_NOTE:
            ms_note_now(c.a, c.b);
            break;
        case C_CHORDNOTE:
            live_chord(c.a, c.b);
            break;
        case C_EVOLVE: {
            float v = u(clamp(c.b, 0, 100));
            if (c.a == 0)
                evo.amount = v;
            else if (c.a == 1)
                evo.pace = v;
            else if (c.a == 2)
                evo.chords = v;
            else
                evo.beat = v;
            break;
        }
        case C_PANIC:
            lp.state = LP_EMPTY;
            lp.n = 0;
            for (int i = 0; i < NV; i++)
                vo[i].stage = ST_OFF;
            for (int i = 0; i < NLIVE; i++)
                cp.key[i] = -1;
            ms_vis.live_root = -1;
            bass.env = bass.env_target = 0.0f;
            break;
        case C_SEED:
            rng ^= (uint32_t)(uint16_t)c.a << 16 | (uint16_t)c.b;
            if (!rng)
                rng = 1;
            break;
        case C_KEYSET:
            if (c.a == 0)
                kb.snap = clamp(c.b, 0, 2);
            else if (c.a == 1)
                kb.harmony = u(clamp(c.b, 0, 100));
            else if (c.a == 2)
                kb.voice = clamp(c.b, 0, 4);
            else {
                kb.listen = c.b == 1;
                if (!kb.hold && c.b == 2)
                    loop_close(-1);     /* a key held into HOLD would leave its loop note open */
                if (kb.hold != (c.b == 2))
                    seeds_clear();      /* HOLD starts bare, and leaving it takes its seeds */
                kb.hold = c.b == 2;
            }
            break;
        case C_FX:
            st.fx[c.a & 3] = (c.a & 3) == 1 ? clamp(c.b, 0, 5) : clamp(c.b, 0, 100);
            apply();
            break;
        case C_BEAT:
            if (c.a >= 10 && c.a < 10 + MS_NDRUMS) {
                ub.row[c.a - 10] = (uint16_t)c.b;
                break;
            }
            if (c.a >= 20 && c.a < 20 + MS_NDRUMS) {
                ub.level[c.a - 20] = u(clamp(c.b & 127, 0, 100));
                ub.chance[c.a - 20] = u(clamp(c.b >> 7 & 127, 0, 100));
                break;
            }
            if (c.a == 46) {
                ub.len = c.b == 12 ? 12 : 16;
                break;
            }
            if (c.a >= 30 && c.a < 46) {
                ub.bass[c.a - 30] = (uint8_t)clamp(c.b, 0, MS_NBN - 1);
                break;
            }
            if (c.a == 8) {              /* hear one drum now */
                static const uint8_t EV[MS_NDRUMS] = {EV_KICK, EV_SNARE, EV_GHOST, EV_HAT, EV_OHAT, EV_PERC};
                int d = clamp(c.b, 0, MS_NDRUMS - 1);
                ev_add(0, EV[d], 0, 0, d == MS_DRUM_GHOST ? 0.35f : 0.85f, 1, 0, 0.0f);
                break;
            }
            if (c.a == 9) {              /* hear one bass note on the chord now */
                static const int8_t TONE[MS_NBN] = {0, 0, 1, 2, 3, 0, 0};
                int b = clamp(c.b, 1, MS_NBN - 1), root = 36 + (st.root + hm.tones[0]) % 12;
                int n = b == MS_BN_STEP ? root - 1 : 36 + (st.root + hm.tones[TONE[b]]) % 12 + (b == MS_BN_OCT ? 12 : 0);
                if (n < root && b != MS_BN_STEP)
                    n += 12;
                bass_note(n, 0.8f, (int32_t)(60.0f / (float)st.tempo * MS_SR), bt.bass_glide, 0.004f);
                break;
            }
            if (c.a >= 4 && c.a <= 7) {
                int v = clamp(c.b, 0, 100);
                if (c.a == 4)
                    bt.swing = v ? 0.04f * (float)(clamp(v, 1, 9) - 1) : -1.0f;
                else if (c.a == 5)
                    bass.fm = 0.035f + 0.9f * u(v) * u(v);
                else if (c.a == 6)
                    bt.bass_glide = 0.12f * u(v) * u(v);
                else
                    bt.bass_len = (float)clamp(v, 1, 8);
                break;
            }
            if (c.a == 0)
                bt.kit = clamp(c.b, 0, KIT_N - 1);
            else if (c.a == 1)
                bt.pattern = clamp(c.b, 0, PAT_N - 1);
            else if (c.a == 2)
                bt.level = lvl(clamp(c.b, 0, 100));
            else
                bt.bass = clamp(c.b, 0, BASS_N - 1);
            if (c.a == 3 && (bt.bass == BASS_OFF || (bt.bass >= BASS_ROOT && bass.gate < 0)))
                bass.env_target = 0.0f;      /* and AUTO's held drone gives way to the steps */
            if (c.a == 3 && bt.bass == BASS_AUTO && st.mode == MODE_DRIFT && st.playing)
                bass_note(36 + ((st.root + hm.tones[0]) % 12), 0.8f, -1, 1.6f, 2.5f);
            if (c.a == 0 && bt.kit == KIT_OFF) {
                dr.k_amp = dr.s_namp = dr.s_tamp = dr.h_amp = dr.p_amp = dr.bus_lp = 0.0f;
                dk_l += ms.drum_l;
                dk_r += ms.drum_r;
                ms.drum_l = ms.drum_r = 0.0f;
            }
            break;
        case C_SPACE:
            st.space = clamp(c.a, 0, SP_N - 1);
            apply();
            break;
        case C_LOOP:
            if (c.a == 2) {              /* REC: keep it. A recording closes; what you add is a layer now */
                int add = lp.state == LP_PLAY && lp.keep && !kb.listen && !kb.hold;   /* by hand: kept, then a layer */
                if (lp.state == LP_EMPTY || lp.state == LP_REC || lp.state == LP_DUB || add)
                    loop_tap();
                if (lp.state == LP_PLAY) {
                    loop_close(-1);
                    lp.keep = 1;
                    lp.adding = 0;
                    lp.quiet = 3.0f * MS_SR;   /* the next note starts a new layer, as after a pause */
                }
            } else if (c.a) {
                lp.state = LP_EMPTY;
                lp.n = 0;
                lp.autorec = 0;
                lp.mute = 0;
                lp.keep = 0;
            } else if (lp.state == LP_PLAY) {
                lp.keep = !lp.keep;
            } else {
                loop_tap();                  /* empty: record by hand; recording: close it; adding: done */
            }
            ms_vis.lp_keep = (uint8_t)lp.keep;
            break;
        case C_SNAP:
            loop_snap();
            __asm__ volatile("" ::: "memory");   /* the slot is whole before the UI is told */
            ms_vis.xfer_seq++;
            break;
        case C_LOAD:
            loop_load();
            __asm__ volatile("" ::: "memory");
            ms_vis.xfer_seq++;
            break;
        case C_TRACK: {
            int t = c.a % MS_NLAYERS, i, kept = 0;
            if (c.b == -6 || c.b == -7) {
                lp.redo = c.b == -6 ? t : -1;
            } else if (c.b >= 0) {
                lp.tgain[t] = lvl(clamp(c.b, 0, 100));
            } else if (c.b == -1) {
                for (i = 0; i < lp.n; i++)
                    if (lp.ev[i].track == t)
                        break;
                if (i < lp.n)
                    lp.mute |= 1u << t;
            } else if (c.b == -3) {
                lp.mute &= ~(1u << t);
            } else if (c.b == -4 || c.b == -5) {
                float d = c.b == -4 ? sixteenth_len() : -sixteenth_len();
                for (i = 0; i < lp.n; i++)
                    if (lp.ev[i].track == t && lp.len > 0.0f) {
                        float x = lp.ev[i].t + d;
                        lp.ev[i].t = x - fm_floorf(x / lp.len) * lp.len;
                    }
            } else {
                for (i = 0; i < lp.n; i++)
                    if (lp.ev[i].track != t)
                        lp.ev[kept++] = lp.ev[i];
                lp.n = kept;
                lp.mute &= ~(1u << t);
                if (!lp.n && lp.state != LP_REC)
                    lp.state = LP_EMPTY;
            }
            break;
        }
        case C_CHORDS:
            if (c.a == 0) {
                hc.lock = c.b > 0 ? clamp(c.b - 1, 0, 6) : -1;
                if (hc.lock >= 0 && st.mode != MODE_LOFI && seq.chord_t > 1.0f)
                    seq.chord_t = 0.5f + 0.5f * frand();   /* soon, not on the spot: LOFI waits for its next bar */
            } else if (c.a == 1) {
                int was = hc.rate;
                hc.rate = clamp(c.b, 0, 3);
                if (st.mode != MODE_LOFI && hc.rate && (was == 0 || seq.chord_t > 60.0f))
                    seq.chord_t = 4.0f + 4.0f * frand();
                else if (st.mode != MODE_LOFI && !hc.rate)
                    seq.chord_t = 1e9f;
            } else if (c.a == 2) {
                hc.path = clamp(c.b, 0, PATH_BUILT);
                if (hc.path != PATH_BUILT && hm.pending >= CH_BUILT && hm.pending < CH_LIVE)
                    hm.pending = -1;
            } else if (c.a == 4) {       /* MINE: n << 12 | four degrees of 3 bits; the newest is heard soon */
                int n = clamp(c.b >> 12 & 7, 1, 4), j;
                for (j = 0; j < 4; j++)
                    hc.mine[j] = (int8_t)clamp(c.b >> (3 * j) & 7, 0, 6);
                hc.mine_n = n;
                if (hc.path == PATH_MINE && hc.lock < 0) {
                    hm.prog_i = n - 1;
                    hm.pending = hc.mine[n - 1];
                    if (st.mode != MODE_LOFI)
                        seq.chord_t = fm_minf(seq.chord_t, 0.3f + 0.3f * frand());
                }
            } else if (c.a == 5) {
                int j = c.b >> 11 & 7;
                hc.b_root[j] = (uint8_t)((c.b & 15) % 12);
                hc.b_kind[j] = (uint8_t)clamp(c.b >> 4 & 31, 0, MS_NCHORDS - 1);
                hc.b_len[j] = (uint8_t)(c.b >> 9 & 3);
                if (hm.slot == j)
                    chord_set(CH_BUILT + j);
            } else if (c.a == 7) {
                cp.type = clamp(c.b, 0, MS_NCHORDS);
            } else if (c.a == 8) {
                cp.voicing = clamp(c.b, 0, 3);
            } else if (c.a == 9) {
                cp.strum = u(clamp(c.b, 0, 100));
            } else if (c.a == 10) {
                cp.follow = c.b != 0;
            } else if (c.a == 6) {       /* BUILT's count, the chord just made (heard soon), the chord removed */
                int hear = (c.b >> 4 & 15) - 1, gone = (c.b >> 8 & 15) - 1;
                hc.b_n = clamp(c.b & 15, 1, MS_NBUILT);
                if (gone >= 0 && hm.slot > gone) {
                    hm.slot--;
                    hm.prog_i--;
                }
                if (hm.slot >= hc.b_n)
                    hm.slot = hc.b_n - 1;
                if (hm.prog_i >= hc.b_n)
                    hm.prog_i = hc.b_n - 1;
                if (hm.slot >= 0)
                    ms_vis.chord_slot = (int8_t)hm.slot;
                if (hear >= 0 && hear < hc.b_n && hc.path == PATH_BUILT && hc.lock < 0) {
                    hm.prog_i = hear;
                    hm.pending = CH_BUILT + hear;
                    if (st.mode != MODE_LOFI)
                        seq.chord_t = fm_minf(seq.chord_t, 0.3f + 0.3f * frand());
                }
            } else {
                hc.color = clamp(c.b, 0, 3);
                chord_set(chord_id());
            }
            break;
        case C_MOTION:
            motion_set(c.a & 3, c.b);
            break;
        case C_LOOPFX:
            lfx_set(c.a & 3, clamp(c.b, 0, 100));
            break;
        case C_LOOPSET:
            if (c.a == 0)
                lp.fade = u(clamp(c.b, 0, 100));
            else if (c.a == 1)
                lp.mutate = u(clamp(c.b, 0, 100));
            else if (c.a == 2)
                lp.smear = u(clamp(c.b, 0, 100));
            else
                lp.speed = clamp(c.b, 0, 4);
            break;
        }
    }
}

static void fire_events(void)
{
    int i;
    for (i = 0; i < NEVT; i++) {
        evt_t *e = &evq[i];
        if (!ev_used[i])
            continue;
        e->t -= CTL;
        if (e->t > 0)
            continue;
        ev_used[i] = 0;
        if (!st.playing && e->layer < SRC_KEYS)
            continue;
        if (e->type != EV_NOTE && bt.kit == KIT_OFF)   /* hits queued before KIT went OFF */
            continue;
        switch (e->type) {
        case EV_NOTE:
            voice_start(e->kind, e->note, e->vel, e->layer, e->gate, e->kind == V_EP ? 0.003f : 0.002f,
                        e->kind == V_EP ? 0.35f : 2.0f, e->pan);
            break;
        case EV_KICK:
            drum_kick(e->vel);
            break;
        case EV_SNARE:
        case EV_GHOST:
            drum_snare(e->vel);
            break;
        case EV_HAT:
            drum_hat(e->vel, 0);
            break;
        case EV_OHAT:
            drum_hat(e->vel, 1);
            break;
        case EV_PERC:
            drum_perc(e->vel, e->note ? 1.5f : 1.0f);
            break;
        }
    }
}

static void gates(void)
{
    int i;
    for (i = 0; i < NV; i++)
        if (vo[i].gate > 0 && (vo[i].stage == ST_ATK || vo[i].stage == ST_DEC)) {
            vo[i].gate -= CTL;
            if (vo[i].gate <= 0)
                vo[i].stage = ST_REL;
        }
    if (bass.gate > 0) {
        bass.gate -= CTL;
        if (bass.gate <= 0) {
            bass.env_target = 0.0f;
            bass.env_rate = onepole_a(1.0f / (FM_TWO_PI * (st.mode == MODE_LOFI || bt.bass >= BASS_ROOT ? 0.05f : 2.0f)));
        }
    }
}

static void nature_control(void)
{
    const float dt = (float)CTL / MS_SR;
    float p;
    int i;
    if ((na.gust_t -= CTL) <= 0) {
        na.gust_t = (int32_t)((1.5f + 5.0f * frand()) * MS_SR);
        na.gust_target = fm_clampf(na.gust_target + 0.6f * frand2(), 0.0f, 1.0f);
    }
    na.gust += 0.0015f * (na.gust_target - na.gust);
    ms_vis.gust = na.gust * k.wind;
    if (k.wind > 0.0f) {
        svf_set(&na.wl, 250.0f + 900.0f * na.gust * (0.5f + k.wind), 1.6f + 1.5f * na.gust);
        svf_set(&na.wr, 290.0f + 1000.0f * na.gust * (0.5f + k.wind), 1.6f + 1.5f * na.gust);
    }
    ms_vis.rain = k.rain;
    if (k.rain > 0.0f) {
        float rate = 25.0f + 260.0f * k.rain * k.rain;
        if (frand() < rate * dt)
            blip(1800.0f + 3800.0f * frand(), 0.6f, (0.02f + 0.07f * frand() * frand()) * (k.rain_tone < 3000.0f ? 0.4f : 1.0f),
                 0.004f + 0.006f * frand(), frand2());
        if (frand() < rate * 0.08f * dt)
            blip(600.0f + 700.0f * frand(), 1.2f, 0.06f * (k.rain_tone < 3000.0f ? 0.4f : 1.0f), 0.015f, frand2());
    }
    if (k.surf <= 0.0f)
        ms_vis.wave = 0.0f;
    if (k.fire <= 0.0f)
        ms_vis.fire = 0.0f;
    /* surf: swell until 0.55 of the period, then the break and the wash running back */
    if (k.surf > 0.0f) {
        float rise, ph;
        na.surf_ph += dt / na.surf_period;
        if (na.surf_ph >= 1.0f) {
            na.surf_ph -= 1.0f;
            na.surf_period = 7.0f + 6.0f * frand();
        }
        ph = na.surf_ph;
        if (ph < 0.55f) {
            rise = ph / 0.55f;
            na.surf_env = 0.25f + 0.75f * rise * rise;
            na.surf_bright = 0.2f + 0.5f * rise * rise * rise;
            na.surf_wash *= 0.995f;
        } else {
            rise = (ph - 0.55f) / 0.45f;
            na.surf_env = 0.25f + 0.75f * (1.0f - rise) * (1.0f - rise);
            na.surf_bright = 0.25f + 0.75f * (1.0f - rise) * (1.0f - rise);
            if (ph < 0.58f)
                na.surf_wash = 1.0f;
            na.surf_wash *= 0.9993f;
        }
        svf_set(&na.sl, 150.0f + 2600.0f * na.surf_bright * k.surf, 0.6f);
        svf_set(&na.sr, 170.0f + 2400.0f * na.surf_bright * k.surf, 0.6f);
        ms_vis.wave = na.surf_env;
    }
    if (k.creek > 0.0f && frand() < (12.0f + 50.0f * k.creek) * dt)
        blip(350.0f + 900.0f * frand(), 1.5f + 2.5f * frand(), 0.04f + 0.05f * frand(), 0.012f + 0.025f * frand(),
             frand2() * 0.8f);
    if (k.fire > 0.0f) {
        if ((na.fire_flick_t -= dt) <= 0.0f) {
            na.fire_flick_t = 0.05f + 0.3f * frand();
            na.fire_flick = 0.4f + 0.6f * frand();
        }
        p = (3.0f + 18.0f * k.fire) * dt * (na.burst > 0 ? 8.0f : 1.0f);
        if (na.burst > 0)
            na.burst -= CTL;
        else if (frand() < 0.15f * dt * k.fire * 4.0f)
            na.burst = (int32_t)((0.1f + 0.4f * frand()) * MS_SR);
        if (frand() < p) {
            na.crackle = (0.1f + 0.3f * frand() * frand()) * k.fire;
            na.crackle_dec = decay_mul(0.0015f + 0.004f * frand());
            na.crackle_pan = frand2() * 0.7f;
        }
        ms_vis.fire = na.fire_flick * k.fire;
    }
    if (k.thunder_rate > 0.0f && !th.active && frand() < k.thunder_rate / 60.0f * dt)
        thunder_strike();
    if (th.active) {
        if (th.delay > 0) {
            if ((th.delay -= CTL) <= 0) {
                th.target = 0.5f + 0.5f * th.near;
                th.crack = th.near > 0.6f ? 0.1f * th.near : 0.0f;
            }
        } else {
            th.target *= th.dec;
            if (th.swells > 0 && frand() < 0.8f * dt) {
                th.target = fm_minf(th.target + 0.3f + 0.3f * frand(), 1.0f);
                th.swells--;
            }
            if (th.target < 0.002f && th.env < 0.002f)
                th.active = 0;
        }
    }
    for (i = 0; i < NBIRD; i++)
        if (bd[i].species != BIRD_NONE && !bd[i].syl_left && st.playing && k.bird_rate > 0.01f) {
            bd[i].wait -= CTL;
            if (bd[i].wait <= 0)
                bird_song(&bd[i]);
        }
}

static void control(void)
{
    const float dt = (float)CTL / MS_SR;
    int j;
    drain();
    for (j = 0; j < MS_NMIX; j++)        /* ~10 ms: a MIX change, or EVOLVE's, glides */
        st.layer[j] += 0.08f * (st.layer_to[j] - st.layer[j]);
    if (st.switching) {
        st.gen_fade -= dt / 0.6f;
        if (st.gen_fade <= 0.0f) {
            st.gen_fade = 0.0f;
            st.switching = 0;
            st.mode = clamp(st.next_mode, 0, MS_NMODES - 1);
            st.scene = clamp(st.next_scene, 0, MS_NSCENES[st.mode] - 1);
            mode_start();
        }
    } else {
        float target = st.playing ? 1.0f : 0.0f;
        st.gen_fade += fm_clampf(target - st.gen_fade, -dt / 2.5f, dt / 1.5f);
    }
    ms.sleep += 0.001f * (st.sleep_target - ms.sleep);
    fire_events();
    gates();
    loop_tick();
    seeds_tick(dt);
    live_tick(dt);
    evolve_tick(dt);
    motion_tick(dt);
    kb.energy *= 1.0f - dt / 20.0f;
    if (st.playing && !st.switching) {
        seq.acc -= (float)CTL;
        while (seq.acc <= 0.0f) {
            if (bt.kit != KIT_OFF)
                beat_step();
            if (bt.bass == BASS_ROOT || bt.bass == BASS_WALK || bt.bass == BASS_MINE)
                backing_bass_step();
            if (st.mode == MODE_LOFI)
                lofi_step();
            else if (st.mode == MODE_DRIFT && seq.step % DRIFT_STYLE[st.scene].step == 0u)
                drift_step();
            seq.acc += step_len();
            seq.step++;
            ms_vis.step = seq.step;
        }
        if (st.mode != MODE_LOFI && !st.frozen) {
            seq.chord_t -= dt;
            if (seq.chord_t <= 0.0f) {
                if (st.mode == MODE_SCAPES)
                    scapes_next_chord();
                else
                    drift_next_chord();
            }
        }
        if (st.mode == MODE_SCAPES && k.note_rate > 0.0f && st.mix[3]) {
            seq.note_acc -= dt;
            if (seq.note_acc <= 0.0f) {
                scapes_note();
                seq.note_acc = 60.0f / k.note_rate * (0.4f + 1.2f * frand()) / (1.0f + 2.0f * kb.energy);
            }
        }
        if (st.mode == MODE_DRIFT)
            drift_pads(dt);
    }
    nature_control();
    if (st.mode == MODE_DRIFT)
        for (int i = 0; i < NLOOP; i++)
            ms_vis.loop_pos[i] = ((float)dg.pos[i] + (float)(seq.step % DRIFT_STYLE[st.scene].step) / (float)DRIFT_STYLE[st.scene].step)
                               / (float)dg.len[i];
    ms_vis.playing = (uint8_t)st.playing;
    ms_vis.frozen = (uint8_t)st.frozen;
}

HOT float wob_read(float *buf, float delay)
{
    float pos = (float)wob_w - delay, fr;
    int32_t i0;
    if (pos < 0.0f)
        pos += (float)WOB_N;
    i0 = (int32_t)pos;
    fr = pos - (float)i0;
    i0 &= WOB_N - 1;
    return buf[i0] + fr * (buf[(i0 + 1) & (WOB_N - 1)] - buf[i0]);
}

PLAT_RAM_CODE static void render_block(float *out_l, float *out_r, uint32_t n)
{
    uint32_t i;
    int j;
    float gf = st.gen_fade, wob_target = h_maxf(h_maxf(st.mode == MODE_LOFI ? k.wobble : 0.0f, 0.75f * k.film), k.wob_add);
    float wob_depth;
    float lg[6], dust = st.mode == MODE_LOFI ? k.dust : 0.0f, noise = h_maxf(dust, 0.6f * k.film) * st.gen_fade;
    float crush_bits = k.crush_bits;
    float rain_gain = st.mode == MODE_LOFI ? st.layer[3] : st.layer[0];
    float wind_gain = st.mode == MODE_DRIFT ? st.layer[3] * 0.6f : st.layer[0];
    float bed_gain = st.layer[0], life_gain = st.mode == MODE_DRIFT ? st.layer[3] * 0.7f : st.layer[1];
    float bass_gain = bt.bass == BASS_OFF ? 0.0f
                    : bt.bass >= BASS_ROOT ? bt.level * (st.mode == MODE_LOFI ? st.layer[2] : 0.8f)
                    : st.mode == MODE_DRIFT ? st.layer[2] * 0.7f : st.layer[2];
    ms.crush_n = dust > 0.55f ? 1 + (int)((dust - 0.55f) * 6.0f) : 1;
    for (j = 0; j < 6; j++)
        lg[j] = j >= SRC_KEYS ? st.layer[j] * (st.mode == MODE_LOFI ? 1.6f : 1.0f) : st.layer[j] * gf;   /* LOFI: over the beat */
    if (st.mode == MODE_DRIFT)
        lg[2] = 0.0f;                    /* DRIFT's layer 2 is the bass, outside the voice pool */
    for (i = 0; i < n; i++) {
        float nl = frand2(), nr = frand2(), l = 0.0f, r = 0.0f, rs_l = 0.0f, rs_r = 0.0f, es = 0.0f, s, x, y;
        float bl_l = 0.0f, bl_r = 0.0f, kbus = 0.0f, lb_l = 0.0f, lb_r = 0.0f;
        for (j = 0; j < NV; j++) {
            voice_t *v = &vo[j];
            float g, send = 1.0f;
            int loop = v->layer == SRC_LOOP;
            if (v->stage == ST_OFF)
                continue;
            s = voice_tick(v, loop ? lfx.wmul : 1.0f);
            g = lg[v->layer];
            s *= g;
            if (v->layer >= SRC_KEYS)
                kbus += s;
            v->out_l = s * v->pl;
            v->out_r = s * v->pr;
            if (loop) {                      /* the loop has its own tape, below */
                lb_l += v->out_l;
                lb_r += v->out_r;
                send = lfx.send;
            } else {
                l += v->out_l;
                r += v->out_r;
            }
            rs_l += s * v->pl * v->rev * send;
            rs_r += s * v->pr * v->rev * send;
            es += s * v->echo * send * (v->layer >= SRC_KEYS ? 2.5f : 1.0f);   /* what you play is what the delay is for */
        }
        lb_l += dkl_l;
        lb_r += dkl_r;
        dkl_l = h_flush(dkl_l * 0.993f);
        dkl_r = h_flush(dkl_r * 0.993f);
        if (lb_l != 0.0f || lb_r != 0.0f || lfx.lp_l != 0.0f) {   /* the loop's tape: aged, then driven */
            lfx.lp_l += lfx.age_a * (lb_l - lfx.lp_l);
            lfx.lp_r += lfx.age_a * (lb_r - lfx.lp_r);
            lfx.lp_l = h_flush(lfx.lp_l);
            lfx.lp_r = h_flush(lfx.lp_r);
            lfx.hp_l = h_flush(lfx.hp_l + lfx.age_hp * (lfx.lp_l - lfx.hp_l));
            lfx.hp_r = h_flush(lfx.hp_r + lfx.age_hp * (lfx.lp_r - lfx.hp_r));
            x = lfx.lp_l - lfx.hp_l;
            y = lfx.lp_r - lfx.hp_r;
            s = 0.5f * (x + y);
            x = (s + lfx.width * (x - s)) * lfx.wear;
            y = (s + lfx.width * (y - s)) * lfx.wear;
            l += lfx.dirt > 0.0f ? soft_tanh(x * lfx.drive) * lfx.makeup : x;
            r += lfx.dirt > 0.0f ? soft_tanh(y * lfx.drive) * lfx.makeup : y;
        }
        s = bass_tick() * bass_gain * gf;
        l += s;
        r += s;
        rs_l += 0.1f * s;
        rs_r += 0.1f * s;
        if (bt.kit != KIT_OFF) {
            float dg = bt.level * gf * (st.mode == MODE_LOFI ? st.layer[1] : 1.0f);
            drums_tick(nl, &x, &y);
            x *= dg;
            y *= dg;
            ms.drum_l = x;
            ms.drum_r = y;
            l += x;
            r += y;
            rs_l += 0.12f * x;
            rs_r += 0.12f * y;
        }
        for (j = 0; j < NBLIP; j++) {
            blip_t *b = &bl[j];
            if (b->amp < 0.0002f)
                continue;
            b->inc *= b->inc_mul;
            b->ph = wrap1(b->ph + b->inc);
            s = sin_t(b->ph) * b->amp;
            b->amp *= b->dec;
            bl_l += s * b->pl;
            bl_r += s * b->pr;
        }
        {
            float bg = 0.9f * (k.rain > 0.0f ? rain_gain : bed_gain) * gf;
            l += bl_l * bg;
            r += bl_r * bg;
            rs_l += 0.3f * bl_l * bg;
            rs_r += 0.3f * bl_r * bg;
        }
        if (k.wind > 0.0f) {
            float a = (0.075f + 0.35f * na.gust) * k.wind * wind_gain * gf;
            x = svf_bp(&na.wl, nl) * a;
            y = svf_bp(&na.wr, nr) * a;
            l += x;
            r += y;
            rs_l += 0.2f * x;
            rs_r += 0.2f * y;
        }
        if (k.rain > 0.0f) {
            float a = 0.09f * k.rain * rain_gain * gf;
            na.rain_hp_l += 0.12f * (nl - na.rain_hp_l);
            na.rain_hp_r += 0.12f * (nr - na.rain_hp_r);
            x = nl - na.rain_hp_l;
            y = nr - na.rain_hp_r;
            na.rain_lp_l += (k.rain_tone < 3000.0f ? 0.2f : 0.55f) * (x - na.rain_lp_l);
            na.rain_lp_r += (k.rain_tone < 3000.0f ? 0.2f : 0.55f) * (y - na.rain_lp_r);
            l += na.rain_lp_l * a;
            r += na.rain_lp_r * a;
        }
        if (k.surf > 0.0f) {
            float a = 0.5f * na.surf_env * k.surf * bed_gain * gf, w = 0.075f * na.surf_wash * k.surf * bed_gain * gf;
            x = svf_lp(&na.sl, nl) * a;
            y = svf_lp(&na.sr, nr) * a;
            na.wash_lp_l += 0.5f * (nl - na.wash_lp_l);
            na.wash_lp_r += 0.5f * (nr - na.wash_lp_r);
            x += (nl - na.wash_lp_l) * w;
            y += (nr - na.wash_lp_r) * w;
            l += x;
            r += y;
            rs_l += 0.15f * x;
            rs_r += 0.15f * y;
        }
        if (k.creek > 0.0f) {
            float a = 0.17f * k.creek * bed_gain * gf;
            l += svf_bp(&na.creek_l, nl) * a;
            r += svf_bp(&na.creek_r, nr) * a;
        }
        if (k.fire > 0.0f) {
            float a = 2.0f * k.fire * bed_gain * gf;
            na.fire_rum_l += 0.02f * (nl - na.fire_rum_l);
            na.fire_rum_r += 0.02f * (nr - na.fire_rum_r);
            na.fire_rum2_l += 0.02f * (na.fire_rum_l - na.fire_rum2_l);
            na.fire_rum2_r += 0.02f * (na.fire_rum_r - na.fire_rum2_r);
            x = na.fire_rum2_l * 1.6f * (0.5f + 0.5f * na.fire_flick) * a;
            y = na.fire_rum2_r * 1.6f * (0.5f + 0.5f * na.fire_flick) * a;
            if (na.crackle > 0.0005f) {
                na.fire_hp += 0.5f * (nl - na.fire_hp);
                s = (nl - na.fire_hp) * na.crackle * a;
                na.crackle *= na.crackle_dec;
                x += s * (1.0f - na.crackle_pan);
                y += s * (1.0f + na.crackle_pan);
            }
            l += x;
            r += y;
            rs_l += 0.2f * x;
            rs_r += 0.2f * y;
        }
        if (th.active && th.delay <= 0) {
            float a = bed_gain * gf;
            th.env += th.att * (th.target - th.env);
            x = svf_lp(&th.lpl, nl) * th.env * 1.4f * a;
            y = svf_lp(&th.lpr, nr) * th.env * 1.4f * a;
            if (th.crack > 0.0005f) {
                th.cl += 0.3f * (nl - th.cl);   /* dulled: a crack, not a burst of static */
                th.cr += 0.3f * (nr - th.cr);
                x += th.cl * th.crack * a * 1.6f;
                y += th.cr * th.crack * a * 1.6f;
                th.crack *= 0.9994f;
            }
            l += x;
            r += y;
            rs_l += 0.3f * x;
            rs_r += 0.3f * y;
        }
        if (st.mode == MODE_SCAPES || bd[0].species != BIRD_NONE) {   /* DRIFT's garden has birds too, on AIR */
            x = y = 0.0f;
            for (j = 0; j < NBIRD; j++)
                bird_tick(&bd[j], &x, &y);
            for (j = 0; j < n_crickets; j++)
                cricket_tick(&cr[j], k.life, &x, &y);
            x *= 2.2f * life_gain * gf;
            y *= 2.2f * life_gain * gf;
            l += x;
            r += y;
            rs_l += 0.45f * x;
            rs_r += 0.45f * y;
        }
        if (kbus != 0.0f || k.reverse > 0.0f)
            ms.sm_quiet = 0;
        else if (ms.sm_quiet < SM_N)
            ms.sm_quiet++;
        if (ms.sm_quiet < SM_N && (lp.smear > 0.0f || k.reverse > 0.0f)) {
            float sm_mix = h_maxf(lp.smear, k.reverse);
            smear_tick(kbus + k.reverse * 0.5f * (rs_l + rs_r), &x, &y);
            x *= sm_mix;
            y *= sm_mix;
            l += 0.8f * x;
            r += 0.8f * y;
            rs_l += x;
            rs_r += y;
        }
        echo_tick(es * k.echo_mix * 2.0f, &x, &y);
        l += x;
        r += y;
        rs_l += k.echo_to_rev * x;
        rs_r += k.echo_to_rev * y;
        /* shimmer feeds the reverb input, never its output, so there is no loop to run away */
        s = k.shimmer > 0.0f && !st.frozen ? shimmer_tick(0.5f * (rs_l + rs_r) * k.rev_mix) * k.shimmer * 2.0f : 0.0f;
        reverb_tick(rs_l * (k.rev_mix + mo.rev_boost) + s, rs_r * (k.rev_mix + mo.rev_boost) + s, st.frozen, &x, &y);
        ms.rev_last = 0.5f * (x + y);
        l += x * 0.7f;
        r += y * 0.7f;
        if (k.radio > 0.0f || ms.radio_s > 0.0005f) {   /* RADIO: eased in and out like the chorus */
            float m = soft_tanh(1.1f * (l + r)) * 0.55f;
            ms.radio_s += 0.0005f * (k.radio - ms.radio_s);
            ms.hp_l += k.hp_a * (m - ms.hp_l);
            m -= ms.hp_l;
            l += ms.radio_s * (m - l);
            r += ms.radio_s * (m - r);
        }
        l = svf_lp(&ms.tone_l, l);
        r = svf_lp(&ms.tone_r, r);
        /* tape wow, flutter and drift: the whole mix through a moving read head */
        wob_l[wob_w] = l;
        wob_r[wob_w] = r;
        {                                /* always through the head, so turning the wobble on never jumps */
            float d;
            ms.wob_s += 0.0005f * (wob_target - ms.wob_s);
            wob_depth = ms.wob_s;
            wob_drift += 0.0002f * (wob_drift_to - wob_drift);
            ms_wob_wf_s += 0.02f * (ms.wob_wf - ms_wob_wf_s);   /* wow and flutter move a block at a time: eased */
            d = 220.0f + wob_depth * (ms_wob_wf_s + 60.0f * wob_drift);
            l = wob_read(wob_l, d);
            r = wob_read(wob_r, d + 3.0f);
            ms.ch_s += 0.0005f * (k.chorus - ms.ch_s);
            if (ms.ch_s > 0.001f) {      /* a second, swaying head beside it: chorus */
                float c = ms.ch_s;
                ms.ch_ph = wrap1(ms.ch_ph + k.chorus_rate * (1.0f / MS_SR));
                x = wob_read(wob_l, d + 150.0f + 250.0f * c * (0.5f + 0.5f * sin_t(ms.ch_ph)));
                y = wob_read(wob_r, d + 150.0f + 250.0f * c * (0.5f + 0.5f * sin_t(ms.ch_ph + 0.25f)));
                l = l * (1.0f - 0.35f * c) + 0.65f * c * x;
                r = r * (1.0f - 0.35f * c) + 0.65f * c * y;
            }
        }
        wob_w = (wob_w + 1) & (WOB_N - 1);
        ms.tr_s += 0.0005f * (k.trem - ms.tr_s);
        if (ms.tr_s > 0.001f) {          /* PULSE: eighth notes, one side then the other */
            float t;
            ms.tr_ph = wrap1(ms.tr_ph + (float)st.tempo * (2.0f / 60.0f / MS_SR));
            t = sin_t(ms.tr_ph);
            l *= 1.0f - ms.tr_s * (0.5f + 0.5f * t);
            r *= 1.0f - ms.tr_s * (0.5f - 0.5f * t);
        }
        if (crush_bits > 0.0f) {
            if (++ms.crush_i >= ms.crush_n) {
                ms.crush_i = 0;
                ms.crush_hold_l = (float)(int32_t)(l * crush_bits) / crush_bits;
                ms.crush_hold_r = (float)(int32_t)(r * crush_bits) / crush_bits;
            }
            l = ms.crush_hold_l;
            r = ms.crush_hold_r;
        }
        if (noise > 0.0f) {
            ms.hiss_lp += 0.3f * (nr - ms.hiss_lp);
            s = ms.hiss_lp * 0.006f * noise;
            if (ms.crackle > 0.0005f) {
                x = nl * ms.crackle;
                ms.crackle *= ms.crackle_dec;
                l += x * (1.0f - ms.crackle_pan);
                r += x * (1.0f + ms.crackle_pan);
            }
            l += s;
            r += s;
        }
        out_l[i] = l + dk_l;
        out_r[i] = r + dk_r;
        dk_l *= 0.993f;                  /* ~3 ms */
        dk_r *= 0.993f;
    }
}

static float bl_l_tmp[CTL], bl_r_tmp[CTL];

void ms_render(int32_t *lr, uint32_t n, uint32_t gain_q12)
{
    uint32_t done = 0, i, m;
    float gain = (float)gain_q12 * (1.0f / 4096.0f) * 8300000.0f, peak = 0.0f;
    while (done < n) {
        m = n - done < CTL ? n - done : CTL;
        control();
        wow_ph = wrap1(wow_ph + 0.45f * (float)m / MS_SR);
        flutter_ph = wrap1(flutter_ph + 5.7f * (float)m / MS_SR);
        ms.wob_wf = 90.0f * sin_t(wow_ph) + 9.0f * sin_t(flutter_ph);
        rv.mod_ph = wrap1(rv.mod_ph + 0.37f * (float)m / MS_SR);
        rv.mod_m = sin_t(rv.mod_ph);
        if ((wob_drift_t -= (float)m / MS_SR) <= 0.0f) {
            wob_drift_t = 0.5f + 1.5f * frand();
            wob_drift_to = frand2();      /* a new place to drift to; the head glides there */
        }
        {
            float noise = fm_maxf(st.mode == MODE_LOFI ? k.dust : 0.0f, 0.6f * k.film) * st.gen_fade;
#ifdef MS_TEST_NO_CRACKLE                    /* the pops test hears every other corner */
            noise = 0.0f;
#endif
            if (noise > 0.0f && frand() < (1.0f + 14.0f * noise) * (float)m / MS_SR) {
            ms.crackle = (0.02f + 0.12f * frand() * frand()) * noise;
            ms.crackle_dec = decay_mul(0.0004f + 0.0012f * frand());
            ms.crackle_pan = frand2() * 0.6f;
            }
        }
        /* FILM: brief tape dropouts */
        if (ms.drop_t > 0.0f)
            ms.drop_t -= (float)m / MS_SR;
        else if (k.film > 0.45f && frand() < (k.film - 0.45f) * 0.25f * (float)m / MS_SR)
            ms.drop_t = 0.08f + 0.22f * frand();
        ms_vis.tape_ph += (float)m / MS_SR * 0.35f * (st.playing ? 1.0f : 0.0f);
        lfx.wph = wrap1(lfx.wph + 0.7f * (float)m / MS_SR);
        lfx.wph2 = wrap1(lfx.wph2 + 0.23f * (float)m / MS_SR);
        lfx.wmul = 1.0f + lfx.warble * (0.012f * sin_t(lfx.wph) + 0.008f * sin_t(lfx.wph2));
        {
            static const float MODE_GAIN[MS_NMODES] = {0.85f, 1.5f, 1.15f};   /* level-match the modes */
            ms.mode_gain += 0.01f * (MODE_GAIN[st.mode] - ms.mode_gain);
        }
        PLAT_FAR(render_block)(bl_l_tmp, bl_r_tmp, m);
        for (i = 0; i < m; i++) {
            float l = bl_l_tmp[i] * ms.mode_gain, r = bl_r_tmp[i] * ms.mode_gain, a;
            if (!(l > -8.0f && l < 8.0f))  /* drop NaN or a blowup so it cannot latch in the filters below */
                l = 0.0f;
            if (!(r > -8.0f && r < 8.0f))
                r = 0.0f;
            ms.dc_l += 0.0028f * (l - ms.dc_l);   /* ~20 Hz highpass: feedback FM leaves a little DC */
            ms.dc_r += 0.0028f * (r - ms.dc_r);
            float drive, makeup;
            ms.film_s += 0.0005f * (k.film - ms.film_s);   /* FILM's saturation eases in, never steps */
            drive = 1.1f + 1.8f * ms.film_s;
            makeup = ms.sleep * ms.drop_g / (1.0f + 0.6f * ms.film_s);
            ms.drop_g += 0.002f * ((ms.drop_t > 0.0f ? 0.25f : 1.0f) - ms.drop_g);
            l = fm_tanhf((l - ms.dc_l) * drive) * makeup;
            r = fm_tanhf((r - ms.dc_r) * drive) * makeup;
            a = fm_fabsf(l) + fm_fabsf(r);
            if (a > peak)
                peak = a;
            lr[2 * (done + i)] = (int32_t)(l * gain);
            lr[2 * (done + i) + 1] = (int32_t)(r * gain);
        }
        done += m;
    }
    ms.peak = fm_maxf(peak * 0.5f, ms.peak * 0.97f);
    ms_vis.level = ms.peak;
}

void ms_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * FM_TWO_PI / (float)SN);
    fx_init();
    fx_clear();
    svf_set(&na.creek_l, 900.0f, 1.2f);
    svf_set(&na.creek_r, 1100.0f, 1.2f);
    na.surf_period = 9.0f;
    na.gust_target = 0.5f;
    st.mode = MODE_DRIFT;
    st.scene = 4;
    st.root = 0;
    st.playing = 1;
    st.sleep_target = ms.sleep = 1.0f;
    bt.level = 1.0f;
    for (i = 0; i < MS_NLAYERS; i++)
        lp.tgain[i] = 1.0f;
    lp.redo = -1;
    ms.drop_g = 1.0f;
    st.layer[SRC_KEYS] = st.layer[SRC_LOOP] = 1.0f;
    st.fx[0] = 30;
    st.fx[1] = 2;
    st.fx[2] = 45;
    st.fx[3] = 20;
    kb.snap = SNAP_SCALE;
    kb.harmony = 0.3f;
    kb.listen = 1;
    for (i = 0; i < 128; i++)
        kb.held[i][0] = kb.held[i][1] = kb.held[i][2] = -1;
    for (i = 0; i < 8; i++)
        kb.seed[i] = -1;
    chord_set(0);
    ms_vis.live_root = -1;
    for (i = 0; i < NLIVE; i++)
        cp.key[i] = -1;
    cp.follow = 1;
    cp.kind = MS_CHORD_MAJ7;
    cp.centre = 64.0f;
    lp.fade = 0.3f;
    lp.mutate = 0.25f;
    lp.smear = 0.3f;
    lp.speed = 1;
    {
        const ms_scene_t *sc = &SCENES[st.mode][st.scene];
        for (i = 0; i < MS_NMACRO; i++)
            st.macro[i] = sc->macro[i];
        for (i = 0; i < MS_NMIX; i++)
            st.mix[i] = sc->mix[i];
        st.tempo = sc->tempo;
        st.mood = sc->mood;
    }
    mode_start();
}

/* commands apply in order, so send the mode before its knob values */
void ms_mode(int mode, int scene) { post(C_MODE, mode, scene); }
void ms_macro(int i, int v) { post(C_MACRO, i, v); }
void ms_mix(int i, int v) { post(C_MIX, i, v); }
void ms_tempo(int bpm) { post(C_TEMPO, bpm, 0); }
void ms_root(int pc) { post(C_ROOT, pc, 0); }
void ms_mood(int mood) { post(C_MOOD, mood, 0); }
void ms_play(int on) { post(C_PLAY, on, 0); }
void ms_freeze(int on) { post(C_FREEZE, on, 0); }
void ms_new(void) { post(C_NEW, 0, 0); }
void ms_note(int note, int vel) { post(C_NOTE, note, vel); }
void ms_loop_keep(void) { post(C_LOOP, 2, 0); }
void ms_chord_note(int note, int vel) { post(C_CHORDNOTE, note, vel); }
void ms_evolve(int i, int v) { post(C_EVOLVE, i, v); }
void ms_panic(void) { post(C_PANIC, 0, 0); }
void ms_seed(uint32_t s) { post(C_SEED, (int)(s >> 16 & 0x7FFF), (int)(s & 0x7FFF)); }
void ms_sleep_fade(float level) { st.sleep_target = fm_clampf(level, 0.0f, 1.0f); }
/* called from the audio interrupt, so it acts directly instead of through the queue */
void ms_shed(void)
{
    int i, n = 0;
    for (i = 0; i < NV; i++)
        if (vo[i].stage == ST_REL)
            voice_cut(&vo[i]), n++;
    if (!n)
        voice_cut(voice_alloc(SRC_KEYS));
}
void ms_keys(int i, int v) { post(C_KEYSET, i, v); }
void ms_loop(int clear) { post(C_LOOP, clear, 0); }
void ms_loop_set(int i, int v) { post(C_LOOPSET, i, v); }
void ms_loop_fx(int i, int v) { post(C_LOOPFX, i, v); }
void ms_motion(int i, int v) { post(C_MOTION, i, v); }
void ms_chords(int i, int v) { post(C_CHORDS, i, v); }
void ms_fx(int i, int v) { post(C_FX, i, v); }
void ms_space(int sp) { post(C_SPACE, sp, 0); }
void ms_beat(int i, int v) { post(C_BEAT, i, v); }
void ms_track(int t, int v) { post(C_TRACK, t, v); }
int ms_loop_snap(void) { return post(C_SNAP, 0, 0); }
int ms_loop_load(void) { return post(C_LOAD, 0, 0); }
