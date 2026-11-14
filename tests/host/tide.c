/* SPDX-License-Identifier: MPL-2.0 */
/* EVOLVE's tide: LOFI Study for fifteen minutes at the default EVOLVE. The tide must come home and go out again, and
 * the melody must come back with it: at home nearly every melody note is the one played a round earlier, out at
 * the far end few are.
 *   tide   exits 1 if not */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "moss.h"

#define SR 44100
#define BLK 256
#define MAXN 8192
static int32_t blk[BLK * 2];
static struct { long t; int note; float away; } mel[MAXN];

int main(void)
{
    const ms_scene_t *sc = ms_scene(MODE_LOFI, 0);
    long t = 0, round = 0, changes[8];
    int n = 0, nc = 0, i, j, root = -1, home_n = 0, home_hit = 0, out_n = 0, out_hit = 0, went_home = 0, left = 0;
    uint32_t seen = 0;
    ms_init();
    ms_seed(7);
    ms_mode(MODE_LOFI, 0);
    for (i = 0; i < 4; i++) {
        ms_macro(i, sc->macro[i]);
        ms_mix(i, sc->mix[i]);
    }
    ms_tempo(sc->tempo);
    ms_beat(0, sc->kit);
    ms_evolve(0, 30), ms_evolve(1, 40), ms_evolve(2, 30), ms_evolve(3, 30);
    for (; t < 15L * 60 * SR; t += BLK) {
        ms_render(blk, BLK, 4096);
        if (ms_vis.evo_away < 0.001f)
            went_home = 1;
        else if (went_home && ms_vis.evo_away > 0.9f)
            left = 1;
        if (ms_vis.chord_root != root) {  /* a round is four chords: its length, from the first ones heard */
            root = ms_vis.chord_root;
            if (nc < 8)
                changes[nc++] = t;
        }
        for (; seen != ms_vis.notes; seen++) {
            int e = ms_vis.recent[(seen + 1u) & 15u];
            if ((e >> 8) == 0 && (e & 127) >= 70 && n < MAXN) {   /* the melody: the music's own layer, up high */
                mel[n].t = t;
                mel[n].note = e & 127;
                mel[n++].away = ms_vis.evo_away;
            }
        }
    }
    if (nc >= 6)
        round = changes[5] - changes[1];
    for (i = 0; i < n && round; i++) {
        int hit = 0;
        if (mel[i].t < mel[0].t + round)
            continue;
        for (j = i - 1; j >= 0 && mel[j].t > mel[i].t - round - SR / 40; j--)
            hit |= mel[j].note == mel[i].note && labs(mel[i].t - round - mel[j].t) <= SR / 40;
        if (mel[i].away < 0.001f) {
            home_n++;
            home_hit += hit;
        } else if (mel[i].away > 0.9f) {
            out_n++;
            out_hit += hit;
        }
    }
    printf("round %.1f s; melody notes at home %d, %.0f%% as a round before; far out %d, %.0f%%; home %s, out again %s\n",
           (double)round / SR, home_n, home_n ? 100.0 * home_hit / home_n : 0.0, out_n,
           out_n ? 100.0 * out_hit / out_n : 0.0, went_home ? "yes" : "no", left ? "yes" : "no");
    return !(went_home && left && home_n >= 20 && out_n >= 20 && home_hit >= 0.85 * home_n
             && out_hit <= 0.5 * out_n);
}
