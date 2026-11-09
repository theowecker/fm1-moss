/* SPDX-License-Identifier: MPL-2.0 */
/* Pops: a tonal scene with no drums, pushed where sound used to be cut or jump (voices stolen by fast playing,
 * REVERB swept, spaces, FILM and KIT switched), measured as the largest second difference of the output. A pop is a
 * corner in the waveform; smooth tones have none.
 *   clicks [LIMIT]   exits 1 if the worst corner is above LIMIT */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "moss.h"

static int32_t blk[512];
static double worst, prev1, prev2;
static int primed;

static void run(int blocks, int keys)
{
    int b, i;
    for (b = 0; b < blocks; b++) {
        if (keys && b % 3 == 0)
            ms_note_now(48 + (b * 7) % 36, 110);
        if (keys && b % 3 == 2)
            ms_note_now(48 + ((b - 2) * 7) % 36, 0);
        ms_render(blk, 256, 4096);
        for (i = 0; i < 512; i += 2) {
            double x = blk[i] / 8388608.0, c = fabs(x - 2.0 * prev1 + prev2);
            if (primed > 2 && c > worst)
                worst = c;
            prev2 = prev1;
            prev1 = x;
            primed++;
        }
    }
}

int main(int argc, char **argv)
{
    double limit = argc > 1 ? atof(argv[1]) : 0.05;
    const ms_scene_t *sc = ms_scene(MODE_DRIFT, 4);
    int i, v;
    ms_init();
    ms_mode(MODE_DRIFT, 4);
    for (i = 0; i < 4; i++) {
        ms_macro(i, sc->macro[i]);
        ms_mix(i, sc->mix[i]);
    }
    ms_tempo(sc->tempo);
    ms_beat(0, 0);
    ms_keys(1, 100);                     /* harmony: three voices a key, so the pool fills and voices are stolen */
    ms_keys(2, 3);                       /* PAD, the longest voices */
    run(400, 0);
    run(1200, 1);
    for (v = 0; v <= 100; v += 10) {     /* REVERB swept */
        ms_macro(3, v);
        run(30, 1);
    }
    for (i = 0; i < MS_NSPACES; i++) {
        ms_space(i);
        run(120, 1);
    }
    ms_fx(3, 100);                       /* FILM: its saturation must ease in (its crackle is meant to pop, and is off */
    run(6, 1);                           /* for these few blocks: the first comes after ~50 ms on average) */
    ms_fx(3, 0);
    run(200, 1);
    ms_beat(0, 2);
    run(100, 0);
    ms_beat(0, 0);
    run(200, 0);
    printf("worst corner %.4f (limit %.4f)\n", worst, limit);
    return worst > limit;
}
