/* SPDX-License-Identifier: MPL-2.0 */
/* EVOLVE: SCAPES Forest for ten minutes, its level and brightness (zero crossings a second) per 20 s window. Off, they
 * hold; at the default they must move (level or brightness spread at least half again off's, over three runs) but
 * never jump (no two windows more than 8 dB apart: weather coming in is allowed, a step is not).
 *   evolve   exits 1 if not */
#include <stdio.h>
#include <math.h>
#include "moss.h"

#define WIN 30
static int32_t blk[512];

static double spread(int amount, int seed, double *jump, double *bright)
{
    const ms_scene_t *sc = ms_scene(MODE_SCAPES, 0);
    double lv[WIN], br[WIN], m = 0.0, s = 0.0, mb = 0.0, sb = 0.0;
    int w, b, i;
    ms_init();
    ms_seed((uint32_t)seed);
    ms_mode(MODE_SCAPES, 0);
    for (i = 0; i < 4; i++) {
        ms_macro(i, sc->macro[i]);
        ms_mix(i, sc->mix[i]);
    }
    ms_tempo(sc->tempo);
    ms_beat(0, sc->kit);
    ms_evolve(0, amount), ms_evolve(1, 40), ms_evolve(2, amount), ms_evolve(3, amount);
    *jump = 0.0;
    for (b = 0; b < 20 * 44100 / 256; b++)   /* the scene fades in first: not part of what is measured */
        ms_render(blk, 256, 4096);
    for (w = 0; w < WIN; w++) {
        double e = 0.0;
        long zc = 0;
        int32_t prev = 0;
        for (b = 0; b < 20 * 44100 / 256; b++) {
            ms_render(blk, 256, 4096);
            for (i = 0; i < 512; i += 2) {
                e += (double)blk[i] * blk[i] / (8388608.0 * 8388608.0);
                zc += (blk[i] > 0) != (prev > 0);
                prev = blk[i];
            }
        }
        lv[w] = 10.0 * log10(e / (20.0 * 44100.0) + 1e-12);
        br[w] = zc / 20.0;
        mb += br[w];
        if (w && fabs(lv[w] - lv[w - 1]) > *jump)
            *jump = fabs(lv[w] - lv[w - 1]);
        m += lv[w];
    }
    m /= WIN;
    mb /= WIN;
    for (w = 0; w < WIN; w++) {
        s += (lv[w] - m) * (lv[w] - m);
        sb += (br[w] - mb) * (br[w] - mb);
    }
    *bright = sqrt(sb / WIN);
    return sqrt(s / WIN);
}

int main(void)
{
    double off = 0, on = 0, b0 = 0, b1 = 0, jump = 0, j, b;
    int sd;
    for (sd = 1; sd <= 3; sd++) {        /* EVOLVE is chance: three runs, not one */
        off += spread(0, sd, &j, &b), b0 += b;
        on += spread(30, sd, &j, &b), b1 += b;
        if (j > jump)
            jump = j;
    }
    printf("level spread: off %.2f dB, default %.2f dB; brightness spread: off %.0f, default %.0f; biggest step %.2f dB\n",
           off / 3, on / 3, b0 / 3, b1 / 3, jump);
    return !((on >= 1.5 * off || b1 >= 1.5 * b0) && jump < 8.0);
}
