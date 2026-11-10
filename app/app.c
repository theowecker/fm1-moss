/* SPDX-License-Identifier: MPL-2.0 */
/* Part of fm1-base's unity build, included after the platform (gfx.c and announce.c are in scope).
 *
 *   PRESETS  scene (all modes in one list)   ALGORITHM  space   SELECT  page
 *   HOME SEL ENV LFO FX GLO  pages HOME, MIX/BEAT, KEYS, LOOP..LIBRARY, FX/MOTION, SETUP   K1..K4  the page's knobs
 *   ARP  looper (hold: clear)   REC  freeze   EDIT  new variation   PLAY  play/stop   SEQ  scene/studio view
 *   SAVE  save (hold: reset the scene)   OCT- OCT+  keys' octave */
#include <stddef.h>
#include "fastmath.h"

#define SET_MAGIC 0x4D4F5335u            /* "MOS5" */
/* SELECT walks them in this order: the sound, its rhythm, the keys, the loops, then the setup */
enum { PG_HOME, PG_MIX, PG_FX, PG_MOTION, PG_BEAT, PG_KEYS, PG_LOOP, PG_SHAPE, PG_TAPE, PG_LIBRARY, PG_CHORDS, PG_SETUP,
       NPAGES };
enum { VIEW_SCENE, VIEW_STUDIO, VIEW_LOOPS, NVIEWS };
static const char *const PAGE_NAME[NPAGES] = {"HOME", "MIX", "FX", "MOTION", "BEAT", "KEYS", "LAYERS", "SHAPE", "TAPE", "LIBRARY",
                                              "CHORDS", "SETUP"};
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_SEL, B_FX, B_FX, B_SEL, B_ENV, B_LFO, B_LFO, B_LFO, B_LFO, B_GLO, B_GLO};
static const char *const CHANGE_NAME[4] = {"STILL", "SLOW", "NORMAL", "OFTEN"};
static const char *const PATH_NAME[6] = {"SCENE", "CIRCLE", "DREAM", "MOODY", "SONG", "DRONE"};
static const char *const COLOR_NAME[4] = {"LUSH", "PLAIN", "SUS", "OPEN"};
static const char *const SNAP_NAME[3] = {"FREE", "SCALE", "CHORD"};
static const char *const VOICE_NAME[5] = {"AUTO", "EP", "BELL", "PAD", "KALIMBA"};
static const char *const TAP_NAME[3] = {"PLAY", "LISTEN", "HOLD"};
static const char *const SPEED_NAME[5] = {"1/2", "1x", "2x", "REV", "REV 1/2"};
static const char *const PAGE_KNOBS[NPAGES][4] = {
    {0}, {0}, {"DELAY", "TIME", "REPEATS", "FILM"}, {"MOD", "RANDOM", "TONE", "SPREAD"}, {"KIT", "PATTERN", "LEVEL", "BASS"},
    {"SNAP", "HARMONY", "VOICE", "TAP"}, {"LAYER", "LEVEL", "REDO", "CLEAR"}, {"FADE", "MUTATE", "SHIFT", "SPEED"},
    {"WARBLE", "DIRT", "AGE", "SMEAR"}, {"LOOP", "", "", "DELETE"}, {"CHORD", "CHANGE", "PATH", "COLOR"},
    {"TEMPO", "KEY", "MOOD", "SLEEP"},
};
/* max value of a stepped knob; 0 means a 0..100 knob */
static const uint8_t PAGE_STEPS[NPAGES][4] = {
    {0}, {0}, {0, 5, 0, 0}, {0}, {MS_NKITS - 1, MS_NPATTERNS - 1, 0, MS_NBASS - 1}, {2, 0, 4, 2}, {0}, {0, 0, 0, 4}, {0}, {0}, {7, 3, 5, 3}, {0},
};
#define CONFIRM_WAIT 3000u               /* ms an armed CLEAR or DELETE waits for REC */
#define LAYER_LVL 70                     /* a layer's level when it is new */
static int loop_page(int p) { return p >= PG_LOOP && p <= PG_LIBRARY; }
static void lib_load(int i);
static void lib_delete(int i);
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
    uint8_t keys[4], loop[4], fx[4], tape[4], motion[4];
    uint8_t beat[MS_NMODES][4];          /* kit, pattern, level, bass */
    uint8_t chords[4];                   /* chord (0 AUTO, 1..7), change, path, color */
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
    set.chords[1] = 2;                    /* NORMAL */
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
            || !knob_ok(PG_TAPE, m, set.tape[m]) || !knob_ok(PG_MOTION, m, set.motion[m])
            || !knob_ok(PG_CHORDS, m, set.chords[m]))
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
        ms_chords(i, set.chords[i]);
    }
    ms_space(set.space);
    for (i = 0; i < 4; i++)
        ms_beat(i, set.beat[set.mode][i]);
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
    if (loop_page(set.page) != (set.view == VIEW_LOOPS))
        set.page = set.view == VIEW_LOOPS ? PG_LOOP : PG_HOME;
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
    uint32_t save_t, arp_t;
    uint8_t key_note[NKEYS];             /* note sent at key down, so the note-off matches after an octave change */
    uint8_t track_lvl[MS_NLAYERS];       /* the layers' levels; not saved, as the loop itself is not */
    uint8_t sel_track, sel_slot;
    uint8_t confirm[4];                  /* [3]: CLEAR or DELETE armed, waiting for REC */
    uint32_t confirm_t;
    uint32_t seq_t;
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
         : page == PG_MOTION ? set.motion : page == PG_CHORDS ? set.chords : page == PG_BEAT ? set.beat[set.mode] : 0;
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
             : i == 2 ? (ms_vis.lp_redo == ui.sel_track ? 100 : 0) : ui.confirm[3] * 50;
    if (set.page == PG_LIBRARY)
        return i == 0 ? ui.sel_slot * 100 / 7 : i == 3 ? ui.confirm[3] * 50 : 0;
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

static void chords_text(int i, char *t)
{
    static const char *const ROMAN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    const uint8_t *c = set.chords;
    if (i == 0 && c[0]) {
        text_cat(t, ROMAN[c[0] - 1]);
        text_cat(t, " ");
        text_cat(t, MS_NOTE_NAME[degree_pc(c[0] - 1)]);
    } else {
        text_cat(t, i == 0 ? "AUTO" : i == 1 ? CHANGE_NAME[c[1]] : i == 2 ? PATH_NAME[c[2]] : COLOR_NAME[c[3]]);
    }
}

static void knob_text(int i, char *t)
{
    int m = set.mode;
    t[0] = 0;
    if (set.page == PG_LOOP) {
        int sel = ui.sel_track, used = ms_vis.lp_tracks >> sel & 1u;
        if (i == 0) {
            int_text(sel + 1, t);
            text_cat(t, used ? "" : " EMPTY");
        } else if (i == 1) {
            if (ui.track_lvl[sel])
                int_text(ui.track_lvl[sel], t);
            else
                text_cat(t, "OFF");
        } else if (i == 2) {
            text_cat(t, ms_vis.lp_redo == sel ? "ARMED" : "TURN >");
        } else {
            text_cat(t, ui.confirm[3] ? "PRESS REC" : "TURN >");
        }
        return;
    }
    if (set.page == PG_LIBRARY) {
        if (i == 0) {
            int_text(ui.sel_slot + 1, t);
            text_cat(t, slots.s[ui.sel_slot].n ? "" : " EMPTY");
        } else if (i == 3) {
            text_cat(t, ui.confirm[3] ? "PRESS REC" : "TURN >");
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

/* CLEAR and DELETE: turning right only arms them, and REC does it, so no spin of a knob erases anything. Turning
 * left, waiting or changing page lets go */
static void arm_turn(int32_t d, const char *what, const char *armed)
{
    ui.confirm[3] = d > 0;
    ui.confirm_t = d > 0 ? plat_ms() : 0;
    app_say(what, d > 0 ? armed : "LET GO");
}

static void layer_clear(int t)
{
    char n[12];
    ms_track(t, -2);
    ui.track_lvl[t] = LAYER_LVL;
    ms_track(t, LAYER_LVL);
    layer_name(t, n);
    app_say(n, "CLEARED");
}

/* REC while CLEAR or DELETE is armed: do it. 1 = it was armed */
static int armed_rec(void)
{
    if (!ui.confirm[3] || (set.page != PG_LOOP && set.page != PG_LIBRARY))
        return 0;
    ui.confirm[3] = 0;
    ui.confirm_t = 0;
    if (set.page == PG_LOOP)
        layer_clear(ui.sel_track);
    else
        lib_delete(ui.sel_slot);
    return 1;
}

/* LAYERS works on one layer at a time: K1 picks it, K2 its level (down is OFF), K3 right arms REDO (the next
 * phrase you play replaces it; you hear the old one until then), K4 a full turn right clears it */
static int layer_turn(int i, int32_t d)
{
    char t[12];
    int sel = ui.sel_track, used = ms_vis.lp_tracks >> sel & 1u;
    if (i == 0) {
        ui.sel_track = (uint8_t)clamp(sel + d, 0, MS_NLAYERS - 1);
        ui.confirm[3] = 0;
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
    if (i == 2) {
        if (ms_vis.lp_state == MS_LOOP_EMPTY) {
            app_say("REDO", "NO LOOP YET");
            return 1;
        }
        ms_track(sel, d > 0 ? -6 : -7);
        app_say(t, d > 0 ? (used ? "PLAY TO REDO IT" : "PLAY TO FILL IT") : "REDO OFF");
        return 1;
    }
    if (!used) {
        app_say(t, "EMPTY");
        return 1;
    }
    arm_turn(d, t, "PRESS REC TO CLEAR");
    return 1;
}

/* a layer recorded where one was turned off or cleared starts at its usual level, so it is heard */
static void tracks_tick(void)
{
    uint8_t now = ms_vis.lp_tracks, born = (uint8_t)(now & ~ui.tracks_seen);
    int i;
    for (i = 0; i < MS_NLAYERS; i++)
        if (born >> i & 1u && ui.track_lvl[i] == 0) {
            ui.track_lvl[i] = LAYER_LVL;
            ms_track(i, LAYER_LVL);
        }
    ui.tracks_seen = now;
}

/* the LOOP page: a layer's level a knob (down is off); the LIBRARY page: K1 plays a loop, K4 deletes one; SHAPE's
 * SHIFT moves the whole loop. 1 = it spoke for itself */
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
        return 1;
    }
    if (i != 3)
        return 1;
    if (!slots.s[ui.sel_slot].n) {
        app_say("DELETE", "NOTHING THERE");
        return 1;
    }
    arm_turn(d, "DELETE", "PRESS REC TO DELETE");
    return 1;
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
        else if (set.page == PG_CHORDS)
            ms_chords(i, v[i]);
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

/* the loop pages live on the LOOPS screen: going to one opens it, going elsewhere goes back to where it was opened */
static void page_set(int p)
{
    if (loop_page(p) && set.view != VIEW_LOOPS) {
        ui.view_before = set.view;
        ui.page_before = set.page;
        set.view = VIEW_LOOPS;
    } else if (!loop_page(p) && set.view == VIEW_LOOPS) {
        set.view = ui.view_before == VIEW_LOOPS ? VIEW_STUDIO : ui.view_before;
    }
    set.page = (uint8_t)p;
    ui.confirm[0] = ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
    ui.confirm_t = 0;
    ui.redraw = 1;
    app_say(PAGE_NAME[p], "");
}

/* settings and the loop library to the flash: the sound stops for a moment while it writes, so only on SAVE */
static void save(void)
{
    int ok = plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0;
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
    el = plat_ms() - ui.sleep_start;
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
        if ((keys & bit) && !(ui.keys & bit)) {
            ui.key_note[k] = (uint8_t)note;
            if (set.keys[3] == 2)        /* HOLD: say what the tap does before the engine does it */
                app_say(MS_NOTE_NAME[note % 12], ms_vis.hold[note >> 5] >> (note & 31) & 1u ? "PULLED" : "PLANTED");
            ms_note(note, 96);
            plat_midi_out(0x09u | 0x90u << 8 | (uint32_t)note << 16 | 96u << 24);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            note = ui.key_note[k];
            ms_note(note, 0);
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
            if (!armed_rec())
                ui.arp_t = now | 1u;
            break;
        case B_EDIT:
            ms_seed(now);
            ms_new();
            app_say("NEW", "VARIATION");
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
            page_set(set.page == PG_MIX ? PG_BEAT : PG_MIX);
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
            page_set(set.page == PG_FX ? PG_MOTION : PG_FX);
            break;
        case B_GLO:
            page_set(set.page == PG_SETUP ? PG_CHORDS : PG_SETUP);
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
            int next = (set.view + 1) % NVIEWS;
            if (next == VIEW_LOOPS) {    /* the LOOPS screen has the loop pages only */
                ui.view_before = set.view;
                ui.page_before = set.page;
                set.page = PG_LOOP;
            } else if (set.view == VIEW_LOOPS) {
                set.page = loop_page(ui.page_before) ? PG_HOME : ui.page_before;
            }
            set.view = (uint8_t)next;
            ui.confirm[0] = ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
            set_dirty = 1;
            ui.label_until = now + 3000u;
            app_say(set.view == VIEW_SCENE ? "PICTURE" : set.view == VIEW_STUDIO ? "STUDIO" : "LOOPS",
                    set.view == VIEW_LOOPS ? "SELECT FOR MORE" : "");
        }
    }
    if (ui.save_t) {                     /* SAVE: a tap saves; held two seconds and let go, resets the scene */
        if (btn >> B_SAVE & 1u) {
            if (!ui.save_long && now - ui.save_t >= 2000u) {
                ui.save_long = 1;
                app_say("LET GO TO", "RESET SCENE");
            } else if (ui.save_long == 1 && now - ui.save_t >= 6000u) {
                ui.save_long = 2;
                app_say("LET GO TO", "RESET EVERYTHING");
            }
        } else {
            ui.save_t = 0;
            if (ui.save_long == 2) {     /* every setting as it first starts; the loop library stays */
                settings_defaults();
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
    if (ui.arp_t) {                      /* REC: records or closes a loop, keeps it or lets it fade; held, clears */
        if (btn >> B_REC & 1u) {
            if (now - ui.arp_t >= 1000u) {
                ui.arp_t = 0;
                ms_loop(1);
                app_say("CLEARED", ui.lib_cur >= 0 ? "IT STAYS IN THE LIBRARY" : "");
            }
        } else {
            uint32_t st = ms_vis.lp_state;
            ui.arp_t = 0;
            if (set.keys[3] == 2 && st == MS_LOOP_EMPTY)   /* HOLD's notes are planted, not looped */
                app_say("HOLD", "KEYS PLANT NOTES");
            else if (st == MS_LOOP_EMPTY)
                app_say("RECORDING", "TAP REC TO LOOP");
            else if (st == MS_LOOP_REC && !ms_vis.lp_n)
                app_say("NOTHING", "RECORDED");
            else if (st == MS_LOOP_PLAY)
                app_say(ms_vis.lp_keep ? "FADING" : "KEPT", ms_vis.lp_keep ? "TAP REC TO KEEP" : "HOLD REC TO CLEAR");
            else
                app_say("LOOPING", "TAP REC TO KEEP");
            if (!(set.keys[3] == 2 && st == MS_LOOP_EMPTY))
                ms_loop(0);
        }
    }
    if ((e = plat_enc(EN_ALGO)) != 0)
        space_step(e);
    if ((e = plat_enc(EN_PRESET)) != 0)
        scene_step(e);
    if ((e = plat_enc(EN_SELECT)) != 0) {
        if (set.view == VIEW_LOOPS) {
            int n = PG_LIBRARY - PG_LOOP + 1;
            page_set(PG_LOOP + ((set.page - PG_LOOP + e) % n + n) % n);
        } else {                         /* the loop pages are the LOOPS screen's own, so SELECT goes round them */
            int p = set.page, n = e > 0 ? e : -e, dir = e > 0 ? 1 : -1;
            while (n--)
                do
                    p = (p + dir + NPAGES) % NPAGES;
                while (loop_page(p));
            page_set(p);
        }
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
    if (ui.confirm_t && now - ui.confirm_t > CONFIRM_WAIT) {   /* armed and left alone: let go */
        ui.confirm_t = 0;
        ui.confirm[0] = ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
        ui.redraw = 1;
    }
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
