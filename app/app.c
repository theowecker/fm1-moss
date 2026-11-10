/* SPDX-License-Identifier: MPL-2.0 */
/* Part of fm1-base's unity build, included after the platform (gfx.c and announce.c are in scope).
 *
 *   PRESETS  scene (all modes in one list)   ALGORITHM  space   SELECT  the screen's pages   SEQ  the next screen
 *   HOME SEL ENV LFO FX  pages HOME, MIX/BEAT, KEYS, LOOPS, FX/MOTION/EVOLVE   K1..K4  the page's knobs
 *   REC  keep (the looper)   GLO  take out (hold: clear the loop)   ARP  freeze   EDIT  new variation   PLAY  play/stop
 *   SAVE  save (hold: reset the scene)   OCT- OCT+  keys' octave */
#include <stddef.h>
#include "fastmath.h"

#define SET_MAGIC 0x4D4F5338u            /* "MOS8" */
/* each screen's pages in the order SELECT walks them: the sound, the loops, the chords, the beat; SETUP with the sound */
enum { PG_HOME, PG_MIX, PG_FX, PG_MOTION, PG_EVOLVE, PG_KEYS, PG_LOOP, PG_SHAPE, PG_TAPE, PG_LIBRARY, PG_CHORDS, PG_BUILD, PG_PLAY,
       PG_BEAT, PG_DRUMS, PG_BASS, PG_SETUP, NPAGES };
/* SEQ goes round the screens; SELECT walks the pages of the screen it is on */
enum { VIEW_SCENE, VIEW_STUDIO, VIEW_LOOPS, VIEW_CHORDS, VIEW_BEAT, NVIEWS };
static const char *const PAGE_NAME[NPAGES] = {"HOME", "MIX", "FX", "MOTION", "EVOLVE", "KEYS", "LAYERS", "SHAPE", "TAPE", "LIBRARY",
                                              "SIMPLE", "BUILD", "PLAY", "BEAT", "DRUMS", "BASS", "SETUP"};
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_SEL, B_FX, B_FX, B_FX, B_ENV, B_LFO, B_LFO, B_LFO, B_LFO, B_SEQ, B_SEQ, B_SEQ,
                                         B_SEL, B_SEL, B_SEL, B_HOME};
static const char *const DRUM_NAME[MS_NDRUMS] = {"KICK", "SNARE", "GHOST", "HAT", "OPEN HAT", "PERC"};
static const char *const BN_NAME[MS_NBN] = {"OFF", "ROOT", "3RD", "5TH", "7TH", "OCTAVE", "LEAD-IN"};
static const char *const CHANGE_NAME[4] = {"STILL", "SLOW", "NORMAL", "OFTEN"};
static const char *const PATH_NAME[8] = {"SCENE", "CIRCLE", "DREAM", "MOODY", "SONG", "DRONE", "MINE", "BUILT"};
enum { PATH_MINE = 6, PATH_BUILT = 7 };
/* BUILD's TYPE knob walks the chord kinds in this order, plain to coloured */
static const uint8_t KIND_ORDER[MS_NCHORDS] = {
    MS_CHORD_MAJ, MS_CHORD_MIN, MS_CHORD_DOM7, MS_CHORD_MAJ7, MS_CHORD_MIN7, MS_CHORD_6, MS_CHORD_MIN6, MS_CHORD_9,
    MS_CHORD_MIN9, MS_CHORD_MAJ9, MS_CHORD_ADD9, MS_CHORD_SUS2, MS_CHORD_SUS, MS_CHORD_7SUS, MS_CHORD_DIM, MS_CHORD_DIM7,
    MS_CHORD_HALFDIM, MS_CHORD_7B5, MS_CHORD_AUG,
};
static const char *const LEN_NAME[4] = {"1/2x", "1x", "2x", "4x"};
static const char *const VOICING_NAME[4] = {"CLOSE", "OPEN", "WIDE", "SMOOTH"};
static const char *const COLOR_NAME[4] = {"LUSH", "PLAIN", "SUS", "OPEN"};
static const char *const SNAP_NAME[3] = {"FREE", "SCALE", "CHORD"};
static const char *const VOICE_NAME[5] = {"AUTO", "EP", "BELL", "PAD", "KALIMBA"};
static const char *const TAP_NAME[3] = {"PLAY", "LISTEN", "HOLD"};
static const char *const SPEED_NAME[5] = {"1/2", "1x", "2x", "REV", "REV 1/2"};
static const char *const PAGE_KNOBS[NPAGES][4] = {
    {0}, {0}, {"DELAY", "TIME", "REPEATS", "FILM"}, {"MOD", "RANDOM", "TONE", "SPREAD"}, {"AMOUNT", "PACE", "CHORDS", "FILLS"},
    {"SNAP", "HARMONY", "VOICE", "TAP"},
    {"LAYER", "LEVEL", "MUTE", "SHIFT"}, {"FADE", "MUTATE", "SHIFT", "SPEED"}, {"WARBLE", "DIRT", "AGE", "SMEAR"},
    {"LOOP", "", "", ""}, {"CHORD", "CHANGE", "PATH", "COLOR"}, {"CHORD", "ROOT", "TYPE", "LENGTH"},
    {"TYPE", "VOICING", "STRUM", "FOLLOW"}, {"KIT", "PATTERN", "LEVEL", "BASS"}, {"DRUM", "LEVEL", "CHANCE", "SWING"},
    {"NOTE", "TONE", "GLIDE", "LENGTH"}, {"TEMPO", "KEY", "MOOD", "SLEEP"},
};
/* max value of a stepped knob; 0 means a 0..100 knob */
static const uint8_t PAGE_STEPS[NPAGES][4] = {
    {0}, {0}, {0, 5, 0, 0}, {0}, {0}, {2, 0, 4, 2}, {0}, {0, 0, 0, 4}, {0}, {0}, {7, 3, 7, 3}, {0}, {MS_NCHORDS, 3, 0, 1},
    {MS_NKITS - 1, MS_NPATTERNS - 1, 0, MS_NBASS - 1}, {0}, {MS_NBN - 2, 0, 0, 7}, {0},
};
#define PAT_MINE (MS_NPATTERNS - 1)
#define BASS_MINE (MS_NBASS - 1)
#define CONFIRM_WAIT 6000u               /* ms a library DELETE waits for GLO's second tap */
#define GLO_HOLD 1000u                   /* ms GLO is held to clear the loop */
/* ms since t, a time kept as plat_ms | 1 so 0 can mean none: signed, as t is a ms ahead of now when it is even */
static int32_t since(uint32_t now, uint32_t t) { return (int32_t)(now - t); }
#define BUILD_WAIT 6000u                 /* ms after a BUILD tap or edit that the next tap goes to the next chord */
#define LAYER_LVL 70                     /* a layer's level when it is new */
static int loop_page(int p) { return p >= PG_LOOP && p <= PG_LIBRARY; }
static int chord_page(int p) { return p >= PG_CHORDS && p <= PG_PLAY; }
static int beat_page(int p) { return p >= PG_BEAT && p <= PG_BASS; }
/* the screen a page lives on, when it is not one of the sound's */
static int page_view(int p) { return loop_page(p) ? VIEW_LOOPS : chord_page(p) ? VIEW_CHORDS : beat_page(p) ? VIEW_BEAT : -1; }
static int own_view(int v) { return v == VIEW_LOOPS || v == VIEW_CHORDS || v == VIEW_BEAT; }
static int view_first(int v) { return v == VIEW_LOOPS ? PG_LOOP : v == VIEW_CHORDS ? PG_CHORDS : PG_BEAT; }
static void lib_load(int i);
static void lib_delete(int i);
static void degree_name(int d, char *t);
static void knob_text(int i, char *t);
static const uint8_t SLEEP_MIN[] = {0, 10, 20, 30, 45, 60, 90};
#define NSLEEP (sizeof SLEEP_MIN)

typedef struct {
    uint32_t magic;
    uint8_t mode, page, root, sleep;
    int8_t octave;
    uint8_t view;
    uint8_t space;
    uint8_t rsv0;
    uint8_t scene[MS_NMODES];
    uint8_t macro[MS_NMODES][MS_NMACRO];
    uint8_t mix[MS_NMODES][MS_NMIX];
    uint8_t tempo[MS_NMODES], mood[MS_NMODES];
    uint8_t keys[4], loop[4], fx[4], tape[4], motion[4], evolve[4];
    uint8_t beat[MS_NMODES][4];          /* kit, pattern, level, bass */
    uint8_t chords[4];                   /* chord (0 AUTO, 1..7), change, path, color */
    uint8_t mine_n, mine[4];             /* PATH MINE: the progression played on the CHORDS page's keys, degrees 0..6 */
    uint8_t b_n;                         /* PATH BUILT: the chords made on the BUILD screen */
    uint8_t b_root[MS_NBUILT], b_kind[MS_NBUILT], b_len[MS_NBUILT];
    uint8_t play[4];                     /* PLAY: type (0 the key's own, else 1 + KIND_ORDER's), voicing, strum, follow */
    uint8_t drum_sel, swing;             /* DRUMS: the drum the keys set, SWING (0 the pattern's own) */
    uint8_t bassx[4];                    /* BASS: the note the keys set (0 ROOT .. 5 STEP), tone, glide, length - 1 */
    uint8_t drum_lvl[MS_NDRUMS], drum_ch[MS_NDRUMS];
    uint16_t drum_row[MS_NDRUMS];        /* PATTERN MINE: each drum's sixteen steps */
    uint8_t bass_line[16];               /* BASS MINE: MS_BN_* on each sixteenth */
    uint8_t drum_len;                    /* PATTERN MINE's steps: 16, or 12 when it came from WALTZ */
    uint32_t seed;                       /* a new one at each SAVE, so a scene's start differs from one boot to the next */
} settings_t;
static settings_t set;

#define SLOTS_MAGIC 0x534C4F31u          /* "SLO1" */
#define NSLOTS 8
static struct {
    uint32_t magic;
    ms_slot_t s[NSLOTS];
} slots;
_Static_assert(sizeof slots <= PLAT_STORE_MAX, "the slots fit one store object");
static uint8_t set_dirty;

static void scene_recipe(int m)
{
    const ms_scene_t *sc = ms_scene(m, set.scene[m]);
    int i;
    for (i = 0; i < MS_NMACRO; i++)
        set.macro[m][i] = sc->macro[i];
    for (i = 0; i < MS_NMIX; i++)
        set.mix[m][i] = sc->mix[i];
    set.tempo[m] = sc->tempo;
    set.mood[m] = sc->mood;
    set.beat[m][0] = sc->kit;
    set.beat[m][1] = sc->pattern;
    if (sc->beat_level)
        set.beat[m][2] = sc->beat_level;
}

static void settings_defaults(void)
{
    int m;
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.mode = MODE_DRIFT;
    set.scene[MODE_DRIFT] = 4;           /* GARDEN: the liveliest first impression */
    for (m = 0; m < MS_NMODES; m++) {
        set.beat[m][2] = 70;
        scene_recipe(m);
    }
    set.keys[0] = 1, set.keys[1] = 30, set.keys[2] = 0, set.keys[3] = 1;
    set.loop[0] = 30, set.loop[1] = 25, set.loop[2] = 30, set.loop[3] = 1;
    set.tape[3] = 30;                     /* SMEAR */
    set.fx[0] = 30, set.fx[1] = 2, set.fx[2] = 45, set.fx[3] = 20;
    set.motion[0] = 15, set.motion[1] = 0, set.motion[2] = 30, set.motion[3] = 70;
    set.evolve[0] = 30, set.evolve[1] = 40, set.evolve[2] = 30, set.evolve[3] = 30;
    set.chords[1] = 2;                    /* NORMAL */
    set.mine_n = 4;
    set.mine[0] = 0, set.mine[1] = 5, set.mine[2] = 3, set.mine[3] = 4;   /* I vi IV V until you play your own */
    {
        static const uint8_t ROOT[4] = {0, 9, 5, 7}, KIND[4] = {MS_CHORD_MAJ7, MS_CHORD_MIN9, MS_CHORD_MAJ7, MS_CHORD_7SUS};
        set.play[0] = 0, set.play[1] = 3, set.play[2] = 15, set.play[3] = 1;   /* the key's chords, SMOOTH, FOLLOW */
    {
        static const uint8_t LVL[MS_NDRUMS] = {90, 85, 30, 65, 55, 60};
        set.drum_len = (uint8_t)ms_pattern_rows(MODE_LOFI, 0, 1, set.drum_row, 0);   /* BOOMBAP until you make your own */
        for (m = 0; m < MS_NDRUMS; m++) {
            set.drum_lvl[m] = LVL[m];
            set.drum_ch[m] = 100;
        }
        set.bassx[1] = 10, set.bassx[3] = 2;
        set.bass_line[0] = MS_BN_ROOT, set.bass_line[7] = MS_BN_5TH, set.bass_line[10] = MS_BN_ROOT;
        set.bass_line[14] = MS_BN_STEP;
    }
        set.b_n = 4;                     /* Cmaj7 Am9 Fmaj7 G7sus4 until you build your own */
        for (m = 0; m < MS_NBUILT; m++) {
            set.b_root[m] = ROOT[m & 3];
            set.b_kind[m] = KIND[m & 3];
            set.b_len[m] = 1;
        }
    }
}

static int knob_ok(int page, int i, int v)
{
    return v <= (PAGE_STEPS[page][i] ? PAGE_STEPS[page][i] : 100);
}

static int settings_ok(void)
{
    int m;
    if (set.magic != SET_MAGIC || set.mode >= MS_NMODES || set.page >= NPAGES || set.root > 11u || set.sleep >= NSLEEP
        || set.octave < -2 || set.octave > 2 || set.view >= NVIEWS || set.space >= MS_NSPACES)
        return 0;
    for (m = 0; m < 4; m++)
        if (!knob_ok(PG_KEYS, m, set.keys[m]) || !knob_ok(PG_SHAPE, m, set.loop[m]) || !knob_ok(PG_FX, m, set.fx[m])
            || !knob_ok(PG_TAPE, m, set.tape[m]) || !knob_ok(PG_MOTION, m, set.motion[m]) || set.evolve[m] > 100
            || !knob_ok(PG_CHORDS, m, set.chords[m]) || !knob_ok(PG_PLAY, m, set.play[m]))
            return 0;
    if (set.mine_n < 1 || set.mine_n > 4)
        return 0;
    for (m = 0; m < 4; m++)
        if (set.mine[m] > 6)
            return 0;
    if (set.b_n < 1 || set.b_n > MS_NBUILT)
        return 0;
    for (m = 0; m < MS_NBUILT; m++)
        if (set.b_root[m] > 11 || set.b_kind[m] >= MS_NCHORDS || set.b_len[m] > 3)
            return 0;
    if (set.drum_sel >= MS_NDRUMS || set.swing > 9 || (set.drum_len != 12 && set.drum_len != 16))
        return 0;
    for (m = 0; m < 4; m++)
        if (!knob_ok(PG_BASS, m, set.bassx[m]))
            return 0;
    for (m = 0; m < MS_NDRUMS; m++)
        if (set.drum_lvl[m] > 100 || set.drum_ch[m] > 100)
            return 0;
    for (m = 0; m < 16; m++)
        if (set.bass_line[m] >= MS_NBN)
            return 0;
    for (m = 0; m < MS_NMODES * 4; m++)
        if (!knob_ok(PG_BEAT, m % 4, set.beat[m / 4][m % 4]))
            return 0;
    for (m = 0; m < MS_NMODES; m++) {
        int i;
        if (set.scene[m] >= MS_NSCENES[m] || set.mood[m] >= MS_NMOODS || set.tempo[m] < 40u || set.tempo[m] > 160u)
            return 0;
        for (i = 0; i < 4; i++)
            if (set.macro[m][i] > 100u || set.mix[m][i] > 100u)
                return 0;
    }
    return 1;
}

static void send_mode(void)
{
    int m = set.mode, i;
    ms_mode(m, set.scene[m]);
    for (i = 0; i < MS_NMACRO; i++)
        ms_macro(i, set.macro[m][i]);
    for (i = 0; i < MS_NMIX; i++)
        ms_mix(i, set.mix[m][i]);
    ms_tempo(set.tempo[m]);
    ms_mood(set.mood[m]);
    ms_root(set.root);
}

/* TAPE's knobs: warble, dirt and age to the loop's tape; SMEAR is the looper's own */
static void tape_send(int i)
{
    if (i == 3) {
        ms_loop_set(2, set.tape[3]);
        ms_loop_fx(3, 50);
    } else {
        ms_loop_fx(i, set.tape[i]);
    }
}

static void mine_send(void)
{
    ms_chords(4, set.mine_n << 12 | set.mine[3] << 9 | set.mine[2] << 6 | set.mine[1] << 3 | set.mine[0]);
}

static void drum_send(int d)
{
    ms_beat(10 + d, (int16_t)set.drum_row[d]);
    ms_beat(20 + d, set.drum_lvl[d] | set.drum_ch[d] << 7);
}

static void bassx_send(int i)
{
    if (i)
        ms_beat(4 + i, i == 3 ? set.bassx[3] + 1 : set.bassx[i]);
}

static void play_send(int i)
{
    int v = set.play[i];
    ms_chords(7 + i, i == 0 && v ? KIND_ORDER[v - 1] + 1 : v);
}

static void built_send(int j)
{
    ms_chords(5, j << 11 | set.b_len[j] << 9 | set.b_kind[j] << 4 | set.b_root[j]);
}

static void send_all(void)
{
    int i;
    send_mode();
    for (i = 0; i < 4; i++) {
        ms_keys(i, set.keys[i]);
        if (i != 2)
            ms_loop_set(i, set.loop[i]);
        tape_send(i);
        ms_fx(i, set.fx[i]);
        ms_motion(i, set.motion[i]);
        ms_evolve(i, set.evolve[i]);
        ms_chords(i, set.chords[i]);
        play_send(i);
    }
    ms_space(set.space);
    mine_send();
    for (i = 0; i < MS_NBUILT; i++)
        built_send(i);
    ms_chords(6, set.b_n);
    for (i = 0; i < 4; i++) {
        ms_beat(i, set.beat[set.mode][i]);
        bassx_send(i);
    }
    ms_beat(4, set.swing);
    ms_beat(46, set.drum_len);
    for (i = 0; i < MS_NDRUMS; i++)
        drum_send(i);
    for (i = 0; i < 16; i++)
        ms_beat(30 + i, set.bass_line[i]);
}

void app_init(void)
{
    ms_init();
    plat_usb_audio_full(1);              /* the computer always gets the full mix; MASTER sets the FM-1's own speaker */
    if (plat_store_size(OBJ_SETTINGS) != (int)sizeof set || plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set
        || !settings_ok())
        settings_defaults();
    if (plat_store_size(OBJ_SLOTS) != (int)sizeof slots || plat_store_load(OBJ_SLOTS, &slots, sizeof slots) != (int)sizeof slots
        || slots.magic != SLOTS_MAGIC) {
        memset(&slots, 0, sizeof slots);
        slots.magic = SLOTS_MAGIC;
    }
    {
        int i, j;
        for (i = 0; i < NSLOTS; i++) {
            ms_slot_t *sl = &slots.s[i];
            if (sl->n > MS_SLOT_EVENTS)
                sl->n = 0;
            if (sl->n && sl->rsv[0] != MS_SLOT_FORMAT) {   /* saved when there were four layers */
                for (j = 0; j < sl->n; j++) {
                    int v = sl->ev[j].vt >> 3;
                    sl->ev[j].vt = (uint8_t)((v ? v : 1) << 3 | (sl->ev[j].vt & 3u));
                }
                sl->rsv[0] = MS_SLOT_FORMAT;
            }
        }
    }
    if (page_view(set.page) != (own_view(set.view) ? set.view : -1))
        set.page = own_view(set.view) ? view_first(set.view) : PG_HOME;
    {                                    /* no clock or noise source: the MASTER pot's reading and the chip's id */
        const uint8_t *id = plat_chip_uid();
        uint32_t seed = plat_master() * 2654435761u ^ plat_ms() * 40503u ^ set.seed, i;
        for (i = 0; id && i < 8; i++)
            seed = seed * 31u + id[i];
        ms_seed(seed);
    }
    send_all();
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xF0u, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24 & 0x7Fu;
        if (st == 0x90u)
            ms_note_now((int)d1, (int)d2);
        else if (st == 0x80u)
            ms_note_now((int)d1, 0);
    }
    ms_render(lr, n, master_q12);
}

void app_panic(void) { ms_panic(); }
void app_overload(void) { ms_shed(); }

static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint32_t save_t;
    uint8_t key_note[NKEYS];             /* note sent at key down, so the note-off matches after an octave change */
    uint32_t key_chord;                  /* keys down on PLAY: their note-off ends a whole chord */
    uint8_t track_lvl[MS_NLAYERS];       /* the layers' levels; not saved, as the loop itself is not */
    uint8_t sel_track, sel_slot, sel_chord;
    uint8_t confirm[4];                  /* [3]: GLO asked once, waiting for the second */
    uint32_t confirm_t;
    uint32_t seq_t;
    uint32_t mine_t;                     /* the last chord key, plat_ms | 1 */
    uint32_t build_t;                    /* the last BUILD key or edit, plat_ms | 1 */
    uint8_t build_from, build_taps;      /* where that run of taps began, and how many */
    uint8_t chord_tab, beat_tab;         /* the CHORDS and BEAT pages last open, to come back to */
    uint8_t play_shown;                  /* PLAY's chord still matches TYPE */
    uint32_t glo_t;                      /* GLO down, plat_ms | 1; 0 when up or its hold was used */
    uint8_t glo_warned;                  /* the hold has said it is coming */
    uint8_t confirm_page;                /* the page an armed GLO was asked on */
    uint8_t copied;                      /* this edit made a preset into MINE */
    uint8_t hist[MS_NLAYERS], hist_n;    /* the layers in the order they came, for GLO's undo */
    uint8_t view_before, page_before;    /* where the LOOPS screen was opened from, to go back to */
    uint8_t tracks_seen;                 /* ms_vis.lp_tracks last frame: a layer appearing gets its level back */
    uint8_t save_long;                   /* SAVE held: 1 resets the scene on release, 2 everything */
    int8_t slot_save;                    /* the library entry a snapshot is on its way to, -1 none */
    int8_t lib_cur;                      /* the entry the playing loop is kept in, -1 none */
    int8_t lib_load_idx;                 /* the entry being loaded, -1 none */
    uint8_t lib_dirty;                   /* the library changed since SAVE */
    uint32_t lib_gen, lib_snap_t;
    uint8_t xfer_busy, export_state;
    uint32_t xfer_wait, xfer_t;
    uint32_t sleep_start;                /* plat_ms | 1 when set, 0 off */
    uint32_t last_flash, flash_at, last_calls, calls_at, last_chirps, chirp_at;
    uint32_t vis_at, label_until;
    uint8_t redraw, playing, frozen;
} ui;

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static uint32_t text_len(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t a = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + a % 10u);
    while ((a /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static uint8_t *page_vals(int page)
{
    return page == PG_KEYS ? set.keys : page == PG_SHAPE ? set.loop : page == PG_TAPE ? set.tape : page == PG_FX ? set.fx
         : page == PG_MOTION ? set.motion : page == PG_EVOLVE ? set.evolve : page == PG_CHORDS ? set.chords
         : page == PG_PLAY ? set.play
         : page == PG_BEAT ? set.beat[set.mode] : page == PG_BASS ? set.bassx : 0;
}

static const char *knob_name(int i)
{
    if (set.page == PG_HOME)
        return MS_MACRO_NAME[set.mode][i];
    if (set.page == PG_MIX)
        return MS_MIX_NAME[set.mode][i];
    return PAGE_KNOBS[set.page][i];
}


static int knob_value(int i)   /* 0..100 */
{
    int m = set.mode, steps = PAGE_STEPS[set.page][i];
    uint8_t *v = page_vals(set.page);
    if (set.page == PG_LOOP)
        return i == 0 ? ui.sel_track * 100 / (MS_NLAYERS - 1) : i == 1 ? ui.track_lvl[ui.sel_track]
             : i == 2 ? (ms_vis.lp_mute >> ui.sel_track & 1u ? 100 : 0) : 50;
    if (set.page == PG_LIBRARY)
        return i == 0 ? ui.sel_slot * 100 / 7 : 0;
    if (set.page == PG_DRUMS)
        return i == 0 ? set.drum_sel * 100 / (MS_NDRUMS - 1) : i == 1 ? set.drum_lvl[set.drum_sel]
             : i == 2 ? set.drum_ch[set.drum_sel] : set.swing * 100 / 9;
    if (set.page == PG_BUILD) {
        int j = ui.sel_chord, k;
        if (i == 0)
            return j * 100 / (MS_NBUILT - 1);
        if (j >= set.b_n)
            return 0;
        for (k = 0; k < MS_NCHORDS - 1 && KIND_ORDER[k] != set.b_kind[j]; k++)
            ;
        return i == 1 ? set.b_root[j] * 100 / 11 : i == 2 ? k * 100 / (MS_NCHORDS - 1) : set.b_len[j] * 100 / 3;
    }
    if (set.page == PG_SHAPE && i == 2)
        return 50;
    if (set.page == PG_HOME)
        return set.macro[m][i];
    if (set.page == PG_MIX)
        return set.mix[m][i];
    if (v)
        return steps ? v[i] * 100 / steps : v[i];
    if (i == 0)
        return (set.tempo[m] - 40) * 100 / 120;
    if (i == 1)
        return set.root * 100 / 11;
    if (i == 2)
        return set.mood[m] * 100 / (MS_NMOODS - 1);
    return (int)set.sleep * 100 / (int)(NSLEEP - 1);
}

/* the pitch class of the key's degree d, from the scale the mood makes */
static int degree_pc(int d)
{
    uint16_t mask = ms_scale_mask(set.mood[set.mode], set.root);
    int pc = set.root, n = 0;
    for (;;) {
        if (mask >> pc & 1u && n++ == d)
            return pc;
        pc = (pc + 1) % 12;
    }
}

static const char *const ROMAN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};

/* the degree of the key a note falls on, a note outside the scale taken as the one below it */
static int note_degree(int note)
{
    uint16_t mask = ms_scale_mask(set.mood[set.mode], set.root);
    int pc = note % 12, d = 0, p;
    while (!(mask >> pc & 1u))
        pc = (pc + 11) % 12;
    for (p = set.root; p != pc; p = (p + 1) % 12)
        d += mask >> p & 1u;
    return d;
}

/* the CHORDS page's keys: each tap adds the chord on that note to MINE, up to four; after a pause a tap starts anew */
static void chord_key(int note, uint32_t now)
{
    char t[24];
    int i, d = note_degree(note);
    if (set.mine_n >= 4 || since(now, ui.mine_t) > (int32_t)BUILD_WAIT || !ui.mine_t)
        set.mine_n = 0;
    ui.mine_t = now | 1u;
    set.mine[set.mine_n++] = (uint8_t)d;
    for (i = set.mine_n; i < 4; i++)
        set.mine[i] = set.mine[set.mine_n - 1];
    set.chords[2] = 6;                   /* PATH: MINE */
    set.chords[0] = 0;                   /* CHORD: AUTO, so the progression moves */
    ms_chords(0, 0);
    ms_chords(2, 6);
    mine_send();
    set_dirty = 1;
    {
        char a[24] = "MINE ";
        int_text(set.mine_n, a + 5);
        text_cat(a, set.mine_n < 4 ? " OF 4" : " OF 4. DONE");
        t[0] = 0;
        degree_name(set.mine[set.mine_n - 1], t);
        app_say(a, t);
    }
}

/* the BUILD screen: up to MS_NBUILT chords of any root, kind and length. A key sets the chosen chord's root, with
 * the chord the key has on that note (a major one off the key); a tap soon after goes to the next chord, so a
 * progression can be played in, while the knobs stay on the chord just tapped */
static void chord_name(int j, char *t)
{
    t[0] = 0;
    text_cat(t, MS_NOTE_NAME[set.b_root[j]]);
    text_cat(t, MS_CHORD_SUFFIX[set.b_kind[j]]);
}

static int key_kind_on(int pc)
{
    uint16_t mask = ms_scale_mask(set.mood[set.mode], set.root);
    int d, third, fifth, seventh;
    if (!(mask >> pc & 1u))
        return MS_CHORD_MAJ;
    d = note_degree(pc);
    third = (degree_pc((d + 2) % 7) - pc + 12) % 12;
    fifth = (degree_pc((d + 4) % 7) - pc + 12) % 12;
    seventh = (degree_pc((d + 6) % 7) - pc + 12) % 12;
    return fifth == 6 ? MS_CHORD_HALFDIM : third == 3 ? MS_CHORD_MIN7 : seventh == 11 ? MS_CHORD_MAJ7 : MS_CHORD_DOM7;
}

/* the progression is BUILT's from the first change made to it, and the chord just changed is heard soon */
static void built_use(int hear)
{
    if (set.chords[2] != PATH_BUILT || set.chords[0]) {
        set.chords[2] = PATH_BUILT;
        set.chords[0] = 0;
        ms_chords(0, 0);
        ms_chords(2, PATH_BUILT);
    }
    ms_chords(6, set.b_n | (hear + 1) << 4);
    set_dirty = 1;
}

/* the chord the key has on degree d, by name: Am7 */
static void degree_name(int d, char *t)
{
    int pc = degree_pc(d), kind = key_kind_on(pc);
    text_cat(t, MS_NOTE_NAME[pc]);
    text_cat(t, MS_CHORD_SUFFIX[kind]);
}

static void say_chord(int j)
{
    char a[12], t[16];
    a[0] = 0;
    text_cat(a, "CHORD ");
    int_text(j + 1, a + 6);
    if (j >= set.b_n) {
        app_say(a, "NEW: TAP A KEY");
        return;
    }
    chord_name(j, t);
    if (set.b_len[j] != 1) {
        text_cat(t, " ");
        text_cat(t, LEN_NAME[set.b_len[j]]);
    }
    app_say(a, t);
}

/* a new chord at the end starts from the one before it */
static int built_grow(int j)
{
    if (j < set.b_n)
        return 1;
    if (j != set.b_n || set.b_n >= MS_NBUILT)
        return 0;
    set.b_root[j] = set.b_root[j - 1];
    set.b_kind[j] = set.b_kind[j - 1];
    set.b_len[j] = 1;
    set.b_n++;
    return 1;
}

static void build_key(int note, uint32_t now)
{
    int j = ui.sel_chord;
    if (ui.build_t && since(now, ui.build_t) < (int32_t)BUILD_WAIT && j + 1 < MS_NBUILT) {
        j++;
    } else if (!ui.build_t || since(now, ui.build_t) >= (int32_t)BUILD_WAIT) {
        ui.build_from = (uint8_t)j;
        ui.build_taps = 0;
    }
    ui.build_taps++;
    ui.build_t = now | 1u;
    if (!built_grow(j))
        return;
    ui.sel_chord = (uint8_t)j;
    set.b_root[j] = (uint8_t)(note % 12);
    set.b_kind[j] = (uint8_t)key_kind_on(note % 12);
    built_send(j);
    built_use(j);
    say_chord(j);
}

/* a run of taps from the first chord, then left alone, that stops short of the old progression ends it there:
 * playing C Am F in leaves C Am F, not C Am F and an old 4th. Picking a chord with K1 ends a run as it is */
static void build_end(void)
{
    if (!ui.build_t)
        return;
    ui.build_t = 0;
    if (ui.build_from == 0 && ui.build_taps >= 2 && ui.sel_chord + 1 < set.b_n) {
        char t[16];
        set.b_n = (uint8_t)(ui.sel_chord + 1);
        ms_chords(6, set.b_n);
        set_dirty = 1;
        t[0] = 0;
        int_text(set.b_n, t);
        text_cat(t, " CHORDS");
        app_say("PROGRESSION", t);
    }
}

static void build_select(int32_t d)
{
    ui.build_t = 0;
    int top = set.b_n < MS_NBUILT ? set.b_n : MS_NBUILT - 1;   /* one past the last: a new chord */
    ui.sel_chord = (uint8_t)clamp(ui.sel_chord + d, 0, top);
    ui.confirm[3] = 0;
    ui.confirm_t = 0;
    say_chord(ui.sel_chord);
}

static void build_tick(uint32_t now)
{
    if (ui.build_t && since(now, ui.build_t) >= (int32_t)BUILD_WAIT)
        build_end();
}

static void build_remove(void)
{
    int j = ui.sel_chord, i;
    if (j >= set.b_n || set.b_n < 2)
        return;
    ui.build_t = 0;                      /* a run of taps ends here, as it is */
    for (i = j; i + 1 < MS_NBUILT; i++) {
        set.b_root[i] = set.b_root[i + 1];
        set.b_kind[i] = set.b_kind[i + 1];
        set.b_len[i] = set.b_len[i + 1];
    }
    set.b_n--;
    ms_chords(6, set.b_n | (j + 1) << 8);   /* before the chords move up, so the one playing keeps its place */
    for (i = j; i < MS_NBUILT; i++)
        built_send(i);
    if (ui.sel_chord >= set.b_n)
        ui.sel_chord = (uint8_t)(set.b_n - 1);
    set_dirty = 1;
    app_say("CHORD REMOVED", "");
}

/* K1 picks the chord, one past the last a new one; GLO takes the chosen one out */
static int build_turn(int i, int32_t d)
{
    int j = ui.sel_chord, k;
    char t[16];
    if (i == 0) {
        build_select(d);
        return 1;
    }
    if (ui.build_t)                      /* an edit between taps keeps the run going */
        ui.build_t = plat_ms() | 1u;
    if (!built_grow(j))
        return 1;
    if (i == 1) {
        set.b_root[j] = (uint8_t)((set.b_root[j] + d % 12 + 12) % 12);
    } else if (i == 2) {
        for (k = 0; k < MS_NCHORDS - 1 && KIND_ORDER[k] != set.b_kind[j]; k++)
            ;
        set.b_kind[j] = KIND_ORDER[clamp(k + d, 0, MS_NCHORDS - 1)];
    } else {
        set.b_len[j] = (uint8_t)clamp(set.b_len[j] + d, 0, 3);
    }
    built_send(j);
    built_use(j);
    if (i == 3) {
        chord_name(j, t);
        app_say(t, LEN_NAME[set.b_len[j]]);
    } else {
        say_chord(j);
    }
    return 1;
}

/* the BEAT screen's DRUMS and BASS: the first 16 keys are the sixteenths, the keys above pick a drum or a bass note
 * and play it. Editing changes what you hear: a preset pattern or bass is copied into MINE first */
static int beat_rows(uint16_t rows[MS_NDRUMS])
{
    int m = set.mode, i;
    if (set.beat[m][1] != PAT_MINE)
        return ms_pattern_rows(m, set.scene[m], set.beat[m][1], rows, 0);
    for (i = 0; i < MS_NDRUMS; i++)
        rows[i] = set.drum_row[i];
    return set.drum_len;
}

static void drums_mine(void)
{
    int m = set.mode, i;
    if (set.beat[m][1] != PAT_MINE) {
        char t[24] = "COPIED FROM ";
        float sw;
        text_cat(t, MS_PATTERN_NAME[set.beat[m][1]]);
        set.drum_len = (uint8_t)ms_pattern_rows(m, set.scene[m], set.beat[m][1], set.drum_row, &sw);
        ms_beat(46, set.drum_len);
        if (!set.swing) {                /* its swing comes along, as SWING's nearest step */
            set.swing = (uint8_t)clamp((int)(sw / 0.04f + 0.5f) + 1, 1, 9);
            ms_beat(4, set.swing);
        }
        set.beat[m][1] = PAT_MINE;
        for (i = 0; i < MS_NDRUMS; i++)
            drum_send(i);
        ms_beat(1, PAT_MINE);
        app_say("YOUR PATTERN", t);
        ui.copied = 1;
    }
    if (!set.beat[m][0]) {               /* no kit, nothing to hear */
        set.beat[m][0] = 2;
        ms_beat(0, 2);
    }
    set_dirty = 1;
}

static void bass_mine(void)
{
    int m = set.mode, i;
    uint16_t rows[MS_NDRUMS];
    if (set.beat[m][3] == BASS_MINE)
        return;
    beat_rows(rows);                     /* from what it played: ROOT on the kicks, WALK's four, else the root on 1 */
    for (i = 0; i < 16; i++)
        set.bass_line[i] = set.beat[m][3] == 2 ? ((rows[MS_DRUM_KICK] >> i & 1u) || !i ? MS_BN_ROOT : MS_BN_OFF)
                         : set.beat[m][3] == 3 ? (i == 0 ? MS_BN_ROOT : i == 4 ? MS_BN_3RD : i == 8 ? MS_BN_5TH
                                                  : i == 12 ? MS_BN_STEP : MS_BN_OFF)
                         : i == 0 ? MS_BN_ROOT : MS_BN_OFF;
    for (i = 0; i < 16; i++)
        ms_beat(30 + i, set.bass_line[i]);
    set.beat[m][3] = BASS_MINE;
    ms_beat(3, BASS_MINE);
    ui.copied = 1;
    set_dirty = 1;
}

static void step_say(const char *what, int k, const char *v)
{
    char a[24], t[12] = "STEP ";
    a[0] = 0;
    text_cat(a, what);
    int_text(k + 1, t + 5);
    text_cat(a, " ");
    text_cat(a, t);
    app_say(a, v);
}

static void drums_key(int k)
{
    int d = set.drum_sel;
    if (k >= 16) {
        if (k - 16 < MS_NDRUMS) {
            set.drum_sel = (uint8_t)(k - 16);
            ms_beat(8, set.drum_sel);
            app_say(DRUM_NAME[set.drum_sel], "TAP ITS STEPS");
        }
        return;
    }
    ui.copied = 0;
    drums_mine();
    if (k >= set.drum_len) {
        app_say("A BAR OF 12 STEPS", "");
        return;
    }
    set.drum_row[d] ^= (uint16_t)(1u << k);
    drum_send(d);
    if (!ui.copied)                      /* the copy's news goes first */
        step_say(DRUM_NAME[d], k, set.drum_row[d] >> k & 1u ? "ON" : "OFF");
}

static void bass_key(int k)
{
    int v = set.bassx[0] + 1;
    if (k >= 16) {
        if (k - 16 < MS_NBN - 1) {
            set.bassx[0] = (uint8_t)(k - 16);
            ms_beat(9, k - 15);
            app_say(BN_NAME[k - 15], set.beat[set.mode][3] == 1 ? "BASS IS OFF ON BEAT" : "TAP WHERE IT GOES");
            set_dirty = 1;
        }
        return;
    }
    ui.copied = 0;
    bass_mine();                         /* the line was not on screen before: the first tap places, never removes */
    set.bass_line[k] = (uint8_t)(set.bass_line[k] == v && !ui.copied ? MS_BN_OFF : v);
    ms_beat(30 + k, set.bass_line[k]);
    step_say("BASS", k, BN_NAME[set.bass_line[k]]);
}

static void drums_turn(int i, int32_t d)
{
    int r = set.drum_sel;
    char t[16];
    if (i == 0) {
        set.drum_sel = (uint8_t)clamp(r + d, 0, MS_NDRUMS - 1);
        ms_beat(8, set.drum_sel);
    } else if (i == 3) {
        set.swing = (uint8_t)clamp(set.swing + d, 0, 9);
        ms_beat(4, set.swing);
    } else {
        uint8_t *v = i == 1 ? &set.drum_lvl[r] : &set.drum_ch[r];
        drums_mine();
        *v = (uint8_t)clamp(*v + 2 * d, 0, 100);
        drum_send(r);
    }
    set_dirty = 1;
    knob_text(i, t);
    app_say(i ? PAGE_KNOBS[PG_DRUMS][i] : "DRUM", t);
}

static void chords_text(int i, char *t)
{
    const uint8_t *c = set.chords;
    if (i == 0 && c[0]) {
        text_cat(t, ROMAN[c[0] - 1]);
        text_cat(t, " ");
        degree_name(c[0] - 1, t);
    } else {
        text_cat(t, i == 0 ? "AUTO" : i == 1 ? CHANGE_NAME[c[1]] : i == 2 ? PATH_NAME[c[2]] : COLOR_NAME[c[3]]);
    }
}

static void knob_text(int i, char *t)
{
    int m = set.mode;
    t[0] = 0;
    if (set.page == PG_LOOP) {
        int sel = ui.sel_track;
        if (i == 0) {
            int_text(sel + 1, t);
        } else if (i == 1) {
            if (ui.track_lvl[sel])
                int_text(ui.track_lvl[sel], t);
            else
                text_cat(t, "OFF");
        } else if (i == 2) {
            text_cat(t, ms_vis.lp_mute >> sel & 1u ? "MUTED" : "PLAYS");
        } else {
            text_cat(t, "< >");
        }
        return;
    }
    if (set.page == PG_LIBRARY) {
        if (i == 0) {
            int_text(ui.sel_slot + 1, t);
        }
        return;
    }
    if (set.page == PG_EVOLVE && i != 1 && !set.evolve[i]) {
        text_cat(t, "OFF");
        return;
    }
    if (set.page == PG_DRUMS) {
        if (i == 0)
            text_cat(t, DRUM_NAME[set.drum_sel]);
        else if (i == 3 && !set.swing)
            text_cat(t, "AUTO");
        else {
            int_text(i == 1 ? set.drum_lvl[set.drum_sel] : i == 2 ? set.drum_ch[set.drum_sel] : 48 + 2 * set.swing, t);
            if (i > 1)
                text_cat(t, "%");
        }
        return;
    }
    if (set.page == PG_BASS && i != 1 && i != 2) {
        if (i == 0) {
            text_cat(t, BN_NAME[set.bassx[0] + 1]);
        } else {
            int_text(set.bassx[3] + 1, t);
            text_cat(t, "/16");
        }
        return;
    }
    if (set.page == PG_BUILD) {
        int j = ui.sel_chord;
        if (i == 0) {
            int_text(j + 1, t);
            text_cat(t, j >= set.b_n ? " NEW" : "");
        } else if (j >= set.b_n) {
            text_cat(t, "-");
        } else {
            text_cat(t, i == 1 ? MS_NOTE_NAME[set.b_root[j]] : i == 2 ? (set.b_kind[j] == MS_CHORD_MAJ ? "maj"
                        : MS_CHORD_SUFFIX[set.b_kind[j]]) : LEN_NAME[set.b_len[j]]);
        }
        return;
    }
    if (set.page == PG_SHAPE && i == 2) {
        text_cat(t, "< >");
        return;
    }
    if (set.page == PG_KEYS && i != 1)
        text_cat(t, i == 0 ? SNAP_NAME[set.keys[0]] : i == 2 ? VOICE_NAME[set.keys[2]] : TAP_NAME[set.keys[3]]);
    else if (set.page == PG_SHAPE && i == 3)
        text_cat(t, SPEED_NAME[set.loop[3]]);
    else if (set.page == PG_FX && i == 1)
        text_cat(t, MS_DIV_NAME[set.fx[1]]);
    else if (set.page == PG_CHORDS)
        chords_text(i, t);
    else if (set.page == PG_PLAY && i != 2) {
        const uint8_t *p = set.play;
        int kind = p[0] ? KIND_ORDER[p[0] - 1] : 0;
        text_cat(t, i == 0 ? (!p[0] ? "KEY" : kind == MS_CHORD_MAJ ? "maj" : MS_CHORD_SUFFIX[kind])
                    : i == 1 ? VOICING_NAME[p[1]] : p[3] ? "ON" : "OFF");
    }
    else if (set.page == PG_BEAT && i != 2) {
        const uint8_t *b = set.beat[m];
        text_cat(t, i == 0 ? MS_KIT_NAME[b[0]] : i == 1 ? MS_PATTERN_NAME[b[1]] : MS_BASS_NAME[b[3]]);
    }
    else if (set.page != PG_SETUP)
        int_text(knob_value(i), t);
    else if (i == 0)
        int_text(set.tempo[m], t);
    else if (i == 1)
        text_cat(t, MS_NOTE_NAME[set.root]);
    else if (i == 2)
        text_cat(t, MS_MOOD_NAME[set.mood[m]]);
    else if (set.sleep == 0)
        text_cat(t, "OFF");
    else {
        int_text(SLEEP_MIN[set.sleep], t);
        text_cat(t, "m");
    }
}

static void say_knob(int i)
{
    char t[16];
    knob_text(i, t);
    app_say(knob_name(i), t);
}

static void sleep_set(int idx)
{
    set.sleep = (uint8_t)idx;
    ui.sleep_start = idx ? plat_ms() | 1u : 0;
    ms_sleep_fade(1.0f);
}

static void layer_name(int i, char *t)
{
    t[0] = 0;
    text_cat(t, "LAYER ");
    t[6] = (char)('1' + i);
    t[7] = 0;
}

static void layer_clear(int t)
{
    char n[12];
    ms_track(t, -2);
    ui.track_lvl[t] = LAYER_LVL;
    ms_track(t, LAYER_LVL);
    layer_name(t, n);
    app_say(n, "REMOVED");
}

/* LAYERS works on one layer at a time: K1 picks it, K2 its level (down is OFF), K3 mutes it, K4 moves it a
 * sixteenth later or earlier. GLO takes it out */
static int layer_turn(int i, int32_t d)
{
    char t[12];
    int sel = ui.sel_track, used = ms_vis.lp_tracks >> sel & 1u, n = d < 0 ? -d : d;
    if (i == 0) {
        ui.sel_track = (uint8_t)clamp(sel + d, 0, MS_NLAYERS - 1);
        layer_name(ui.sel_track, t);
        app_say(t, ms_vis.lp_tracks >> ui.sel_track & 1u ? "" : "EMPTY");
        return 1;
    }
    layer_name(sel, t);
    if (i == 1) {
        ui.track_lvl[sel] = (uint8_t)clamp(ui.track_lvl[sel] + 2 * d, 0, 100);
        ms_track(sel, ui.track_lvl[sel]);
        if (!ui.track_lvl[sel])
            app_say(t, "OFF");
        return !ui.track_lvl[sel];
    }
    if (!used) {
        app_say(t, "EMPTY");
        return 1;
    }
    if (i == 2) {
        ms_track(sel, d > 0 ? -1 : -3);
        app_say(t, d > 0 ? "MUTED" : "PLAYING");
        return 1;
    }
    if (ms_vis.lp_state == MS_LOOP_REC) {   /* no length yet to move it in */
        app_say(t, "WAIT TILL IT LOOPS");
        return 1;
    }
    while (n--)
        ms_track(sel, d > 0 ? -4 : -5);
    app_say(t, d > 0 ? "LATER" : "EARLIER");
    return 1;
}

/* a new layer starts at the usual level, whatever the layer before it in that place was left at; the layers' order
 * is kept for GLO's undo */
static void tracks_tick(void)
{
    uint8_t now = ms_vis.lp_tracks, born = (uint8_t)(now & ~ui.tracks_seen);
    int i, j, k;
    for (i = 0; i < MS_NLAYERS; i++)
        if (born >> i & 1u) {
            if (ui.track_lvl[i] != LAYER_LVL) {
                ui.track_lvl[i] = LAYER_LVL;
                ms_track(i, LAYER_LVL);
            }
            if (ui.hist_n < MS_NLAYERS)
                ui.hist[ui.hist_n++] = (uint8_t)i;
        }
    for (j = k = 0; j < ui.hist_n; j++)  /* gone layers leave the order */
        if (now >> ui.hist[j] & 1u)
            ui.hist[k++] = ui.hist[j];
    ui.hist_n = (uint8_t)k;
    ui.tracks_seen = now;
}

/* the LOOP page: one layer at a time; the LIBRARY page: K1 plays a loop; SHAPE's SHIFT moves the whole loop.
 * 1 = it spoke for itself */
static int pages_act(int i, int32_t d)
{
    if (set.page == PG_LOOP)
        return layer_turn(i, d);
    if (set.page == PG_SHAPE) {          /* SHIFT: the loop a sixteenth later or earlier a click */
        int32_t n = d < 0 ? -d : d, t;
        if (ms_vis.lp_state == MS_LOOP_EMPTY) {
            app_say("SHIFT", "NO LOOP");
            return 1;
        }
        while (n--)
            for (t = 0; t < MS_NLAYERS; t++)
                ms_track(t, d > 0 ? -4 : -5);
        app_say("SHIFT", d > 0 ? "LATER" : "EARLIER");
        return 1;
    }
    if (i == 0) {                        /* LIBRARY: turning to a loop plays it */
        ui.sel_slot = (uint8_t)clamp(ui.sel_slot + d, 0, NSLOTS - 1);
        ui.confirm[3] = 0;
        lib_load(ui.sel_slot);
    }
    return 1;
}

/* REC keeps: an empty looper records; what is recording or playing is kept, and what you play next is a new layer */
static void rec_press(void)
{
    uint32_t st = ms_vis.lp_state;
    if (set.keys[3] == 2 && st == MS_LOOP_EMPTY) {   /* HOLD's notes are planted, not looped */
        app_say("HOLD", "KEYS PLANT NOTES");
        return;
    }
    if (set.keys[3] == 2) {
        app_say("KEPT", "");
        ms_loop_keep();
        return;
    }
    if (st == MS_LOOP_EMPTY)
        app_say("RECORDING", "REC AGAIN TO KEEP IT");
    else if (st == MS_LOOP_REC && !ms_vis.lp_n)
        app_say("NOTHING RECORDED", "");
    else if (st == MS_LOOP_PLAY && set.keys[3] != 1 && ms_vis.lp_keep)
        app_say("ADDING A LAYER", "REC AGAIN TO KEEP IT");
    else
        app_say("KEPT", set.keys[3] == 1 ? "NOW PLAY A NEW LAYER" : "REC AGAIN ADDS A LAYER");
    ms_loop_keep();
}

/* a second GLO within CONFIRM_WAIT on the same page: 1 do it; else ask, and 0 */
static int glo_again(uint32_t now, const char *ask)
{
    if (ui.confirm[3] && ui.confirm_page == set.page) {
        ui.confirm[3] = 0;
        ui.confirm_t = 0;
        return 1;
    }
    ui.confirm[3] = 1;
    ui.confirm_t = now | 1u;
    ui.confirm_page = set.page;
    app_say(ask, "GLO AGAIN TO DO IT");
    ui.msg_until = now + CONFIRM_WAIT;   /* the question stays up while it waits */
    return 0;
}

/* GLO takes out, by page: the chord on BUILD, a drum's steps on DRUMS, the line on BASS, the chosen layer on LAYERS,
 * the chosen loop on LIBRARY (asked twice: it does not come back), else the newest layer or the recording */
static void glo_tap(uint32_t now)
{
    char t[16];
    int i;
    if (ms_vis.lp_state == MS_LOOP_REC) {   /* a recording first, whatever the page */
        ms_loop(1);
        app_say("RECORDING", "REMOVED");
        return;
    }
    if (set.page == PG_BUILD) {
        if (set.b_n < 2)
            app_say("REMOVE", "ONE CHORD STAYS");
        else if (ui.sel_chord < set.b_n)
            build_remove();
        return;
    }
    if ((set.page == PG_DRUMS || set.page == PG_BASS) && !glo_again(now, set.page == PG_DRUMS ? "REMOVE ALL ITS STEPS?"
                                                                                              : "REMOVE THE BASS LINE?"))
        return;
    if (set.page == PG_DRUMS) {
        drums_mine();
        set.drum_row[set.drum_sel] = 0;
        drum_send(set.drum_sel);
        app_say(DRUM_NAME[set.drum_sel], "STEPS REMOVED");
        return;
    }
    if (set.page == PG_BASS) {
        bass_mine();
        for (i = 0; i < 16; i++) {
            set.bass_line[i] = MS_BN_OFF;
            ms_beat(30 + i, MS_BN_OFF);
        }
        app_say("BASS LINE", "REMOVED");
        return;
    }
    if (set.page == PG_LIBRARY) {
        char a[24] = "DELETE LOOP ";
        int_text(ui.sel_slot + 1, a + 12);
        text_cat(a, "?");
        if (!slots.s[ui.sel_slot].n)
            app_say("NOTHING THERE", "");
        else if (glo_again(now, a))
            lib_delete(ui.sel_slot);
        return;
    }
    if (set.page == PG_LOOP) {
        if (ms_vis.lp_tracks >> ui.sel_track & 1u)
            layer_clear(ui.sel_track);
        else
            app_say("LAYER", "EMPTY");
        return;
    }
    if (ui.hist_n) {
        int l = ui.hist[ui.hist_n - 1];
        layer_clear(l);
        layer_name(l, t);
        app_say(t, "REMOVED");
    } else {
        app_say("NOTHING TO UNDO", "");
    }
}

static void knob_turn(int i, int32_t d)
{
    int m = set.mode;
    if (set.page == PG_HOME) {
        set.macro[m][i] = (uint8_t)clamp(set.macro[m][i] + 2 * d, 0, 100);
        ms_macro(i, set.macro[m][i]);
    } else if (set.page == PG_MIX) {
        set.mix[m][i] = (uint8_t)clamp(set.mix[m][i] + 2 * d, 0, 100);
        ms_mix(i, set.mix[m][i]);
    } else if (set.page == PG_BUILD) {
        build_turn(i, d);
        return;
    } else if (set.page == PG_DRUMS) {
        drums_turn(i, d);
        return;
    } else if (set.page == PG_LOOP || set.page == PG_LIBRARY || (set.page == PG_SHAPE && i == 2)) {
        if (!pages_act(i, d))
            say_knob(i);
        return;
    } else if (page_vals(set.page)) {
        uint8_t *v = page_vals(set.page);
        int steps = PAGE_STEPS[set.page][i];
        v[i] = (uint8_t)clamp(v[i] + (steps ? d : 2 * d), 0, steps ? steps : 100);
        if (set.page == PG_BEAT)
            ms_beat(i, v[i]);
        else if (set.page == PG_KEYS)
            ms_keys(i, v[i]);
        else if (set.page == PG_SHAPE)
            ms_loop_set(i, v[i]);
        else if (set.page == PG_TAPE)
            tape_send(i);
        else if (set.page == PG_MOTION)
            ms_motion(i, v[i]);
        else if (set.page == PG_EVOLVE) {
            ms_evolve(i, v[i]);
            if (!v[0] && !v[2] && !v[3]) {
                app_say("EVOLVE OFF", "SCENES STAY AS SET");
                set_dirty = 1;
                return;
            }
        }
        else if (set.page == PG_CHORDS) {
            if (i == 2 && v[0]) {        /* a held chord would keep the new path from being heard */
                v[0] = 0;
                ms_chords(0, 0);
            }
            ms_chords(i, v[i]);
        } else if (set.page == PG_PLAY) {
            play_send(i);
            if (i == 0)
                ui.play_shown = 0;
        } else if (set.page == PG_BASS) {
            bassx_send(i);
            if (i == 0)
                ms_beat(9, set.bassx[0] + 1);
        }
        else
            ms_fx(i, v[i]);
    } else if (i == 0) {
        set.tempo[m] = (uint8_t)clamp(set.tempo[m] + d, 40, 160);
        ms_tempo(set.tempo[m]);
        ms_fx(1, set.fx[1]);             /* delay time is tempo synced */
    } else if (i == 1) {
        set.root = (uint8_t)(((int)set.root + d % 12 + 12) % 12);
        ms_root(set.root);
    } else if (i == 2) {
        set.mood[m] = (uint8_t)clamp(set.mood[m] + d, 0, MS_NMOODS - 1);
        ms_mood(set.mood[m]);
    } else {
        sleep_set(clamp(set.sleep + d, 0, (int)NSLEEP - 1));
    }
    set_dirty = 1;
    if (set.page == PG_BEAT && i == 1 && set.beat[m][0] == 0)
        app_say(MS_PATTERN_NAME[set.beat[m][1]], "PICK A KIT FIRST");
    else
        say_knob(i);
}

static void space_step(int32_t d)
{
    set.space = (uint8_t)((set.space + d % MS_NSPACES + MS_NSPACES) % MS_NSPACES);
    set_dirty = 1;
    ms_space(set.space);
    app_say("SPACE", MS_SPACE_NAME[set.space]);
}

static void scene_step(int32_t d)
{
    int total = 0, g = 0, m;
    for (m = 0; m < MS_NMODES; m++) {
        if (m == set.mode)
            g = total + set.scene[m];
        total += MS_NSCENES[m];
    }
    g = ((g + d) % total + total) % total;
    for (m = 0; g >= MS_NSCENES[m]; m++)
        g -= MS_NSCENES[m];
    set.mode = (uint8_t)m;
    set.scene[m] = (uint8_t)g;
    scene_recipe(m);
    set_dirty = 1;
    send_all();
    ui.label_until = plat_ms() + 3000u;
    app_say(MS_MODE_NAME[m], ms_scene(m, set.scene[m])->name);
}

/* the loop, chord and beat pages live on their own screens: going to one opens it, going to a page of the sound goes back
 * to the screen it was opened from */
static void page_set(int p)
{
    int v = page_view(p);
    if (v >= 0 && set.view != v) {
        if (!own_view(set.view)) {
            ui.view_before = set.view;
            ui.page_before = set.page;
        }
        set.view = (uint8_t)v;
    } else if (v < 0 && own_view(set.view)) {
        set.view = own_view(ui.view_before) ? VIEW_STUDIO : ui.view_before;
    }
    set.page = (uint8_t)p;
    if (chord_page(p))
        ui.chord_tab = (uint8_t)p;
    if (beat_page(p))
        ui.beat_tab = (uint8_t)p;
    ui.confirm[0] = ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
    ui.confirm_t = 0;
    ui.redraw = 1;
    app_say(PAGE_NAME[p], "");
}

/* settings and the loop library to the flash: the sound stops for a moment while it writes, so only on SAVE */
static void save(void)
{
    int ok;
    set.seed = set.seed * 1664525u + 1013904223u + plat_ms();
    ok = plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0;
    if (ok && ui.lib_dirty)
        ok = plat_store_save(OBJ_SLOTS, &slots, sizeof slots) == 0;
    if (ok) {
        set_dirty = 0;
        ui.lib_dirty = 0;
        app_say("SAVED", "SETTINGS AND LOOPS");
    } else {
        app_say("SAVE", "FAILED");
    }
}

static void sleep_tick(void)
{
    uint32_t total, el;
    float left;
    if (!ui.sleep_start || !set.sleep)
        return;
    total = SLEEP_MIN[set.sleep] * 60000u;
    el = (uint32_t)fm_maxf((float)since(plat_ms(), ui.sleep_start), 0.0f);
    if (el >= total) {                   /* fade stays at 0 until PLAY so nothing swells back, a freeze included */
        ms_play(0);
        ms_freeze(0);
        ui.playing = 0;
        ui.frozen = 0;
        set.sleep = 0;
        ui.sleep_start = 0;
        ms_sleep_fade(0.0f);
        app_say("GOODNIGHT", "");
        return;
    }
    /* fade over the last third, at most five minutes */
    left = (float)(total - el);
    {
        float fade = fm_minf((float)total / 3.0f, 300000.0f);
        ms_sleep_fade(left < fade ? left / fade : 1.0f);
    }
}

static void lib_say(int i, const char *what)
{
    char t[8] = "LOOP ";
    t[5] = (char)('1' + i);
    t[6] = 0;
    app_say(t, what);
}

static int xfer_start(void)
{
    if (ui.xfer_busy)
        return 0;
    ui.xfer_busy = 1;
    ui.xfer_wait = ms_vis.xfer_seq;
    ui.xfer_t = plat_ms();
    return 1;
}

static void lib_load(int i)
{
    if (!slots.s[i].n) {
        lib_say(i, "EMPTY");
        return;
    }
    if (ui.lib_cur == i && ms_vis.lp_state != MS_LOOP_EMPTY) {
        lib_say(i, "PLAYING");
        return;
    }
    if (!xfer_start()) {
        app_say("LIBRARY", "BUSY");
        return;
    }
    ms_slot_xfer = slots.s[i];
    if (ms_loop_load() != 0) {
        ui.xfer_busy = 0;
        return;
    }
    ui.lib_load_idx = (int8_t)i;
    lib_say(i, "PLAYING");
}

static void lib_delete(int i)
{
    memset(&slots.s[i], 0, sizeof slots.s[i]);
    if (ui.lib_cur == i)
        ui.lib_cur = -1;
    ui.lib_dirty = 1;
    lib_say(i, "DELETED");
}

/* the library follows the loop by itself: a new loop goes in at the top (the rest move down, the eighth drops off),
 * and every few seconds the playing loop's entry is brought up to date. Only RAM: SAVE writes it to the flash */
static void lib_tick(uint32_t now)
{
    if (ms_vis.lp_gen != ui.lib_gen) {
        ui.lib_gen = ms_vis.lp_gen;
        if (ui.lib_load_idx >= 0) {
            ui.lib_cur = ui.lib_load_idx;
            ui.lib_load_idx = -1;
        } else {
            int i;
            for (i = NSLOTS - 1; i > 0; i--)
                slots.s[i] = slots.s[i - 1];
            memset(&slots.s[0], 0, sizeof slots.s[0]);
            ui.lib_cur = 0;
            ui.sel_slot = 0;
            if (set.page == PG_LIBRARY && ui.confirm[3]) {   /* the entries moved: never delete the wrong one */
                ui.confirm[3] = 0;
                ui.confirm_t = 0;
            }
        }
        ui.lib_snap_t = now - 2500u;     /* its first copy soon */
    }
    if (ms_vis.lp_state == MS_LOOP_EMPTY)
        ui.lib_cur = -1;
    if (ui.lib_cur >= 0 && ms_vis.lp_state == MS_LOOP_PLAY && now - ui.lib_snap_t >= 3000u && xfer_start()) {
        ui.lib_snap_t = now;
        ui.slot_save = ui.lib_cur;
        if (ms_loop_snap() != 0) {
            ui.xfer_busy = 0;
            ui.slot_save = -1;
        }
    }
}

/* 7 bytes in 8, the high bits first, as SysEx carries them */
_Static_assert(sizeof(ms_slot_ev_t) == 6 && offsetof(ms_slot_t, ev) == 8, "the slot layout the tape deck page reads");

static void sysex_put7(const uint8_t *p, uint32_t n)
{
    uint8_t b[8];
    uint32_t i, j;
    for (i = 0; i < n; i += 7) {
        uint32_t m = n - i < 7u ? n - i : 7u;
        b[0] = 0;
        for (j = 0; j < m; j++) {
            b[0] |= (uint8_t)((p[i + j] >> 7) << j);
            b[j + 1] = p[i + j] & 0x7Fu;
        }
        plat_sysex_put(b, m + 1);
    }
}

static void export_slot(int idx, const ms_slot_t *s)
{
    uint8_t head[6] = {0xF0, 0x7D, 0x4D, 0x4F, 0x02, (uint8_t)idx}, end = 0xF7;
    uint32_t n = s->n > MS_SLOT_EVENTS ? MS_SLOT_EVENTS : s->n;
    if (plat_sysex_begin() != 0)
        return;
    plat_sysex_put(head, sizeof head);
    sysex_put7((const uint8_t *)s, 8u + n * (uint32_t)sizeof(ms_slot_ev_t));
    plat_sysex_put(&end, 1);
    plat_sysex_end();
}

static int slot_same(const ms_slot_t *a, const ms_slot_t *b)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    uint32_t i;
    for (i = 0; i < sizeof *a; i++)
        if (x[i] != y[i])
            return 0;
    return 1;
}

/* a snapshot or load finished: store it, or send everything out */
static void xfer_done(void)
{
    if (!ui.xfer_busy)
        return;
    if (ms_vis.xfer_seq == ui.xfer_wait) {
        if (plat_ms() - ui.xfer_t > 300u) {   /* the render never answered: free the slots rather than wait forever */
            ui.xfer_busy = 0;
            ui.slot_save = -1;
            ui.export_state = 0;
        }
        return;
    }
    __asm__ volatile("" ::: "memory");   /* ms_slot_xfer is read only after the render said it is done */
    ui.xfer_busy = 0;
    if (ui.slot_save >= 0) {
        int i = ui.slot_save;
        ui.slot_save = -1;
        if (ms_slot_xfer.n && !slot_same(&slots.s[i], &ms_slot_xfer)) {
            slots.s[i] = ms_slot_xfer;
            ui.lib_dirty = 1;
        }
    } else if (ui.export_state == 2) {
        static const uint8_t done[6] = {0xF0, 0x7D, 0x4D, 0x4F, 0x03, 0xF7};
        int i;
        for (i = 0; i < NSLOTS; i++)
            export_slot(i, &slots.s[i]);
        export_slot(NSLOTS, &ms_slot_xfer);   /* the loop playing now */
        plat_sysex_send(done, sizeof done);
        ui.export_state = 0;
        app_say("LOOPS", "SENT");
    }
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n < 4 || p[0] != 0x7Du || p[1] != 0x4Du || p[2] != 0x4Fu)
        return 0;
    if (p[3] == 0x01u && !ui.export_state)
        ui.export_state = 1;
    return 1;
}

static void export_tick(void)
{
    if (ui.export_state == 1 && !ui.xfer_busy && xfer_start()) {
        ui.export_state = 2;
        if (ms_loop_snap() != 0) {
            ui.xfer_busy = 0;
            ui.export_state = 1;
        }
    }
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k, now = plat_ms();
    int32_t e;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        int note = 53 + (int)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit) && (set.page == PG_DRUMS || set.page == PG_BASS)) {
            ui.key_note[k] = 255u;       /* a step key: no note to end */
            if (set.page == PG_DRUMS)
                drums_key((int)k);
            else
                bass_key((int)k);
        } else if ((keys & bit) && !(ui.keys & bit) && set.page == PG_PLAY) {
            ui.key_note[k] = (uint8_t)note;
            ui.key_chord |= bit;
            ui.play_shown = 1;
            ms_chord_note(note, 96);
            plat_midi_out(0x09u | 0x90u << 8 | (uint32_t)note << 16 | 96u << 24);
        } else if ((keys & bit) && !(ui.keys & bit) && (set.page == PG_CHORDS || set.page == PG_BUILD)) {
            ui.key_note[k] = 255u;       /* a chord key: no note to end */
            if (set.page == PG_BUILD)
                build_key(note, now);
            else
                chord_key(note, now);
        } else if ((keys & bit) && !(ui.keys & bit)) {
            ui.key_note[k] = (uint8_t)note;
            if (set.keys[3] == 2)        /* HOLD: say what the tap does before the engine does it */
                app_say(MS_NOTE_NAME[note % 12], ms_vis.hold[note >> 5] >> (note & 31) & 1u ? "PULLED" : "PLANTED");
            ms_note(note, 96);
            plat_midi_out(0x09u | 0x90u << 8 | (uint32_t)note << 16 | 96u << 24);
        } else if (!(keys & bit) && (ui.keys & bit) && ui.key_note[k] != 255u) {
            note = ui.key_note[k];
            if (ui.key_chord & bit)
                ms_chord_note(note, 0);
            else
                ms_note(note, 0);
            ui.key_chord &= ~bit;
            plat_midi_out(0x08u | 0x80u << 8 | (uint32_t)note << 16);
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        switch (k) {
        case B_PLAY:
            ui.playing = !ui.playing;
            ms_play(ui.playing);
            if (ui.playing && !set.sleep)
                ms_sleep_fade(1.0f);
            app_say(ui.playing ? "PLAY" : "REST", "");
            break;
        case B_REC:
            rec_press();
            break;
        case B_EDIT:
            ms_seed(now);
            ms_new();
            app_say("NEW VARIATION", "");
            break;
        case B_SAVE:
            ui.save_t = now | 1u;
            ui.save_long = 0;
            break;
        case B_HOME:
            page_set(PG_HOME);
            break;
        case B_SEQ:
            ui.seq_t = now | 1u;
            break;
        case B_SEL:
            page_set(set.page == PG_MIX ? (beat_page(ui.beat_tab) ? ui.beat_tab : PG_BEAT) : PG_MIX);
            break;
        case B_ENV:                      /* again on the KEYS page: PLAY, LISTEN, HOLD in turn */
            if (set.page == PG_KEYS) {
                static const char *const HINT[3] = {"KEYS JUST PLAY", "MUSIC FOLLOWS YOU", "TAP TO PLANT A NOTE"};
                set.keys[3] = (uint8_t)((set.keys[3] + 1u) % 3u);
                ms_keys(3, set.keys[3]);
                set_dirty = 1;
                app_say(TAP_NAME[set.keys[3]], HINT[set.keys[3]]);
            } else {
                page_set(PG_KEYS);
            }
            break;
        case B_LFO:
            page_set(set.page == PG_LOOP ? PG_SHAPE : set.page == PG_SHAPE ? PG_TAPE : set.page == PG_TAPE ? PG_LIBRARY : PG_LOOP);
            break;
        case B_FX:
            page_set(set.page == PG_FX ? PG_MOTION : set.page == PG_MOTION ? PG_EVOLVE : PG_FX);
            break;
        case B_GLO:
            ui.glo_t = now | 1u;
            ui.glo_warned = 0;
            break;
        case B_ARP:
            ui.frozen = !ui.frozen;
            ms_freeze(ui.frozen);
            app_say("FREEZE", ui.frozen ? "ON" : "OFF");
            break;
        case B_OCTDN:
        case B_OCTUP: {
            char t[4] = {0};
            set.octave = (int8_t)clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
            t[0] = set.octave < 0 ? '-' : '+';
            t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
            set_dirty = 1;
            app_say("OCTAVE", t);
            break;
        }
        }
    }
    if (ui.seq_t && !(btn >> B_SEQ & 1u)) {   /* SEQ: a press switches the view; held with keys, loads slots */
        ui.seq_t = 0;
        {
            static const char *const VIEW_NAME[NVIEWS] = {"PICTURE", "STUDIO", "LOOPS", "CHORDS", "BEAT"};
            static const char *const VIEW_HINT[NVIEWS] = {"", "", "TURN SELECT FOR MORE", "TURN SELECT FOR MORE",
                                                          "TURN SELECT FOR MORE"};
            int next = (set.view + 1) % NVIEWS;
            if (own_view(next)) {        /* the LOOPS, CHORDS and BEAT screens have their own pages only */
                if (!own_view(set.view)) {
                    ui.view_before = set.view;
                    ui.page_before = set.page;
                }
                set.page = (uint8_t)(next == VIEW_CHORDS && chord_page(ui.chord_tab) ? ui.chord_tab
                                     : next == VIEW_BEAT && beat_page(ui.beat_tab) ? ui.beat_tab : view_first(next));
            } else if (own_view(set.view)) {
                set.page = page_view(ui.page_before) >= 0 ? PG_HOME : ui.page_before;
            }
            set.view = (uint8_t)next;
            ui.confirm[0] = ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
            set_dirty = 1;
            ui.label_until = now + 3000u;
            app_say(VIEW_NAME[set.view], VIEW_HINT[set.view]);
        }
    }
    if (ui.save_t) {                     /* SAVE: a tap saves; held two seconds and let go, resets the scene */
        if (btn >> B_SAVE & 1u) {
            if (!ui.save_long && since(now, ui.save_t) >= 2000) {
                ui.save_long = 1;
                app_say("HOLDING", "LET GO TO RESET SCENE");
            } else if (ui.save_long == 1 && since(now, ui.save_t) >= 6000) {
                ui.save_long = 2;
                app_say("HOLDING", "LET GO TO RESET ALL");
            }
        } else {
            ui.save_t = 0;
            if (ui.save_long == 2) {     /* every setting as it first starts; the loop library stays */
                settings_defaults();
                ui.sel_chord = 0;
                ui.build_t = 0;
                sleep_set(0);                /* a fade under way would otherwise stay down */
                ms_loop(1);
                ms_panic();
                send_all();
                ui.frozen = 0;
                ms_freeze(0);
                set_dirty = 1;
                page_set(PG_HOME);
                app_say("EVERYTHING RESET", "SAVE TO KEEP IT");
            } else if (ui.save_long) {
                scene_recipe(set.mode);
                send_all();
                set_dirty = 1;
                app_say("SCENE RESET", ms_scene(set.mode, set.scene[set.mode])->name);
            } else {
                save();
            }
        }
    }
    if (ui.glo_t) {                      /* GLO: a tap takes out; held, it warns, then clears the loop */
        int32_t held = since(now, ui.glo_t);
        if (btn >> B_GLO & 1u) {
            if (held >= (int32_t)GLO_HOLD) {
                ui.glo_t = 0;
                if (ms_vis.lp_state == MS_LOOP_EMPTY) {
                    app_say("NO LOOP TO CLEAR", "");
                } else {
                    ms_loop(1);
                    ui.hist_n = 0;
                    app_say("LOOP CLEARED", ui.lib_cur >= 0 ? "STILL IN THE LIBRARY" : "");
                }
            } else if (held >= 300 && !ui.glo_warned) {
                ui.glo_warned = 1;
                app_say("KEEP HOLDING TO CLEAR", "");
            }
        } else {
            ui.glo_t = 0;
            if (held < 300)              /* let go early from a hold: nothing */
                glo_tap(now);
            else
                app_say("NOTHING CLEARED", "");
        }
    }
    if ((e = plat_enc(EN_ALGO)) != 0)
        space_step(e);
    if ((e = plat_enc(EN_PRESET)) != 0)
        scene_step(e);
    if ((e = plat_enc(EN_SELECT)) != 0) {
        int p = set.page, n = e > 0 ? e : -e, dir = e > 0 ? 1 : -1, v = own_view(set.view) ? set.view : -1;
        while (n--)                      /* round the pages of this screen only */
            do
                p = (p + dir + NPAGES) % NPAGES;
            while (page_view(p) != v);
        page_set(p);
    }
    for (k = 0; k < 4u; k++)
        if ((e = plat_enc(EN_K1 + (int)k)) != 0)
            knob_turn((int)k, e);
    if (btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    {
        uint32_t lp = ms_vis.lp_state, lit;
        int loop_lit = lp == MS_LOOP_PLAY ? (ms_vis.lp_keep || (now / 700u & 1u))
                     : (lp == MS_LOOP_REC || lp == MS_LOOP_DUB) && (now / 200u & 1u);
        lit = (ui.playing ? 1u << B_PLAY : 0u) | (ui.frozen ? 1u << B_ARP : 0u) | (set_dirty || ui.lib_dirty ? 1u << B_SAVE : 0u)
            | (loop_lit ? 1u << B_REC : 0u) | 1u << PAGE_BTN[set.page] | btn;
        uint32_t chord = 0, held = 0, mask = ms_vis.chord_mask;
        for (k = 0; k < NKEYS; k++) {
            uint32_t note = (uint32_t)(53 + (int)k + 12 * set.octave);
            if (mask >> ((53u + k) % 12u) & 1u)
                chord |= 1u << k;
            if (set.keys[3] == 2 && (ms_vis.hold[note >> 5] >> (note & 31u) & 1u))   /* HOLD: the seeds */
                held |= 1u << k;
        }
        plat_lights(lit, keys | held, 0, set.keys[3] == 2 ? 0u : chord, 1);
    }
}

void app_input_only(void) { input(); }

static void track_events(uint32_t now)
{
    if (ms_vis.flashes != ui.last_flash) {
        ui.last_flash = ms_vis.flashes;
        ui.flash_at = now;
    }
    if (ms_vis.calls != ui.last_calls) {
        ui.last_calls = ms_vis.calls;
        ui.calls_at = now;
    }
    if (ms_vis.chirps != ui.last_chirps) {
        ui.last_chirps = ms_vis.chirps;
        ui.chirp_at = now;
    }
}

#include "draw.c"
#include "scene.c"
#include "studio.c"

static void speak(void)
{
    char t[96];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, APP_NAME ". ");
        text_cat(t, MS_MODE_NAME[set.mode]);
        text_cat(t, " ");
        text_cat(t, ms_scene(set.mode, set.scene[set.mode])->name);
        text_cat(t, ". ");
        text_cat(t, PAGE_NAME[set.page]);
        text_cat(t, ". CPU ");             /* the load and the late blocks so far: for checking a build on the FM-1 */
        int_text((int32_t)plat_cpu_pct(), t + text_len(t));
        text_cat(t, ". LATE ");
        int_text((int32_t)plat_xruns(), t + text_len(t));
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

void app_ui_init(void)
{
    static int started;
    if (!started) {
        int i;
        for (i = 0; i < MS_NLAYERS; i++)
            ui.track_lvl[i] = LAYER_LVL;
        ui.slot_save = -1;
        ui.lib_cur = ui.lib_load_idx = -1;
        ui.lib_gen = ms_vis.lp_gen;                      /* also called after an aborted update: keep play and sleep state */
        started = 1;
        ui.playing = 1;
        if (set.sleep)
            sleep_set(set.sleep);
    }
    ui.label_until = plat_ms() + 3000u;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_sec;
    uint32_t now = plat_ms();
    input();
    speak();
    sleep_tick();
    if (ui.msg_until && (int32_t)(now - ui.msg_until) >= 0) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if ((set.sleep || set.page == PG_SETUP) && now / 1000u != last_sec) {
        last_sec = now / 1000u;
        ui.redraw = 1;
    }
    track_events(now);
    tracks_tick();
    xfer_done();
    lib_tick(now);
    if (ui.confirm_t && since(now, ui.confirm_t) > (int32_t)CONFIRM_WAIT) {   /* asked and left alone: let go */
        ui.confirm_t = 0;
        if (ui.confirm[3])               /* still asking: else it was let go another way */
            app_say("NOTHING DONE", "");
        ui.confirm[0] = ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
    }
    build_tick(now);
    export_tick();
    if (ui.redraw || now - ui.vis_at >= 50u) {   /* 20 fps */
        ui.redraw = 0;
        ui.vis_at = now;
        if (set.view == VIEW_SCENE)
            scene_draw(now);
        else
            studio_draw(now);
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "mode"))
        *got = set.mode;
    else if (!strcmp(what, "scene"))
        *got = set.scene[set.mode];
    else if (!strcmp(what, "page"))
        *got = set.page;
    else if (!strcmp(what, "view"))
        *got = set.view;
    else if (!strcmp(what, "tap"))
        *got = set.keys[3];
else if (!strcmp(what, "held"))
        *got = __builtin_popcount(ms_vis.hold[0]) + __builtin_popcount(ms_vis.hold[1]) + __builtin_popcount(ms_vis.hold[2])
             + __builtin_popcount(ms_vis.hold[3]);
    else if (!strcmp(what, "libcur"))
        *got = ui.lib_cur;
    else if (!strncmp(what, "slot", 4))   /* slotN: how many notes library entry N holds */
        *got = slots.s[atoi(what + 4) % NSLOTS].n;
    else if (!strcmp(what, "minen"))
        *got = set.mine_n;
    else if (!strcmp(what, "built"))
        *got = set.b_n;
    else if (!strcmp(what, "chordslot"))
        *got = ms_vis.chord_slot;
    else if (!strcmp(what, "liveroot"))
        *got = ms_vis.live_root;
    else if (!strcmp(what, "livekind"))
        *got = ms_vis.live_kind;
    else if (!strcmp(what, "path"))
        *got = set.chords[2];
    else if (!strcmp(what, "chordroot"))
        *got = ms_vis.chord_root;
    else if (!strcmp(what, "chordkind"))
        *got = ms_vis.chord_kind;
    else if (!strcmp(what, "redo"))
        *got = ms_vis.lp_redo;
    else if (!strcmp(what, "layer"))
        *got = ui.sel_track;
    else if (!strcmp(what, "keep"))
        *got = ms_vis.lp_keep;
    else if (!strcmp(what, "dots0"))      /* track 1's notes, bit per 32nd of the loop */
        *got = (int)ms_vis.lp_dots[0];
    else if (!strcmp(what, "bars1000"))  /* the loop's length in bars, times 1000 */
        *got = (int)(ms_vis.lp_len * (float)set.tempo[set.mode] / 240.0f * 1000.0f + 0.5f);
    else if (!strcmp(what, "tracks"))
        *got = ms_vis.lp_tracks;
    else if (!strcmp(what, "muted"))
        *got = ms_vis.lp_mute;
    else if (!strcmp(what, "kit"))
        *got = set.beat[set.mode][0];
    else if (!strcmp(what, "pattern"))
        *got = set.beat[set.mode][1];
    else if (!strcmp(what, "bass"))
        *got = set.beat[set.mode][3];
    else if (!strncmp(what, "row", 3))    /* rowN: drum N's steps */
        *got = set.drum_row[atoi(what + 3) % MS_NDRUMS];
    else if (!strncmp(what, "bstep", 5))  /* bstepN: the bass note on sixteenth N */
        *got = set.bass_line[atoi(what + 5) & 15];
    else if (!strcmp(what, "space"))
        *got = set.space;
    else if (!strcmp(what, "loop"))
        *got = ms_vis.lp_state;
    else if (!strcmp(what, "loopn"))
        *got = ms_vis.lp_n;
    else if (!strcmp(what, "playing"))
        *got = ms_vis.playing;
    else if (!strcmp(what, "frozen"))
        *got = ms_vis.frozen;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "root"))
        *got = set.root;
    else if (!strcmp(what, "sleep"))
        *got = set.sleep;
    else if (!strcmp(what, "notes"))
        *got = (int)ms_vis.notes;
    else if (!strcmp(what, "flashes"))
        *got = (int)ms_vis.flashes;
    else if (!strcmp(what, "calls"))
        *got = (int)ms_vis.calls;
    else if (!strncmp(what, "macro", 5))
        *got = set.macro[set.mode][atoi(what + 5) & 3];
    else if (!strncmp(what, "mix", 3))
        *got = set.mix[set.mode][atoi(what + 3) & 3];
    else if (!strcmp(what, "tempo"))
        *got = set.tempo[set.mode];
    else
        return 0;
    return 1;
}
#endif
