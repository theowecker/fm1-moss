/* SPDX-License-Identifier: MPL-2.0 */
/* Renders each scene to a WAV and reports levels and host CPU. Usage: render OUTDIR [SECONDS] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "moss.h"

static void wav_header(FILE *f, uint32_t frames)
{
    uint32_t bytes = frames * 4u, v;
    uint16_t h;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f);
    v = 44100; fwrite(&v, 4, 1, f); v = 44100 * 4; fwrite(&v, 4, 1, f);
    h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
}

static void select_scene(int m, int s)
{
    const ms_scene_t *sc = ms_scene(m, s);
    int i;
    ms_mode(m, s);
    for (i = 0; i < MS_NMACRO; i++)
        ms_macro(i, sc->macro[i]);
    for (i = 0; i < MS_NMIX; i++)
        ms_mix(i, sc->mix[i]);
    ms_tempo(sc->tempo);
    ms_mood(sc->mood);
    ms_beat(0, m == MODE_LOFI ? 2 : 0);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    float secs = argc > 2 ? (float)atof(argv[2]) : 30.0f;
    static int32_t blk[512];
    int m, s, fails = 0;
    ms_init();
    for (m = 0; m < MS_NMODES; m++)
        for (s = 0; s < MS_NSCENES[m]; s++) {
            char path[512];
            FILE *f;
            uint32_t frames = (uint32_t)(secs * 44100.0f), done = 0, i;
            double sum = 0.0, peak = 0.0;
            clock_t t0;
            float cpu;
            select_scene(m, s);
            for (i = 0; i < 44100u; i += 256u)   /* discard the crossfade from the previous scene */
                ms_render(blk, 256, 4096);
            snprintf(path, sizeof path, "%s/%d%d-%s-%s.wav", dir, m, s, MS_MODE_NAME[m], ms_scene(m, s)->name);
            f = fopen(path, "wb");
            wav_header(f, frames);
            t0 = clock();
            while (done < frames) {
                uint32_t n = frames - done < 256u ? frames - done : 256u;
                int16_t out[512];
                ms_render(blk, n, 4096);
                for (i = 0; i < 2u * n; i++) {
                    double x = blk[i] / 8388608.0;
                    sum += x * x;
                    if (fabs(x) > peak)
                        peak = fabs(x);
                    out[i] = (int16_t)(blk[i] >> 8);
                }
                fwrite(out, 2, 2u * n, f);
                done += n;
            }
            cpu = (float)(clock() - t0) / CLOCKS_PER_SEC / secs * 100.0f;
            fclose(f);
            printf("%-7s %-7s  peak %6.1f dB  rms %6.1f dB  notes %5u  host cpu %.2f%%\n", MS_MODE_NAME[m],
                   ms_scene(m, s)->name, 20.0 * log10(peak + 1e-9), 10.0 * log10(sum / (2.0 * frames) + 1e-12),
                   ms_vis.notes, (double)cpu);
            if (peak < 0.01 || peak > 0.999)
                fails++;
        }
    return fails ? 1 : 0;
}
