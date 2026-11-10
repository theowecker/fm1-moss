/* SPDX-License-Identifier: MPL-2.0 */
/* The STUDIO view: a circle of fifths with the chord and recent notes, layer levels on the left, a per-mode column
 * on the right, the page's knobs along the bottom. */

#define ST_BG RGB(16, 18, 28)
#define ST_LINE RGB(38, 42, 58)
#define ST_DIM RGB(70, 76, 96)
#define ST_TEXT RGB(236, 228, 206)
#define ST_SOFT RGB(150, 150, 168)

static const rgb_t MODE_ACCENT[MS_NMODES] = {CI(240, 160, 176), CI(150, 214, 150), CI(160, 200, 236)};
static const rgb_t LAYER_RGB[MS_NMODES][4] = {
    {CI(240, 160, 176), CI(240, 200, 120), CI(150, 170, 240), CI(130, 190, 220)},
    {CI(150, 214, 150), CI(240, 220, 130), CI(190, 160, 240), CI(240, 170, 190)},
    {CI(190, 160, 240), CI(160, 200, 236), CI(130, 150, 230), CI(170, 210, 190)},
};

#define WCX 118
#define WCY 106
#define WR 54

static struct {
    uint32_t seen;
    uint32_t at[16];                     /* plat_ms | 1 per ms_vis.recent slot, 0 unused */
} stn;

static void wheel_xy(int32_t pc, int32_t r, int32_t *x, int32_t *y)
{
    float a = ((float)((pc * 7) % 12) / 12.0f - 0.25f) * FM_TWO_PI;   /* circle of fifths */
    *x = WCX + (int32_t)((float)r * fm_cosf(a) + 0.5f);
    *y = WCY + (int32_t)((float)r * fm_sinf(a) + 0.5f);
}

static void studio_track_notes(uint32_t now)
{
    uint32_t n = ms_vis.notes;
    if (n - stn.seen > 16u)
        stn.seen = n - 16u;
    while (stn.seen != n) {
        stn.seen++;
        stn.at[stn.seen & 15u] = now | 1u;
    }
}

static void studio_header(uint32_t now)
{
    char t[64];
    rgb_t acc = MODE_ACCENT[set.mode];
    int32_t w = text_w(&FONT_B, MS_MODE_NAME[set.mode]) + 18;
    pill(6, 5, w, 20, c565(acc));
    cv_text(15, 7, &FONT_B, MS_MODE_NAME[set.mode], ST_BG);
    cv_text(w + 12, 7, &FONT_B, ms_scene(set.mode, set.scene[set.mode])->name, ST_TEXT);
    t[0] = 0;
    text_cat(t, MS_NOTE_NAME[set.root]);
    text_cat(t, " ");
    text_cat(t, MS_MOOD_NAME[set.mood[set.mode]]);
    text_cat(t, "  ");
    text_cat(t, MS_SPACE_NAME[set.space]);
    if ((int32_t)(ui.msg_until - now) <= 0)
        cv_text(w + 12, 25, &FONT_T, t, ST_SOFT);
    if (set.mode != MODE_SCAPES) {
        int beat = (ms_vis.step & 3u) == 0u && ui.playing;
        int_text(set.tempo[set.mode], t);
        cv_text(222 - text_w(&FONT_B, t), 7, &FONT_B, t, ST_TEXT);
        disc(229, 15, 2, beat ? c565(acc) : ST_DIM);
    }
    if ((int32_t)(ui.msg_until - now) > 0) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        cv_text(234 - text_w(&FONT_T, t), 25, &FONT_T, t, c565(acc));
    } else if (ms_vis.lp_state != MS_LOOP_EMPTY) {
        static const char *const ST_NAME[4] = {"", "REC", "FADING", "ADD"};
        int rec = ms_vis.lp_state == MS_LOOP_REC;
        float pos = rec ? 1.0f : ms_vis.lp_pos;
        uint16_t c = ms_vis.lp_state == MS_LOOP_PLAY ? c565(acc) : RGB(240, 110, 110);
        const char *nm = ms_vis.lp_state == MS_LOOP_PLAY && ms_vis.lp_keep ? "KEPT" : ST_NAME[ms_vis.lp_state];
        cv_text(190 - text_w(&FONT_T, nm), 25, &FONT_T, nm, c);
        fill(194, 30, 40, 3, ST_LINE);
        fill(194, 30, (int32_t)(40.0f * pos), 3, (rec && (now / 250u & 1u)) ? ST_LINE : c);
    } else if (ui.frozen || !ui.playing) {
        cv_text(234 - text_w(&FONT_T, ui.frozen ? "FROZEN" : "RESTING"), 25, &FONT_T, ui.frozen ? "FROZEN" : "RESTING", ST_SOFT);
    }
}

static void studio_wheel(uint32_t now)
{
    uint16_t scale = ms_scale_mask(set.mood[set.mode], set.root);
    uint32_t chord = ms_vis.chord_mask, i;
    rgb_t acc = MODE_ACCENT[set.mode], bg = C(16, 18, 28);
    char t[16];
    int32_t pc, x, y;
    circle(WCX, WCY, WR, ST_LINE);
    circle(WCX, WCY, WR - 26, ST_LINE);
    for (pc = 0; pc < 12; pc++) {
        int32_t q;
        if (!(chord >> pc & 1u))
            continue;
        for (q = pc + 1; q < 12; q++)
            if (chord >> q & 1u) {
                int32_t x2, y2;
                wheel_xy(pc, WR, &x, &y);
                wheel_xy(q, WR, &x2, &y2);
                cv_line(x, y, x2, y2, c565(mix(bg, acc, 70)));
            }
    }
    for (i = 0; i < 16u; i++) {
        uint32_t at = stn.at[i], age;
        uint16_t r = ms_vis.recent[i];
        int32_t note = r & 0xFF, layer = r >> 8, rr, oct;
        rgb_t lc;
        if (!at || (age = now - at) > 2400u)
            continue;
        oct = clamp(note / 12 - 3, 0, 4);
        rr = WR - 34 + oct * 9;
        lc = layer < 4 ? LAYER_RGB[set.mode][layer] : C(255, 255, 255);
        wheel_xy(note % 12, rr, &x, &y);
        circle(x, y, 2 + (int32_t)age / 300, c565(mix(lc, bg, (int32_t)age * 256 / 2400)));
        if (age < 400u)
            disc(x, y, 2, c565(lc));
    }
    for (pc = 0; pc < 12; pc++) {
        int on = chord >> pc & 1u, in = scale >> pc & 1u;
        wheel_xy(pc, WR, &x, &y);
        if (on) {
            halo(x, y, 9, c565(mix(bg, acc, 120)), 8);
            disc(x, y, 4, c565(acc));
        } else {
            disc(x, y, in ? 2 : 1, in ? ST_SOFT : ST_DIM);
        }
        wheel_xy(pc, WR + 12, &x, &y);
        cv_text(x - text_w(&FONT_XS, MS_NOTE_NAME[pc]) / 2, y - 7, &FONT_XS, MS_NOTE_NAME[pc], on ? ST_TEXT : in ? ST_SOFT : ST_DIM);
    }
    t[0] = 0;
    if (ms_vis.chord_root >= 0) {
        text_cat(t, MS_NOTE_NAME[ms_vis.chord_root]);
        text_cat(t, MS_CHORD_SUFFIX[ms_vis.chord_kind]);
    }
    cv_text(WCX - text_w(&FONT_B, t) / 2, WCY - 8, &FONT_B, t, ST_TEXT);
}

static void studio_meters(uint32_t now)
{
    int32_t i;
    for (i = 0; i < 4; i++) {
        int32_t x = 8 + i * 9, top = 44, h = 112, v = set.mix[set.mode][i] * h / 100;
        rgb_t lc = LAYER_RGB[set.mode][i];
        uint32_t j;
        int fresh = 0;
        for (j = 0; j < 16u; j++)
            if (stn.at[j] && now - stn.at[j] < 250u && (ms_vis.recent[j] >> 8) == (uint32_t)i)
                fresh = 1;
        fill(x, top, 5, h, ST_LINE);
        fill(x, top + h - v, 5, v, c565(mix(C(16, 18, 28), lc, fresh ? 256 : 150)));
        if (v)
            fill(x, top + h - v, 5, 2, c565(fresh ? C(255, 255, 255) : lc));
    }
}

static void studio_column(uint32_t now)
{
    int32_t i, j;
    rgb_t acc = MODE_ACCENT[set.mode];
    if (set.mode == MODE_DRIFT && set.page != PG_BEAT) {
        for (i = 0; i < 4; i++) {
            int32_t cx = 214, cy = 50 + i * 30, len = ms_vis.loop_len[i];
            float pos = ms_vis.loop_pos[i];
            circle(cx, cy, 11, ST_LINE);
            for (j = 0; j < len; j++)
                if (ms_vis.loop_notes[i] >> j & 1u) {
                    float a = ((float)j / (float)len - 0.25f) * FM_TWO_PI, d = pos - (float)j / (float)len;
                    int lit = d >= 0.0f && d < 1.5f / (float)len;
                    disc(cx + (int32_t)(11.0f * fm_cosf(a)), cy + (int32_t)(11.0f * fm_sinf(a)), lit ? 2 : 1,
                         lit ? ST_TEXT : c565(LAYER_RGB[MODE_DRIFT][1]));
                }
            {
                float a = (pos - 0.25f) * FM_TWO_PI;
                put(cx + (int32_t)(7.0f * fm_cosf(a)), cy + (int32_t)(7.0f * fm_sinf(a)), ST_SOFT);
            }
        }
    } else if (set.beat[set.mode][0] && (set.mode == MODE_LOFI || set.page == PG_BEAT)) {
        uint32_t len = ms_vis.beat_len ? ms_vis.beat_len : 16u, cur = ms_vis.beat_step;
        for (j = 0; j < (int32_t)len; j++) {
            int32_t y = 42 + j * 7;
            if ((uint32_t)j == cur && ui.playing)
                fill(196, y - 1, 38, 7, ST_LINE);
            for (i = 0; i < 3; i++) {
                int on = ms_vis.drums[i] >> j & 1u;
                fill(200 + i * 11, y, 7, 5, on ? c565(i == 2 ? LAYER_RGB[MODE_LOFI][1] : acc) : ST_LINE);
            }
        }
    } else {
        static const char *const NAME[4] = {"WIND", "RAIN", "SEA", "LIFE"};
        float v[4];
        v[0] = ms_vis.gust;
        v[1] = ms_vis.rain;
        v[2] = fm_maxf(ms_vis.wave, ms_vis.fire);
        v[3] = ui.calls_at && now - ui.calls_at < 1500u ? 1.0f - (float)(now - ui.calls_at) / 1500.0f : 0.0f;
        if (ui.chirp_at && now - ui.chirp_at < 300u)
            v[3] = fm_maxf(v[3], 0.6f);
        for (i = 0; i < 4; i++) {
            int32_t y = 46 + i * 30;
            cv_text(198, y, &FONT_T, i == 2 && ms_vis.fire > ms_vis.wave ? "FIRE" : NAME[i], ST_SOFT);
            fill(198, y + 14, 36, 3, ST_LINE);
            fill(198, y + 14, (int32_t)(36.0f * fm_clampf(v[i], 0.0f, 1.0f)), 3, c565(LAYER_RGB[MODE_SCAPES][i]));
        }
    }
}

static void studio_knobs(void)
{
    int32_t i;
    char t[16];
    rgb_t acc = MODE_ACCENT[set.mode];
    fill(6, 172, 228, 1, ST_LINE);
    cv_text(8, 176, &FONT_T, PAGE_NAME[set.page], ST_SOFT);
    if (set.view != VIEW_LOOPS)
        for (i = 0; i < NPAGES; i++)
            disc(232 - (NPAGES - 1 - i) * 7, 182, 2, i == set.page ? c565(acc) : ST_DIM);
    if (set.sleep && ui.sleep_start) {
        uint32_t left = SLEEP_MIN[set.sleep] * 60000u - (plat_ms() - ui.sleep_start);
        int_text((int32_t)(left / 60000u + 1u), t);
        text_cat(t, "m TO SLEEP");
        cv_text(206 - text_w(&FONT_T, t), 176, &FONT_T, t, ST_SOFT);
    }
    if (set.page == PG_SETUP) {
        t[0] = 0;
        text_cat(t, "CPU ");
        int_text((int32_t)plat_cpu_pct(), t + 4);
        text_cat(t, "%");
        cv_text(60, 176, &FONT_T, t, ST_DIM);
    }
    for (i = 0; i < 4; i++) {
        int32_t cx = 30 + i * 60, v = knob_value(i);
        rgb_t kc = set.page == PG_MIX ? LAYER_RGB[set.mode][i] : acc;
        arc(cx, 206, 12, v < 0 ? 0 : v, c565(kc), ST_LINE);
        knob_text(i, t);
        cv_text(cx - text_w(&FONT_T, t) / 2, 200, &FONT_T, t, ST_TEXT);
        cv_text(cx - text_w(&FONT_T, knob_name(i)) / 2, 222, &FONT_T, knob_name(i), ST_SOFT);
    }
}

/* the LOOPS screen: the loop pages as tabs, what the looper is doing and what to do next, a lane per layer, then
 * the library or a tip for the page */
static void loops_tabs(rgb_t acc)
{
    static const char *const TAB[4] = {"LAYERS", "SHAPE", "TAPE", "LIBRARY"};
    int32_t i;
    for (i = 0; i < 4; i++) {
        int32_t cx = 30 + i * 60, w = text_w(&FONT_T, TAB[i]);
        int on = set.page == PG_LOOP + i;
        if (on)
            pill(cx - w / 2 - 6, 40, w + 12, 14, c565(acc));
        cv_text(cx - w / 2, 41, &FONT_T, TAB[i], on ? ST_BG : ST_DIM);
    }
}

/* the looper's state in a few words, and the one thing to do next */
static void loops_status(uint32_t now, const char **state, const char **hint, uint16_t *c, int *blink)
{
    static char st[24];
    int tap = set.keys[3], lp = (int)ms_vis.lp_state, free_layer = -1, i;
    for (i = 0; i < MS_NLAYERS; i++)
        if (!(ms_vis.lp_tracks >> i & 1u)) {
            free_layer = i;
            break;
        }
    *c = c565(MODE_ACCENT[set.mode]);
    *blink = 0;
    st[0] = 0;
    if (lp == MS_LOOP_EMPTY) {
        *state = tap == 2 ? "HOLD: NO LOOPING" : tap == 1 ? "LISTENING" : "READY";
        *hint = tap == 2 ? "SET TAP TO LISTEN TO LOOP" : tap == 1 ? "PLAY AND IT LOOPS BY ITSELF" : "TAP REC, PLAY, TAP REC";
        *blink = tap == 1;
        if (tap == 2)
            *c = ST_SOFT;
        return;
    }
    if (lp == MS_LOOP_REC || lp == MS_LOOP_DUB) {
        text_cat(st, lp == MS_LOOP_REC ? "RECORDING LAYER " : "ADDING LAYER ");
        i = text_len(st);
        st[i] = (char)('1' + ms_vis.lp_track);
        st[i + 1] = 0;
        *state = st;
        *hint = tap == 1 ? (lp == MS_LOOP_REC ? "STOP PLAYING AND IT LOOPS" : "PAUSE AND IT JOINS THE LOOP")
                         : (lp == MS_LOOP_REC ? "TAP REC TO LOOP IT" : "TAP REC WHEN DONE");
        *c = RGB(240, 96, 96);
        *blink = 2;
        return;
    }
    *state = ms_vis.lp_keep ? "KEPT" : "FADING";
    if (tap == 1) {                       /* listening over a loop: the next phrase is a new layer */
        text_cat(st, ms_vis.lp_keep ? "KEPT. LISTENING" : "FADING. LISTENING");
        *state = st;
        *blink = 1;
    }
    switch (now / 3000u % 3u) {           /* the tips take turns */
    case 0:
        *hint = ms_vis.lp_redo >= 0 ? "PLAY TO REDO THE RED LAYER"
              : free_layer < 0 ? "LAYERS FULL. CLEAR ONE ON LAYERS"
              : tap == 1 ? "PLAY TO ADD A LAYER" : "TAP REC TO KEEP OR LET FADE";
        break;
    case 1:
        *hint = ms_vis.lp_keep ? "TAP REC TO LET IT FADE" : "TAP REC TO KEEP IT";
        break;
    default:
        *hint = "HOLD REC FOR A NEW LOOP";
        break;
    }
}

/* a lane per layer, thin so eight fit: the chosen one (LAYERS' K1) is marked, one armed for REDO pulses red */
static void loops_lane(int32_t t, int32_t y, uint32_t now, rgb_t acc)
{
    static const rgb_t WORN = CI(150, 120, 90);
    int32_t x0 = 22, w = 160, b, x;
    int used = ms_vis.lp_tracks >> t & 1u, muted = (ms_vis.lp_mute >> t & 1u) || !ui.track_lvl[t];
    int rec = (ms_vis.lp_state == MS_LOOP_DUB || ms_vis.lp_state == MS_LOOP_REC) && ms_vis.lp_track == t;
    int redo = ms_vis.lp_redo == t, sel = set.page == PG_LOOP && ui.sel_track == t;
    int32_t amp = set.tape[0] * 2 / 100;  /* WARBLE bends the lane, AGE browns the notes */
    rgb_t dot = mix(acc, WORN, (int32_t)set.tape[2] * 200 / 100);
    uint16_t red = RGB(240, 96, 96);
    uint16_t c = rec || (redo && (now / 300u & 1u)) ? red : !used ? ST_DIM : muted ? c565(mix(C(16, 18, 28), dot, 80)) : c565(dot);
    if (sel)
        fill(x0 - 4, y - 2, w + 8, 5, ST_LINE);
    for (x = 0; x < w; x += 2) {
        int32_t p = (int32_t)((x + (int32_t)(now / 40u)) & 31), tri = p < 16 ? p : 31 - p;
        fill(x0 + x, y + (amp ? (tri - 8) * amp / 8 : 0), 2, 1, sel ? ST_DIM : ST_LINE);
    }
    for (b = 0; b < 32; b++)
        if (ms_vis.lp_dots[t] >> b & 1u)
            disc(x0 + 2 + b * (w - 4) / 31, y, 1, c);
    if (redo)
        fill(4, y - 1, 10, 3, red);
    else if (used)
        fill(6, y - 1, 6, 3, c);
}

static void loops_lane_label(uint32_t now, int32_t y0)
{
    int t = ui.sel_track;
    char n[12];
    n[0] = (char)('1' + t);
    n[1] = 0;
    if (ms_vis.lp_redo == t)
        text_cat(n, " REDO");
    else if (!(ms_vis.lp_tracks >> t & 1u))
        text_cat(n, " EMPTY");
    else if (!ui.track_lvl[t] || ms_vis.lp_mute >> t & 1u)
        text_cat(n, " OFF");
    cv_text(234 - text_w(&FONT_T, n), y0 + t * 6 - 6, &FONT_T, n, ms_vis.lp_redo == t ? RGB(240, 96, 96) : ST_TEXT);
    (void)now;
}

static void loops_library(rgb_t acc)
{
    int32_t i;
    char n[16], v[8];
    for (i = 0; i < NSLOTS; i++) {               /* two rows of four: number and length */
        int32_t x = 4 + (i % 4) * 59, y = 146 + (i / 4) * 13;
        const ms_slot_t *sl = &slots.s[i];
        if (i == ui.sel_slot)
            pill(x, y - 1, 56, 13, ST_LINE);
        n[0] = i == ui.lib_cur ? '>' : (char)('1' + i);   /* > the loop playing now */
        n[1] = 0;
        cv_text(x + 4, y, &FONT_T, n, sl->n ? c565(acc) : ST_DIM);
        n[0] = 0;
        if (sl->n) {
            int_text(sl->len_ms / 1000u, n);
            text_cat(n, ".");
            int_text(sl->len_ms / 100u % 10u, v);
            text_cat(n, v);
            text_cat(n, "S");
        } else {
            text_cat(n, "-");
        }
        cv_text(x + 16, y, &FONT_T, n, sl->n ? ST_TEXT : ST_DIM);
    }
}

static void loops_view(uint32_t now)
{
    static const char *const TIP[3][2] = {
        {"LAYER PICKS ONE. REDO: PLAY IT AGAIN", "CLEAR: A FULL TURN RIGHT"},
        {"FADE AND MUTATE: EACH TIME ROUND", "SHIFT NUDGES IT A 16TH"},
        {"THE TAPE CHANGES ONLY THE LOOP", "NOT THE REST OF THE MUSIC"},
    };
    rgb_t acc = MODE_ACCENT[set.mode];
    const char *state, *hint;
    uint16_t c;
    int blink;
    int32_t t, px = 22 + (int32_t)(160.0f * ms_vis.lp_pos);
    char n[24];
    loops_tabs(acc);
    loops_status(now, &state, &hint, &c, &blink);
    if (!blink || (blink == 1 ? (now / 600u & 1u) : (now / 250u & 1u)))
        disc(12, 64, 3, c);
    cv_text(22, 58, &FONT_T, state, c);
    n[0] = 0;
    if (ms_vis.lp_state == MS_LOOP_PLAY || ms_vis.lp_state == MS_LOOP_DUB) {
        int bars = set.mode == MODE_LOFI || set.beat[set.mode][0];
        if (bars) {
            int32_t nb = (int32_t)(ms_vis.lp_len * (float)set.tempo[set.mode] / 240.0f + 0.5f);
            int_text(nb, n);
            text_cat(n, nb == 1 ? " BAR" : " BARS");
        } else {
            int_text((int32_t)ms_vis.lp_len, n);
            text_cat(n, "S");
        }
        if (set.loop[3] != 1) {
            text_cat(n, " ");
            text_cat(n, SPEED_NAME[set.loop[3]]);
        }
    }
    cv_text(234 - text_w(&FONT_T, n), 58, &FONT_T, n, ST_SOFT);
    for (t = 0; t < MS_NLAYERS; t++)
        loops_lane(t, 78 + t * 6, now, acc);
    if (ms_vis.lp_state == MS_LOOP_PLAY || ms_vis.lp_state == MS_LOOP_DUB)
        fill(px, 74, 1, 50, ST_TEXT);
    if (set.page == PG_LOOP)
        loops_lane_label(now, 78);
    centred(130, &FONT_T, hint, ST_TEXT);
    if (set.page == PG_LIBRARY) {
        loops_library(acc);
        return;
    }
    centred(145, &FONT_T, TIP[set.page - PG_LOOP][0], ST_SOFT);
    centred(156, &FONT_T, TIP[set.page - PG_LOOP][1], ST_SOFT);
}

static void studio_draw(uint32_t now)
{
    int half;
    studio_track_notes(now);
    for (half = 0; half < 2; half++) {
        band_begin(half, ST_BG);
        studio_header(now);
        if (set.view == VIEW_LOOPS) {
            loops_view(now);
        } else {
            studio_meters(now);
            studio_wheel(now);
            studio_column(now);
        }
        studio_knobs();
        band_end();
    }
}
