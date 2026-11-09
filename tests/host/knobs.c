/* SPDX-License-Identifier: MPL-2.0 */
/* Every knob must be heard. For each mode's first scene, with keys played into a loop, each knob is rendered at its
 * low and its high end from the same seed, and the two compared sample by sample: the difference, against the
 * sound, in dB. A knob that does nothing gives silence; one that changes the sound gives -20 dB or more.
 *   knobs [MODE]   prints every knob's score, exits 1 if one is not heard */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include "moss.h"

#define SECS 20
#define START (4 * 44100)
#define N ((SECS * 44100 / 256) * 256)
#define HEARD -20.0

static int keep_loop;

typedef struct {
    const char *page, *name;
    void (*set)(int i, int v);
    int i, lo, hi;
} knob_t;

static void scene_setup(int m)
{
    const ms_scene_t *sc = ms_scene(m, 0);
    int i;
    ms_mode(m, 0);
    for (i = 0; i < MS_NMACRO; i++)
        ms_macro(i, sc->macro[i]);
    for (i = 0; i < MS_NMIX; i++)
        ms_mix(i, sc->mix[i]);
    ms_tempo(sc->tempo);
    ms_mood(sc->mood);
    ms_beat(0, sc->kit);
    ms_beat(1, sc->pattern);
    ms_beat(2, sc->beat_level ? sc->beat_level : 70);
    ms_fx(0, 30), ms_fx(1, 2), ms_fx(2, 45), ms_fx(3, 20);
    ms_motion(0, 25), ms_motion(1, 20), ms_motion(2, 30), ms_motion(3, 70);
    ms_loop_set(0, 30), ms_loop_set(1, 25), ms_loop_set(2, 30), ms_loop_set(3, 1);
    ms_loop_fx(0, 0), ms_loop_fx(1, 0), ms_loop_fx(2, 0), ms_loop_fx(3, 50);
    ms_keys(0, 1), ms_keys(1, 30), ms_keys(2, 0), ms_keys(3, 0);
}

/* 3 s of keys into a loop, then the loop plays on. In a child process: the engine's state then starts the same
 * every take, which ms_init alone does not promise */
static void take_here(int m, const knob_t *k, int v, float *out)
{
    static int32_t blk[512];
    static const int MEL[8] = {60, 64, 67, 72, 69, 65, 62, 71};
    int b, i;
    ms_init();
    ms_seed(1);
    scene_setup(m);
    if (k)
        k->set(k->i, v);
    for (b = 0; b < N / 256; b++) {
        double t = b * 256.0 / 44100.0;
        if (b == 30)
            ms_loop(0);
        if (t > 0.3 && t < 3.3 && b % 30 == 0)
            ms_note(MEL[(b / 30) % 8], 100);
        if (t > 0.3 && t < 3.3 && b % 30 == 20)
            ms_note(MEL[(b / 30) % 8], 0);
        if (b == (int)(3.5 * 44100 / 256))
            ms_loop(0);
        if (keep_loop && b == (int)(3.6 * 44100 / 256))
            ms_loop(0);
        ms_render(blk, 256, 4096);
        for (i = 0; i < 512; i++)
            out[b * 512 + i] = (float)(blk[i] / 8388608.0);
    }
}

static void take(int m, const knob_t *k, int v, float *out)
{
    int fd[2], st;
    size_t got = 0;
    pid_t pid;
    if (pipe(fd) != 0 || (pid = fork()) < 0)
        exit(2);
    if (pid == 0) {
        size_t put = 0;
        close(fd[0]);
        take_here(m, k, v, out);
        while (put < sizeof(float) * 2 * N) {
            ssize_t w = write(fd[1], (char *)out + put, sizeof(float) * 2 * N - put);
            if (w <= 0)
                _exit(1);
            put += (size_t)w;
        }
        _exit(0);
    }
    close(fd[1]);
    while (got < sizeof(float) * 2 * N) {
        ssize_t r = read(fd[0], (char *)out + got, sizeof(float) * 2 * N - got);
        if (r <= 0)
            exit(2);
        got += (size_t)r;
    }
    close(fd[0]);
    waitpid(pid, &st, 0);
}

static double diff_db(const float *a, const float *b)
{
    double d = 0.0, s = 0.0;
    int i;
    for (i = 2 * START; i < 2 * N; i++) {
        d += (double)(a[i] - b[i]) * (a[i] - b[i]);
        s += 0.5 * ((double)a[i] * a[i] + (double)b[i] * b[i]);
    }
    return d <= 0.0 ? -200.0 : 10.0 * log10(d / (s + 1e-20));
}

static void set_macro(int i, int v) { ms_macro(i, v); }
static void set_mix(int i, int v) { ms_mix(i, v); }
static void set_tempo(int i, int v) { (void)i; ms_tempo(v); ms_fx(1, 2); }
static void set_mood(int i, int v) { (void)i; ms_mood(v); }
/* the knobs that shape the repeats are heard with the repeats up, as they are used */
static void set_wet_fx(int i, int v) { ms_fx(0, 60); ms_fx(i, v); }
static void set_wet_motion(int i, int v) { ms_fx(0, 60); ms_motion(i, v); }
static void set_beat_on(int i, int v) { ms_beat(0, 1); ms_beat(i, v); }   /* PATTERN and LEVEL need a kit */
/* the loop's tape is heard on a loop that is kept, not one fading away */
static void set_tape(int i, int v) { keep_loop = 1; if (i == 3) ms_loop_set(2, v); else ms_loop_fx(i, v); }

int main(int argc, char **argv)
{
    static const knob_t K[] = {
        {"HOME", 0, set_macro, 0, 0, 100}, {"HOME", 0, set_macro, 1, 0, 100}, {"HOME", 0, set_macro, 2, 0, 100},
        {"HOME", 0, set_macro, 3, 0, 100}, {"MIX", 0, set_mix, 0, 0, 100}, {"MIX", 0, set_mix, 1, 0, 100},
        {"MIX", 0, set_mix, 2, 0, 100}, {"MIX", 0, set_mix, 3, 0, 100},
        {"FX", "DELAY", ms_fx, 0, 0, 100}, {"FX", "TIME", set_wet_fx, 1, 0, 5}, {"FX", "REPEATS", set_wet_fx, 2, 0, 100},
        {"FX", "FILM", ms_fx, 3, 0, 100},
        {"MOTION", "MOD", set_wet_motion, 0, 0, 100}, {"MOTION", "RANDOM", ms_motion, 1, 0, 100},
        {"MOTION", "TONE", set_wet_motion, 2, 0, 100}, {"MOTION", "SPREAD", set_wet_motion, 3, 0, 100},
        {"BEAT", "KIT", ms_beat, 0, 0, 3}, {"BEAT", "PATTERN", set_beat_on, 1, 0, 5}, {"BEAT", "LEVEL", set_beat_on, 2, 0, 100},
        {"BEAT", "BASS", ms_beat, 3, 1, 3},
        {"KEYS", "SNAP", ms_keys, 0, 0, 2}, {"KEYS", "HARMONY", ms_keys, 1, 0, 100}, {"KEYS", "VOICE", ms_keys, 2, 1, 3},
        {"KEYS", "TAP", ms_keys, 3, 0, 1},
        {"LOOP", "LAYER 1", ms_track, 0, 0, 100},
        {"SHAPE", "FADE", ms_loop_set, 0, 0, 100}, {"SHAPE", "MUTATE", ms_loop_set, 1, 0, 100},
        {"SHAPE", "SPEED", ms_loop_set, 3, 1, 3},
        {"TAPE", "WARBLE", set_tape, 0, 0, 100}, {"TAPE", "DIRT", set_tape, 1, 0, 100}, {"TAPE", "AGE", set_tape, 2, 0, 100},
        {"TAPE", "SMEAR", set_tape, 3, 0, 100},
        {"SETUP", "TEMPO", set_tempo, 0, 60, 140}, {"SETUP", "MOOD", set_mood, 0, 0, 3},
    };
    static float a[2 * N], b[2 * N];
    int only = argc > 1 ? atoi(argv[1]) : -1, m, fails = 0;
    for (m = 0; m < MS_NMODES; m++) {
        size_t k;
        if (only >= 0 && m != only)
            continue;
        take(m, 0, 0, a);
        take(m, 0, 0, b);
        printf("%s %s: the same take twice %.0f dB\n", MS_MODE_NAME[m], ms_scene(m, 0)->name, diff_db(a, b));
        for (k = 0; k < sizeof K / sizeof K[0]; k++) {
            const char *name = K[k].name ? K[k].name
                             : K[k].set == set_macro ? MS_MACRO_NAME[m][K[k].i] : MS_MIX_NAME[m][K[k].i];
            double d;
            int ok;
            take(m, &K[k], K[k].lo, a);
            take(m, &K[k], K[k].hi, b);
            d = diff_db(a, b);
            ok = d > HEARD;
            fails += !ok;
            printf("  %-4s %-7s %-8s %7.1f dB\n", ok ? "" : "FLAT", K[k].page, name, d);
            fflush(stdout);
        }
    }
    printf(fails ? "%d knobs not heard\n" : "every knob heard\n", fails);
    return fails != 0;
}
