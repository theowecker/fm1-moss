/* SPDX-License-Identifier: MPL-2.0 */
/* EVOLVE: LOFI Study for ten minutes, its level per 20 s window. Off, the level holds; at the default it must move
 * (a spread at least twice off's) but never jump (no two windows more than 6 dB apart).
 *   evolve   exits 1 if not */
#include <stdio.h>
#include <math.h>
#include "moss.h"

#define WIN 30
static int32_t blk[512];

static double spread(int amount, double *jump)
{
    const ms_scene_t *sc = ms_scene(MODE_LOFI, 0);
    double lv[WIN], m = 0.0, s = 0.0;
    int w, b, i;
    ms_init();
    ms_seed(7);
    ms_mode(MODE_LOFI, 0);
    for (i = 0; i < 4; i++) {
        ms_macro(i, sc->macro[i]);
        ms_mix(i, sc->mix[i]);
    }
    ms_tempo(sc->tempo);
    ms_beat(0, sc->kit);
    ms_evolve(0, amount), ms_evolve(1, 40), ms_evolve(2, amount), ms_evolve(3, amount);
    *jump = 0.0;
    for (w = 0; w < WIN; w++) {
        double e = 0.0;
        for (b = 0; b < 20 * 44100 / 256; b++) {
            ms_render(blk, 256, 4096);
            for (i = 0; i < 512; i += 2)
                e += (double)blk[i] * blk[i] / (8388608.0 * 8388608.0);
        }
        lv[w] = 10.0 * log10(e / (20.0 * 44100.0) + 1e-12);
        if (w && fabs(lv[w] - lv[w - 1]) > *jump)
            *jump = fabs(lv[w] - lv[w - 1]);
        m += lv[w];
    }
    m /= WIN;
    for (w = 0; w < WIN; w++)
        s += (lv[w] - m) * (lv[w] - m);
    return sqrt(s / WIN);
}

int main(void)
{
    double j0, j1, off = spread(0, &j0), on = spread(30, &j1);
    printf("level spread: off %.2f dB, default %.2f dB; biggest step between windows %.2f dB\n", off, on, j1);
    return !(on >= 2.0 * off && j1 < 6.0);
}
