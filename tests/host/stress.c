/* SPDX-License-Identifier: MPL-2.0 */
/* Every scene in every space with knobs at their extremes and the looper fed, checking for NaN and that output
 * decays after PLAY stops. Usage: stress [SECONDS_PER_CASE] */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "moss.h"

static int32_t blk[512];

static double run(float secs, int play_keys)
{
    uint32_t n = (uint32_t)(secs * 44100.0f / 256.0f), b, i;
    double peak = 0.0;
    for (b = 0; b < n; b++) {
        if (play_keys && b % 40u == 0u)
            ms_note_now(48 + (int)(b / 40u % 24u), 100);
        if (play_keys && b % 40u == 20u)
            ms_note_now(48 + (int)(b / 40u % 24u), 0);
        ms_render(blk, 256, 4096);
        for (i = 0; i < 512u; i++) {
            double x = fabs(blk[i] / 8388608.0);
            if (x > peak)
                peak = x;
        }
    }
    return peak;
}

int main(int argc, char **argv)
{
    float secs = argc > 1 ? (float)atof(argv[1]) : 20.0f;
    int m, s, sp, fails = 0;
    ms_init();
    for (sp = 0; sp < MS_NSPACES; sp++)
        for (m = 0; m < MS_NMODES; m++)
            for (s = 0; s < MS_NSCENES[m]; s++) {
                double loud, after;
                int i;
                ms_mode(m, s);
                for (i = 0; i < 4; i++) {
                    ms_macro(i, 100);
                    ms_mix(i, 100);
                }
                ms_tempo(40);
                ms_space(sp);
                ms_fx(0, 100);
                ms_fx(1, 5);
                ms_fx(2, 100);
                ms_fx(3, 100);
                for (i = 0; i < 4; i++)
                    ms_motion(i, 100);
                ms_keys(1, 100);
                ms_keys(3, 1 + (s + sp) % 2);   /* LISTEN or HOLD */
                ms_loop_set(2, 100);
                ms_loop_set(3, (sp + s) % 5);
                ms_beat(0, 1 + (s + sp) % (MS_NKITS - 1));
                ms_beat(1, (s + m) % MS_NPATTERNS);
                ms_beat(2, 100);
                ms_beat(3, (s + sp) % MS_NBASS);
                ms_play(1);
                loud = run(secs, 1);
                ms_loop(1);
                ms_play(0);
                run(90.0f, 0);
                after = run(5.0f, 0);
                printf("%-8s %-7s %-7s peak %.3f  after stop %.4f%s\n", MS_SPACE_NAME[sp], MS_MODE_NAME[m],
                       ms_scene(m, s)->name, loud, after, after > 0.01 || !(loud == loud) ? "  FAIL" : "");
                if (after > 0.01 || !(loud == loud))
                    fails++;
                ms_play(1);
            }
    return fails ? 1 : 0;
}
