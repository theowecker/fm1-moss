/* SPDX-License-Identifier: MPL-2.0 */
/* Part of fm1-base's unity build, included after the platform (gfx.c and announce.c are in scope).
 *
 *   PRESETS  scene (all modes in one list)   ALGORITHM  space   SELECT  page
 *   HOME SEL ENV LFO FX GLO  pages HOME, MIX, KEYS, LOOP, FX, SETUP   K1..K4  the page's knobs
 *   ARP  looper (hold: clear)   REC  freeze   EDIT  new variation   PLAY  play/stop   SEQ  scene/studio view
 *   SAVE  save (hold: reset the scene)   OCT- OCT+  keys' octave */
#include <stddef.h>
#include "fastmath.h"

#define SET_MAGIC 0x4D4F5333u            /* "MOS3" */
enum { PG_HOME, PG_MIX, PG_BEAT, PG_KEYS, PG_LOOP, PG_SHAPE, PG_TAPE, PG_SLOTS, PG_FX, PG_SETUP, NPAGES };
enum { VIEW_SCENE, VIEW_STUDIO, VIEW_LOOPS, NVIEWS };
static const char *const PAGE_NAME[NPAGES] = {"HOME", "MIX", "BEAT", "KEYS", "LOOP", "SHAPE", "TAPE", "SLOTS", "FX", "SETUP"};
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_SEL, B_SEL, B_ENV, B_LFO, B_LFO, B_LFO, B_LFO, B_FX, B_GLO};
static const char *const SNAP_NAME[3] = {"FREE", "SCALE", "CHORD"};
static const char *const VOICE_NAME[5] = {"AUTO", "EP", "BELL", "PAD", "KALIMBA"};
static const char *const TAP_NAME[3] = {"PLAY", "LISTEN", "HOLD"};
static const char *const SPEED_NAME[5] = {"1/2", "1x", "2x", "REV", "REV 1/2"};
static const char *const PAGE_KNOBS[NPAGES][4] = {
    {0}, {0}, {"KIT", "PATTERN", "LEVEL", "BASS"}, {"SNAP", "HARMONY", "VOICE", "TAP"}, {"TRACK", "LEVEL", "SHIFT", "ERASE"},
    {"FADE", "MUTATE", "SMEAR", "SPEED"}, {"WARBLE", "DIRT", "AGE", "SEND"}, {"SLOT", "SAVE", "LOAD", "DELETE"},
    {"DELAY", "TIME", "REPEATS", "FILM"}, {"TEMPO", "KEY", "MOOD", "SLEEP"},
};
/* max value of a stepped knob; 0 means a 0..100 knob */
static const uint8_t PAGE_STEPS[NPAGES][4] = {{0}, {0}, {MS_NKITS - 1, MS_NPATTERNS - 1, 0, MS_NBASS - 1}, {2, 0, 4, 2}, {0}, {0, 0, 0, 4}, {0}, {0}, {0, 5, 0, 0}, {0}};
/* a knob that acts (SAVE, LOAD, ERASE, DELETE) does so once turned right this many detents: an accident takes more
 * than a nudge, and the gauge shows how close it is */
#define CONFIRM_SHORT 2
#define CONFIRM_LONG 6
static void slot_store(int i);
static void slot_recall(int i);
static void slot_delete(int i);
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
    uint8_t keys[4], loop[4], fx[4], tape[4];
    uint8_t beat[MS_NMODES][4];          /* kit, pattern, level, bass */
    uint8_t rsv[8];
} settings_t;
static settings_t set;

#define SLOTS_MAGIC 0x534C4F31u          /* "SLO1" */
#define NSLOTS 8
static const uint8_t SLOT_KEY[NSLOTS] = {0, 2, 4, 6, 7, 9, 11, 12};   /* the first eight white keys */
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
    set.tape[3] = 50;
    set.fx[0] = 30, set.fx[1] = 2, set.fx[2] = 45, set.fx[3] = 20;
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
            || !knob_ok(PG_TAPE, m, set.tape[m]))
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

static void send_all(void)
{
    int i;
    send_mode();
    for (i = 0; i < 4; i++) {
        ms_keys(i, set.keys[i]);
        ms_loop_set(i, set.loop[i]);
        ms_loop_fx(i, set.tape[i]);
        ms_fx(i, set.fx[i]);
    }
    ms_space(set.space);
    for (i = 0; i < 4; i++)
        ms_beat(i, set.beat[set.mode][i]);
}

void app_init(void)
{
    ms_init();
    if (plat_store_size(OBJ_SETTINGS) != (int)sizeof set || plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set
        || !settings_ok())
        settings_defaults();
    if (plat_store_size(OBJ_SLOTS) != (int)sizeof slots || plat_store_load(OBJ_SLOTS, &slots, sizeof slots) != (int)sizeof slots
        || slots.magic != SLOTS_MAGIC) {
        memset(&slots, 0, sizeof slots);
        slots.magic = SLOTS_MAGIC;
    }
    {
        int i;
        for (i = 0; i < NSLOTS; i++)
            if (slots.s[i].n > MS_SLOT_EVENTS)
                slots.s[i].n = 0;
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
    uint32_t save_t, arp_t;
    uint8_t key_note[NKEYS];             /* note sent at key down, so the note-off matches after an octave change */
    uint8_t track_lvl[4];                /* the LOOP page's levels; not saved, as the loop itself is not */
    uint8_t sel_track, sel_slot;
    uint8_t confirm[4];                  /* detents turned so far on an acting knob */
    uint32_t confirm_t;
    uint32_t seq_t;
    uint8_t save_used, seq_used;         /* a slot key was used during this SAVE or SEQ hold */
    uint8_t save_long;                   /* SAVE held long enough to reset on release */
    uint32_t slot_keys;                  /* keys pressed while SAVE or SEQ was held: they work the slots */
    uint8_t slot_mod[NSLOTS];            /* 1 SAVE, 2 SEQ: which one was held when the key went down */
    int8_t slot_save;                    /* the slot a snapshot is on its way to, -1 none */
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
         : page == PG_BEAT ? set.beat[set.mode] : 0;
}

static const char *knob_name(int i)
{
    if (set.page == PG_HOME)
        return MS_MACRO_NAME[set.mode][i];
    if (set.page == PG_MIX)
        return MS_MIX_NAME[set.mode][i];
    return PAGE_KNOBS[set.page][i];
}

static int confirm_need(int i) { return set.page == PG_SLOTS && i < 3 ? CONFIRM_SHORT : CONFIRM_LONG; }

static int knob_value(int i)   /* 0..100 */
{
    int m = set.mode, steps = PAGE_STEPS[set.page][i];
    uint8_t *v = page_vals(set.page);
    if (set.page == PG_LOOP)
        return i == 0 ? ui.sel_track * 100 / 3 : i == 1 ? ui.track_lvl[ui.sel_track]
             : i == 2 ? 50 : ui.confirm[3] * 100 / CONFIRM_LONG;
    if (set.page == PG_SLOTS)
        return i == 0 ? ui.sel_slot * 100 / 7 : ui.confirm[i] * 100 / confirm_need(i);
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

static void knob_text(int i, char *t)
{
    int m = set.mode;
    t[0] = 0;
    if (set.page == PG_LOOP || set.page == PG_SLOTS) {
        int sel = set.page == PG_LOOP ? ui.sel_track : ui.sel_slot;
        int used = set.page == PG_LOOP ? (int)(ms_vis.lp_tracks >> sel & 1u) : slots.s[sel].n > 0;
        if (i == 0) {
            int_text(sel + 1, t);
            text_cat(t, used ? "" : " EMPTY");
        } else if (set.page == PG_LOOP && i == 1) {
            if (ui.track_lvl[sel])
                int_text(ui.track_lvl[sel], t);
            else
                text_cat(t, "OFF");
        } else if (set.page == PG_LOOP && i == 2) {
            text_cat(t, "< >");
        } else {
            text_cat(t, ui.confirm[i] ? "MORE >" : "TURN >");
        }
        return;
    }
    if (set.page == PG_KEYS && i != 1)
        text_cat(t, i == 0 ? SNAP_NAME[set.keys[0]] : i == 2 ? VOICE_NAME[set.keys[2]] : TAP_NAME[set.keys[3]]);
    else if (set.page == PG_SHAPE && i == 3)
        text_cat(t, SPEED_NAME[set.loop[3]]);
    else if (set.page == PG_FX && i == 1)
        text_cat(t, MS_DIV_NAME[set.fx[1]]);
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

/* the LOOP and SLOTS pages: K1 picks a track or slot, the others set it or act on it; 1 = it spoke for itself */
static int pages_act(int i, int32_t d)
{
    int loop = set.page == PG_LOOP, sel = loop ? ui.sel_track : ui.sel_slot;
    if (i == 0) {
        if (loop)
            ui.sel_track = (uint8_t)clamp(ui.sel_track + d, 0, 3);
        else
            ui.sel_slot = (uint8_t)clamp(ui.sel_slot + d, 0, NSLOTS - 1);
        ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
        return 0;
    }
    if (loop && i == 1) {               /* all the way down is off */
        ui.track_lvl[sel] = (uint8_t)clamp(ui.track_lvl[sel] + 2 * d, 0, 100);
        ms_track(sel, ui.track_lvl[sel]);
        return 0;
    }
    if (loop && i == 2) {                /* SHIFT: the track a sixteenth later or earlier a click */
        int32_t n = d < 0 ? -d : d;
        if (!(ms_vis.lp_tracks >> sel & 1u)) {
            app_say("TRACK", "EMPTY");
            return 1;
        }
        while (n--)
            ms_track(sel, d > 0 ? -4 : -5);
        app_say("SHIFT", d > 0 ? "LATER" : "EARLIER");
        return 1;
    }
    ui.confirm_t = plat_ms();
    ui.confirm[i] = (uint8_t)clamp(ui.confirm[i] + d, 0, CONFIRM_LONG);
    if (ui.confirm[i] < confirm_need(i))
        return 0;
    ui.confirm[i] = 0;
    if (loop) {
        ms_track(sel, -2);
        app_say("TRACK", "ERASED");
    } else if (i == 1) {
        slot_store(sel);
    } else if (i == 2) {
        slot_recall(sel);
    } else {
        slot_delete(sel);
    }
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
    } else if (set.page == PG_LOOP || set.page == PG_SLOTS) {
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
            ms_loop_fx(i, v[i]);
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

static void page_set(int p)
{
    set.page = (uint8_t)p;
    ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
    ui.confirm_t = 0;
    ui.redraw = 1;
    app_say(PAGE_NAME[p], "");
}

static void save(void)
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
        set_dirty = 0;
        app_say("SAVED", "");
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

static int slot_of_key(uint32_t k)
{
    int i;
    for (i = 0; i < NSLOTS; i++)
        if (SLOT_KEY[i] == k)
            return i;
    return -1;
}

static void slot_say(int i, const char *what)
{
    char t[8] = "SLOT ";
    t[5] = (char)('1' + i);
    t[6] = 0;
    app_say(t, what);
}

static int xfer_start(void)
{
    if (ui.xfer_busy) {
        app_say("SLOTS", "BUSY");
        return 0;
    }
    ui.xfer_busy = 1;
    ui.xfer_wait = ms_vis.xfer_seq;
    ui.xfer_t = plat_ms();
    return 1;
}

static void slot_store(int i)
{
    if (!xfer_start())
        return;
    ui.slot_save = (int8_t)i;
    if (ms_loop_snap() != 0) {
        ui.xfer_busy = 0;
        ui.slot_save = -1;
        slot_say(i, "TRY AGAIN");
    }
}

static void slot_recall(int i)
{
    if (!slots.s[i].n) {
        slot_say(i, "EMPTY");
        return;
    }
    if (!xfer_start())
        return;
    ms_slot_xfer = slots.s[i];
    if (ms_loop_load() != 0) {
        ui.xfer_busy = 0;
        slot_say(i, "TRY AGAIN");
        return;
    }
    slot_say(i, "LOADED");
}

/* slot i becomes *v in the flash, and in RAM only if the flash took it */
static int slot_write(int i, const ms_slot_t *v)
{
    ms_slot_t old = slots.s[i];
    slots.s[i] = *v;
    if (plat_store_save(OBJ_SLOTS, &slots, sizeof slots) == 0)
        return 1;
    slots.s[i] = old;
    return 0;
}

static void slot_delete(int i)
{
    ms_slot_t empty;
    memset(&empty, 0, sizeof empty);
    slot_say(i, slot_write(i, &empty) ? "DELETED" : "NOT DELETED");
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
        if (!ms_slot_xfer.n) {
            slot_say(i, "NO LOOP YET");
            return;
        }
        slot_say(i, slot_write(i, &ms_slot_xfer) ? "SAVED" : "NOT SAVED");
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
        int note = 53 + (int)k + 12 * set.octave, sl = slot_of_key(k);
        int mod = (btn >> B_SAVE & 1u) ? 1 : (btn >> B_SEQ & 1u) ? 2 : 0;
        if ((keys & bit) && !(ui.keys & bit) && mod && sl >= 0) {
            ui.slot_keys |= bit;
            ui.slot_mod[sl] = (uint8_t)mod;
            if (mod == 1)
                ui.save_used = 1;
            else
                ui.seq_used = 1;
            continue;
        }
        if (ui.slot_keys & bit) {        /* SAVE + key saves there, SEQ + key loads: on release, so a slip can be undone */
            if (!(keys & bit)) {
                ui.slot_keys &= ~bit;
                if (ui.slot_mod[sl] == 1)
                    slot_store(sl);
                else
                    slot_recall(sl);
            }
            continue;
        }
        if ((keys & bit) && !(ui.keys & bit)) {
            ui.key_note[k] = (uint8_t)note;
            if (set.keys[3] == 2) {      /* HOLD: say what the tap does before the engine does it */
                int in = ms_vis.pool >> (note % 12) & 1u, last = in && !(ms_vis.pool & ~(1u << (note % 12)));
                app_say(MS_NOTE_NAME[note % 12], last ? "STAYS (LAST NOTE)" : in ? "TAKEN AWAY" : "ADDED");
            }
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
            ui.arp_t = now | 1u;
            break;
        case B_EDIT:
            ms_seed(now);
            ms_new();
            app_say("NEW", "VARIATION");
            break;
        case B_SAVE:
            ui.save_t = now | 1u;
            ui.save_used = 0;
            ui.save_long = 0;
            break;
        case B_HOME:
            page_set(PG_HOME);
            break;
        case B_SEQ:
            ui.seq_t = now | 1u;
            ui.seq_used = 0;
            break;
        case B_SEL:
            page_set(set.page == PG_MIX ? PG_BEAT : PG_MIX);
            break;
        case B_ENV:                      /* again on the KEYS page: PLAY, LISTEN, HOLD in turn */
            if (set.page == PG_KEYS) {
                static const char *const HINT[3] = {"KEYS JUST PLAY", "MUSIC FOLLOWS YOU", "ADD OR TAKE AWAY NOTES"};
                set.keys[3] = (uint8_t)((set.keys[3] + 1u) % 3u);
                ms_keys(3, set.keys[3]);
                set_dirty = 1;
                app_say(TAP_NAME[set.keys[3]], HINT[set.keys[3]]);
            } else {
                page_set(PG_KEYS);
            }
            break;
        case B_LFO:
            page_set(set.page == PG_LOOP ? PG_SHAPE : set.page == PG_SHAPE ? PG_TAPE : set.page == PG_TAPE ? PG_SLOTS : PG_LOOP);
            break;
        case B_FX:
            page_set(PG_FX);
            break;
        case B_GLO:
            page_set(PG_SETUP);
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
        if (!ui.seq_used) {
            set.view = (uint8_t)((set.view + 1u) % NVIEWS);
            set_dirty = 1;
            ui.label_until = now + 3000u;
            app_say(set.view == VIEW_SCENE ? "PICTURE" : set.view == VIEW_STUDIO ? "STUDIO" : "LOOPS", "");
        }
    }
    if (ui.save_t) {                     /* SAVE: a tap saves; held two seconds and let go, resets the scene */
        if (btn >> B_SAVE & 1u) {
            if (!ui.save_used && !ui.save_long && now - ui.save_t >= 2000u) {
                ui.save_long = 1;
                app_say("LET GO TO", "RESET SCENE");
            }
        } else {
            ui.save_t = 0;
            if (ui.save_used) {
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
                app_say("LOOP", "CLEARED");
            }
        } else {
            uint32_t st = ms_vis.lp_state;
            ui.arp_t = 0;
            if (set.keys[3] == 2 && st == MS_LOOP_EMPTY)   /* HOLD's notes are switches, not loopable */
                app_say("HOLD", "KEYS EDIT NOTES");
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
    if ((e = plat_enc(EN_SELECT)) != 0)
        page_set((set.page + e % NPAGES + NPAGES) % NPAGES);
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
        lit = (ui.playing ? 1u << B_PLAY : 0u) | (ui.frozen ? 1u << B_ARP : 0u) | (set_dirty ? 1u << B_SAVE : 0u)
            | (loop_lit ? 1u << B_REC : 0u) | 1u << PAGE_BTN[set.page] | btn;
        uint32_t chord = 0, held = 0, mask = ms_vis.chord_mask;
        for (k = 0; k < NKEYS; k++) {
            uint32_t note = (uint32_t)(53 + (int)k + 12 * set.octave);
            if (mask >> ((53u + k) % 12u) & 1u)
                chord |= 1u << k;
            if (set.keys[3] == 2 && (ms_vis.pool >> (note % 12u) & 1u))   /* HOLD: the notes the music plays */
                held |= 1u << k;
        }
        if (btn & (1u << B_SAVE | 1u << B_SEQ)) {
            uint32_t on = 0, i;
            for (i = 0; i < NSLOTS; i++)
                if (slots.s[i].n)
                    on |= 1u << SLOT_KEY[i];
            plat_lights(lit, on, 0, 0, 1);
        } else {
            plat_lights(lit, keys | held, 0, set.keys[3] == 2 ? 0u : chord, 1);
        }
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
        ui.track_lvl[0] = ui.track_lvl[1] = ui.track_lvl[2] = ui.track_lvl[3] = 70;
        ui.slot_save = -1;                      /* also called after an aborted update: keep play and sleep state */
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
    xfer_done();
    if (ui.confirm_t && now - ui.confirm_t > 1500u) {   /* a half turn left alone does not count later */
        ui.confirm_t = 0;
        ui.confirm[1] = ui.confirm[2] = ui.confirm[3] = 0;
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
    else if (!strcmp(what, "outside")) {   /* recent notes from the music (not the keys) that the HOLD set lacks */
        int i, n = 0;
        for (i = 0; i < 16; i++) {
            uint16_t r = ms_vis.recent[i];
            if ((r >> 8) != 4u && r && !(ms_vis.pool >> ((r & 0xFFu) % 12u) & 1u))
                n++;
        }
        *got = n;
    } else if (!strcmp(what, "held"))
        *got = __builtin_popcount(ms_vis.pool);
    else if (!strncmp(what, "slot", 4))   /* slotN: how many notes slot N holds */
        *got = slots.s[atoi(what + 4) % NSLOTS].n;
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
