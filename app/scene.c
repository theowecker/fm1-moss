/* SPDX-License-Identifier: MPL-2.0 */
/* The SCENE view: one LOOKS entry per scene. Notes light fireflies, windows or flowers; ms_vis drives the weather. */

enum { L_HILLS, L_PINES, L_CITY, L_MOUNT, L_SEA, L_GARDEN };
enum { W_NONE, W_LAKE, W_SEA, W_STREAM };
enum { O_NONE, O_MOON, O_SUN };
#define F_STARS 1u
#define F_FIREFLIES 2u
#define F_REEDS 4u
#define F_CLOUDS 8u
#define F_CAMPFIRE 16u
#define F_BUTTERFLIES 32u

typedef struct {
    rgb_t top, mid, low, far, near, water, ground, light;
    uint8_t land, water_kind, orb, orb_x, orb_y, flags;
} look_t;

#define CI(r, g, b) {r, g, b}
#define C(r, g, b) ((rgb_t){r, g, b})
static const look_t LOOKS[MS_NMODES][MS_MAX_SCENES] = {
    {
        {CI(40, 30, 80), CI(120, 70, 130), CI(250, 160, 120), CI(70, 50, 90), CI(30, 24, 46), CI(40, 34, 70), CI(14, 12, 22),
         CI(255, 210, 120), L_CITY, W_LAKE, O_NONE, 0, 0, F_STARS},
        {CI(40, 48, 60), CI(70, 80, 96), CI(110, 118, 130), CI(60, 66, 80), CI(34, 38, 48), CI(44, 52, 64), CI(18, 20, 26),
         CI(255, 200, 110), L_CITY, W_LAKE, O_NONE, 0, 0, F_CLOUDS},
        {CI(6, 8, 24), CI(16, 22, 52), CI(40, 50, 90), CI(24, 28, 52), CI(10, 12, 24), CI(12, 16, 36), CI(4, 6, 12),
         CI(255, 220, 140), L_CITY, W_LAKE, O_MOON, 190, 40, F_STARS},
        {CI(60, 40, 100), CI(230, 110, 120), CI(255, 200, 120), CI(120, 60, 100), CI(50, 30, 60), CI(90, 60, 100),
         CI(30, 18, 36), CI(255, 230, 160), L_CITY, W_LAKE, O_SUN, 70, 132, 0},
    },
    {
        {CI(90, 150, 210), CI(170, 200, 220), CI(240, 220, 170), CI(100, 140, 130), CI(40, 80, 60), CI(70, 120, 150),
         CI(50, 90, 50), CI(255, 255, 255), L_PINES, W_NONE, O_SUN, 196, 34, F_BUTTERFLIES},
        {CI(70, 80, 92), CI(110, 118, 128), CI(150, 156, 160), CI(90, 100, 104), CI(50, 62, 60), CI(70, 80, 90),
         CI(30, 40, 36), CI(200, 210, 220), L_HILLS, W_LAKE, O_NONE, 0, 0, F_CLOUDS | F_REEDS},
        {CI(70, 90, 170), CI(200, 140, 170), CI(255, 200, 150), CI(120, 100, 140), CI(60, 50, 80), CI(60, 80, 130),
         CI(200, 170, 120), CI(255, 240, 220), L_SEA, W_SEA, O_SUN, 168, 140, 0},
        {CI(4, 8, 26), CI(12, 22, 54), CI(34, 52, 96), CI(18, 26, 50), CI(8, 12, 24), CI(10, 18, 40), CI(4, 6, 12),
         CI(210, 255, 90), L_HILLS, W_LAKE, O_MOON, 60, 36, F_STARS | F_FIREFLIES | F_REEDS},
        {CI(24, 28, 30), CI(44, 50, 50), CI(70, 76, 70), CI(40, 46, 44), CI(20, 26, 24), CI(34, 40, 42), CI(14, 18, 16),
         CI(220, 230, 255), L_HILLS, W_LAKE, O_NONE, 0, 0, F_CLOUDS | F_REEDS},
        {CI(8, 10, 22), CI(18, 20, 40), CI(40, 30, 50), CI(20, 22, 34), CI(8, 10, 16), CI(20, 20, 30), CI(16, 12, 14),
         CI(255, 180, 80), L_PINES, W_NONE, O_NONE, 0, 0, F_STARS | F_CAMPFIRE | F_FIREFLIES},
        {CI(110, 170, 220), CI(180, 215, 230), CI(230, 235, 200), CI(110, 160, 140), CI(60, 110, 70), CI(90, 150, 200),
         CI(70, 120, 60), CI(255, 250, 200), L_HILLS, W_STREAM, O_SUN, 50, 30, F_BUTTERFLIES},
    },
    {
        {CI(120, 140, 200), CI(230, 180, 200), CI(255, 225, 190), CI(160, 150, 190), CI(90, 120, 100), CI(120, 160, 200),
         CI(70, 110, 70), CI(255, 140, 170), L_GARDEN, W_NONE, O_SUN, 60, 118, F_BUTTERFLIES},
        {CI(6, 10, 30), CI(14, 30, 60), CI(30, 60, 90), CI(20, 30, 50), CI(10, 16, 30), CI(10, 24, 48), CI(40, 40, 50),
         CI(220, 230, 255), L_SEA, W_SEA, O_MOON, 80, 50, F_STARS},
        {CI(120, 170, 220), CI(180, 210, 235), CI(225, 240, 245), CI(150, 170, 200), CI(60, 90, 100), CI(100, 150, 190),
         CI(40, 60, 60), CI(255, 255, 255), L_MOUNT, W_LAKE, O_NONE, 0, 0, F_CLOUDS},
        {CI(18, 22, 58), CI(52, 46, 98), CI(210, 120, 120), CI(34, 30, 56), CI(16, 18, 30), CI(20, 26, 52), CI(8, 10, 16),
         CI(210, 255, 90), L_HILLS, W_LAKE, O_NONE, 0, 0, F_STARS | F_FIREFLIES | F_REEDS},
        {CI(105, 165, 230), CI(185, 215, 240), CI(255, 236, 200), CI(150, 175, 175), CI(85, 135, 90), CI(120, 170, 210),
         CI(80, 135, 75), CI(255, 140, 170), L_GARDEN, W_NONE, O_SUN, 188, 38, F_BUTTERFLIES},
    },
};

#define HZ 150                           /* horizon row */
#define WATER_END 200                    /* lake's near shore row */

static struct {
    const look_t *lk;
    int built_mode, built_scene;
    int16_t far_h[240], near_h[240];
    uint16_t sky_a[HZ], sky_b[HZ];
    uint8_t sky_l[HZ];
    rgb_t sky_rgb[HZ];
    uint32_t seen_notes;
    float glow[16];                      /* note-lit objects, decaying 1..0 */
    uint32_t lit_t;
} sc;

static void scene_build(int mode, int scene)
{
    const look_t *lk = &LOOKS[mode][scene];
    uint32_t seed = hash32((uint32_t)(mode * 16 + scene + 1));
    float s1 = (float)(seed & 255u) * 0.1f, s2 = (float)(seed >> 8 & 255u) * 0.1f;
    int32_t x;
    sc.lk = lk;
    sc.built_mode = mode;
    sc.built_scene = scene;
    for (x = 0; x < 240; x++) {
        float fx = (float)x, h;
        if (lk->land == L_MOUNT) {
            float a = fm_fabsf(fm_sinf(fx * 0.018f + s1)), b = fm_fabsf(fm_sinf(fx * 0.041f + s2));
            h = 22.0f + 44.0f * a * a + 12.0f * b;
        } else if (lk->land == L_SEA) {
            h = x > 150 && x < 200 ? 6.0f + 5.0f * fm_sinf((fx - 150.0f) * 0.063f) : 0.0f;   /* island */
        } else {
            h = 20.0f + 10.0f * fm_sinf(fx * 0.021f + s1) + 6.0f * fm_sinf(fx * 0.057f + s2);
        }
        sc.far_h[x] = (int16_t)(HZ - ((int32_t)h & ~1));
        h = lk->land == L_SEA ? 0.0f : 7.0f + 6.0f * fm_sinf(fx * 0.035f + s2) + 3.0f * fm_sinf(fx * 0.11f + s1);
        if (lk->land == L_CITY) {
            uint32_t b = hash32((uint32_t)(x / 14) + seed);
            h = (float)(10 + (int32_t)(b % 34u)) * ((b >> 8) % 5u ? 1.0f : 0.4f);
        }
        sc.near_h[x] = (int16_t)(HZ - ((int32_t)h & ~1));
    }
}

static void sky_rows(uint32_t now)
{
    const look_t *lk = sc.lk;
    int32_t y, split = HZ * 11 / 20, lvl;
    int flash = ui.flash_at && now - ui.flash_at < 140u;
    rgb_t top = lk->top, mid = lk->mid, low = lk->low, white = C(235, 235, 255);
    if (flash) {
        top = mix(top, white, 110);
        mid = mix(mid, white, 140);
        low = mix(low, white, 120);
    }
    for (y = 0; y < HZ; y++) {
        uint16_t a, b;
        if (y < split) {
            grad_at(y, 0, split, top, mid, 9, &a, &b, &lvl);
            sc.sky_rgb[y] = mix(top, mid, y * 256 / split);
        } else {
            grad_at(y, split, HZ, mid, low, 7, &a, &b, &lvl);
            sc.sky_rgb[y] = mix(mid, low, (y - split) * 256 / (HZ - split));
        }
        sc.sky_a[y] = a;
        sc.sky_b[y] = b;
        sc.sky_l[y] = (uint8_t)lvl;
    }
}

static void draw_sky(uint32_t now)
{
    const look_t *lk = sc.lk;
    int32_t y, i;
    for (y = band_y0; y < band_y1 && y < HZ; y++)
        dither_row(y, 0, 240, sc.sky_a[y], sc.sky_b[y], sc.sky_l[y]);
    if (lk->flags & F_STARS)
        for (i = 0; i < 46; i++) {
            uint32_t h = hash32((uint32_t)i * 7919u + 1u);
            int32_t x = (int32_t)(h % 240u), sy = (int32_t)((h >> 9) % (HZ * 3 / 5));
            int twinkle = hash32(h + now / 450u) % 6u == 0u;
            put(x, sy, twinkle ? RGB(255, 255, 255) : c565(mix(lk->top, C(200, 200, 230), 150)));
        }
    if (lk->orb == O_MOON) {
        halo(lk->orb_x, lk->orb_y, 22, c565(mix(lk->mid, C(220, 220, 240), 90)), 7);
        disc(lk->orb_x, lk->orb_y, 8, RGB(236, 232, 210));
        disc(lk->orb_x + 4, lk->orb_y - 2, 6, c565(sc.sky_rgb[clamp(lk->orb_y, 0, HZ - 1)]));
    } else if (lk->orb == O_SUN) {
        halo(lk->orb_x, lk->orb_y, 30, c565(mix(lk->low, C(255, 250, 220), 120)), 9);
        disc(lk->orb_x, lk->orb_y, 11, c565(mix(lk->low, C(255, 250, 230), 200)));
    }
    if (lk->flags & F_CLOUDS)
        for (i = 0; i < 6; i++) {
            uint32_t h = hash32((uint32_t)i + 99u);
            int32_t speed = 60 + (int32_t)(h % 60u), cx = (int32_t)(((h >> 8) % 320u + now / (uint32_t)speed) % 320u) - 40;
            int32_t cy = 18 + (int32_t)((h >> 16) % 70u), rx = 24 + (int32_t)(h % 20u), ry = 5 + (int32_t)(h >> 24) % 4;
            uint16_t cc = c565(mix(sc.sky_rgb[clamp(cy, 0, HZ - 1)], C(200, 205, 215), 90));
            for (y = -ry; y <= ry; y++) {
                int32_t w = rx * (int32_t)fm_sqrtf((float)(ry * ry - y * y) * 256.0f / (float)(ry * ry)) / 16;
                if (row_in_band(cy + y)) {
                    uint16_t *p = row_ptr(cy + y);
                    int32_t x;
                    for (x = clamp(cx - w, 0, 240); x < clamp(cx + w, 0, 240); x++)
                        if (BAYER[(cy + y) & 3][x & 3] < 10)
                            p[x] = swap16(cc);
                }
            }
        }
}

static void pine(int32_t x, int32_t base, int32_t h, int32_t sway, uint16_t c)
{
    int32_t y;
    for (y = 0; y < h; y++) {
        int32_t w = ((h - y) * 2 / 5 + 1) * (((y / 3) & 1) ? 5 : 6) / 6, off = sway * y / (h ? h : 1);
        fill(x - w + off, base - y, 2 * w, 1, c);
    }
}

static void draw_land(uint32_t now)
{
    const look_t *lk = sc.lk;
    uint16_t farc = c565(lk->far), nearc = c565(lk->near);
    int32_t x, i, sway = (int32_t)(ms_vis.gust * 4.0f * fm_sinf((float)now * 0.002f));
    for (x = 0; x < 240; x++) {
        fill(x, sc.far_h[x], 1, HZ - sc.far_h[x], farc);
        if (lk->land == L_MOUNT && sc.far_h[x] < HZ - 52)
            fill(x, sc.far_h[x], 1, 3, RGB(235, 240, 250));
    }
    if (lk->land == L_SEA)
        return;
    for (x = 0; x < 240; x++)
        fill(x, sc.near_h[x], 1, HZ - sc.near_h[x], nearc);
    if (lk->land == L_CITY) {
        for (x = 0; x < 240; x += 4)
            for (i = sc.near_h[x] + 3; i < HZ - 3; i += 5) {
                uint32_t h = hash32((uint32_t)(x * 131 + i));
                int lit = h % 5u == 0u || (sc.glow[h % 16u] > 0.3f && h % 3u == 0u);
                if (lit)
                    fill(x + 1, i, 2, 2, c565(mix(lk->near, lk->light, sc.glow[h % 16u] > 0.3f ? 256 : 200)));
            }
    } else if (lk->land == L_PINES || lk->land == L_MOUNT) {
        for (i = 0; i < 26; i++) {
            uint32_t h = hash32((uint32_t)i * 31u + 5u);
            int32_t tx = (int32_t)(h % 240u), th = 14 + (int32_t)((h >> 8) % 24u);
            pine(tx, sc.near_h[clamp(tx, 0, 239)] + 2, th, sway, nearc);
        }
    } else {
        for (i = 0; i < 18; i++) {                                   /* round trees */
            uint32_t h = hash32((uint32_t)i * 53u + 7u);
            int32_t tx = (int32_t)(h % 240u), r = 4 + (int32_t)((h >> 8) % 6u), base = sc.near_h[clamp(tx, 0, 239)];
            fill(tx, base - r, 1, r + 2, nearc);
            disc(tx + sway / 2, base - r - r / 2, r, nearc);
        }
    }
}

static void draw_lake(uint32_t now)
{
    const look_t *lk = sc.lk;
    int32_t y, x;
    rgb_t sil = mix(lk->near, lk->water, 80);
    for (y = HZ; y < WATER_END; y++) {
        int32_t d = y - HZ, m = clamp(HZ - 1 - d * 2, 0, HZ - 1), o = (int32_t)(hash32((uint32_t)(d + (int32_t)(now / 160u))) % 3u) - 1;
        uint16_t a, b, s = swap16(c565(sil)), *p;
        rgb_t ra = mix(sc.sky_rgb[m], lk->water, 110 + d), rb = mix(ra, lk->water, 60);
        if (!row_in_band(y))
            continue;
        a = swap16(c565(ra));
        b = swap16(c565(rb));
        p = row_ptr(y);
        for (x = 0; x < 240; x++) {
            int32_t xs = clamp(x + o, 0, 239), top = sc.near_h[xs] < sc.far_h[xs] ? sc.near_h[xs] : sc.far_h[xs];
            p[x] = m >= top ? s : BAYER[y & 3][x & 3] < 6 ? b : a;
        }
        if (hash32((uint32_t)(d * 31) + now / 300u) % 7u == 0u) {   /* glint */
            int32_t gx = (int32_t)(hash32((uint32_t)d + now / 300u) % 230u);
            fill(gx, y, 3 + d / 8, 1, c565(mix(ra, lk->low, 160)));
        }
        if (lk->orb && (d + (int32_t)(now / 120u)) % 3 == 0) {        /* orb reflection: open water only */
            int32_t ox = clamp(lk->orb_x, 0, 239), top = sc.near_h[ox] < sc.far_h[ox] ? sc.near_h[ox] : sc.far_h[ox];
            if (m < top && d > 4) {
                int32_t w = 1 + d / 10 + (int32_t)(hash32((uint32_t)d + now / 200u) % (uint32_t)(2 + d / 8));
                fill(lk->orb_x - w / 2, y, w, 1, c565(mix(ra, C(255, 245, 220), 120 + d * 2)));
            }
        }
        if (ms_vis.rain > 0.0f && hash32((uint32_t)(y * 977) + now / 90u) % 5u == 0u)   /* rain on the water */
            fill((int32_t)(hash32((uint32_t)y + now / 90u) % 236u), y, 3, 1, c565(mix(ra, C(220, 225, 235), 140)));
    }
    for (y = WATER_END; y < 240; y++)
        dither_row(y, 0, 240, c565(lk->ground), c565(mix(lk->ground, lk->water, 120)), y < WATER_END + 3 ? 8 : 0);
}

static void draw_sea(uint32_t now)
{
    const look_t *lk = sc.lk;
    int32_t y, beach = 198 - (int32_t)(ms_vis.wave * 16.0f);
    rgb_t sand = lk->ground, wet = mix(lk->ground, lk->water, 110);
    for (y = HZ; y < 240; y++) {
        int32_t lvl;
        uint16_t a, b;
        if (y < beach) {
            grad_at(y, HZ, 205, mix(lk->low, lk->water, 150), lk->water, 6, &a, &b, &lvl);
            dither_row(y, 0, 240, a, b, lvl);
            if (hash32((uint32_t)(y * 37) + now / 250u) % 4u == 0u) {
                int32_t gx = (int32_t)(hash32((uint32_t)y + now / 250u) % 230u);
                fill(gx, y, 2 + (y - HZ) / 10, 1, c565(mix(lk->water, lk->low, 170)));
            }
            if (lk->orb && (y + (int32_t)(now / 120u)) % 3 == 0)
                fill(lk->orb_x - 3, y, 6, 1, c565(mix(lk->water, C(255, 245, 220), 150)));
        } else if (y < beach + 3) {
            dither_row(y, 0, 240, c565(wet), RGB(240, 245, 250), 13 - (y - beach) * 4);   /* foam */
        } else if (y < beach + 10) {
            dither_row(y, 0, 240, c565(sand), c565(wet), 14 - (y - beach - 3) * 2);
        } else {
            dither_row(y, 0, 240, c565(sand), c565(mix(sand, C(255, 255, 255), 30)), 3);
        }
    }
}

static void draw_meadow(void)
{
    const look_t *lk = sc.lk;
    int32_t y, lvl;
    uint16_t a, b;
    for (y = HZ; y < 240; y++) {
        grad_at(y, HZ, 240, mix(lk->near, lk->ground, 128), lk->ground, 5, &a, &b, &lvl);
        dither_row(y, 0, 240, a, b, lvl);
    }
    for (y = 0; y < 140; y++) {                                       /* grass, taller nearer the viewer */
        uint32_t h = hash32((uint32_t)y * 977u + 13u);
        int32_t gy = HZ + 4 + (int32_t)(h % 86u), gx = (int32_t)((h >> 8) % 240u), tall = 1 + (gy - HZ) / 22;
        fill(gx, gy - tall, 1, tall, c565(mix(lk->ground, h >> 30 ? lk->near : C(200, 230, 150), 90)));
        if ((h >> 20) % 9u == 0u)
            fill(gx + 2, gy - tall, 1 + tall / 2, 1 + tall / 2, c565(mix(lk->ground, h & 1u ? C(255, 240, 150) : C(250, 250, 255), 170)));
    }
}

static int32_t stream_x(int32_t y) { return 120 + (int32_t)(55.0f * fm_sinf((float)(y - HZ) * 0.045f + 1.2f) * (float)(y - HZ) / 90.0f); }

static void draw_stream(uint32_t now)
{
    const look_t *lk = sc.lk;
    int32_t y, i;
    uint16_t wc = c565(lk->water), deep = c565(mix(lk->water, C(20, 40, 80), 90)), bank = c565(mix(lk->ground, C(60, 50, 30), 120));
    for (y = HZ + 2; y < 240; y++) {
        int32_t cx = stream_x(y), w = 2 + (y - HZ) * 2 / 5;
        fill(cx - w - 1, y, 2 * w + 2, 1, bank);
        dither_row(y, cx - w, cx + w, wc, deep, 4);
        if ((y * 7 + (int32_t)(now / 45u)) % 13 == 0) {              /* current flowing toward the viewer */
            int32_t off = (int32_t)(hash32((uint32_t)y) % (uint32_t)(2 * w + 1)) - w;
            fill(cx + off, y, 2 + w / 6, 1, RGB(235, 245, 255));
        }
    }
    for (i = 0; i < 8; i++) {                                         /* bank stones */
        int32_t y2 = HZ + 20 + i * 10, cx = stream_x(y2), w = 2 + (y2 - HZ) * 2 / 5;
        disc(cx + (i & 1 ? w + 3 : -w - 3), y2, 1 + i / 3, RGB(120, 120, 110));
    }
}

static void draw_garden(uint32_t now)
{
    static const rgb_t PETAL[4] = {CI(255, 140, 170), CI(255, 220, 100), CI(240, 240, 255), CI(190, 160, 255)};
    const look_t *lk = sc.lk;
    int32_t i, x;
    uint16_t path = c565(mix(lk->ground, C(220, 200, 160), 150));
    for (i = HZ + 4; i < 240; i++) {                                  /* path */
        int32_t w = 2 + (i - HZ) * 3 / 5;
        dither_row(i, 120 - w, 120 + w, path, c565(mix(lk->ground, C(220, 200, 160), 90)), 4);
    }
    for (x = 0; x < 240; x += 12)                                     /* fence */
        fill(x, HZ + 2, 2, 7, c565(mix(lk->near, C(240, 230, 210), 110)));
    fill(0, HZ + 4, 240, 1, c565(mix(lk->near, C(240, 230, 210), 110)));
    for (i = 0; i < 90; i++) {                                        /* flower beds */
        uint32_t h = hash32((uint32_t)i * 613u + 3u);
        int32_t y = HZ + 14 + (int32_t)(h % 84u), side = h & 256u ? 1 : -1, near = (y - HZ) / 22;
        int32_t px = 120 + side * (8 + (y - HZ) * 3 / 5 + (int32_t)((h >> 9) % 70u));
        float g = sc.glow[(uint32_t)i % 16u];
        rgb_t pc = PETAL[(h >> 20) & 3u];
        disc(px, y + 2, 2 + near, c565(mix(lk->ground, lk->near, 140)));
        fill(px - near / 2, y - near / 2, 1 + near, 1 + near, c565(mix(mix(lk->ground, pc, 150), pc, (int32_t)(g * 256.0f))));
        if (g > 0.4f)
            halo(px, y, 3 + near * 2, c565(pc), (int32_t)(g * 8.0f));
    }
    (void)now;
}

static void draw_reeds(uint32_t now)
{
    const look_t *lk = sc.lk;
    uint16_t c = c565(mix(lk->ground, C(0, 0, 0), 100));
    int32_t i;
    for (i = 0; i < 48; i++) {
        uint32_t h = hash32((uint32_t)i * 2741u + 11u);
        int32_t x = (int32_t)(h % 244u) - 2, ht = 14 + (int32_t)((h >> 8) % 44u), lean = (int32_t)((h >> 16) % 7u) - 3;
        int32_t sway = (int32_t)((0.6f + 4.0f * ms_vis.gust) * fm_sinf((float)now * 0.0016f + (float)i));
        cv_line(x, 239, x + lean + sway, 239 - ht, c);
        if (h >> 28 == 0u)
            fill(x + lean + sway - 1, 239 - ht, 2, 5, c);
    }
}

static void draw_fireflies(uint32_t now)
{
    const look_t *lk = sc.lk;
    int32_t i;
    for (i = 0; i < 12; i++) {
        uint32_t h = hash32((uint32_t)i * 4447u + 17u);
        float t = (float)now * 0.001f, ph = (float)(h & 1023u) * 0.006f;
        int32_t x = (int32_t)(h % 240u) + (int32_t)(10.0f * fm_sinf(t * 0.31f + ph));
        int32_t y = HZ - 30 + (int32_t)((h >> 10) % 80u) + (int32_t)(6.0f * fm_sinf(t * 0.53f + ph * 2.0f));
        float b = 0.5f + 0.5f * fm_sinf(t * 1.7f + ph * 3.0f), g = sc.glow[i];
        b = fm_maxf(b * b * b, g);
        if (b < 0.15f)
            continue;
        put(x, y, c565(mix(lk->near, lk->light, (int32_t)(b * 256.0f))));
        if (b > 0.5f) {
            uint16_t d = c565(mix(lk->near, lk->light, (int32_t)(b * 120.0f)));
            put(x - 1, y, d), put(x + 1, y, d), put(x, y - 1, d), put(x, y + 1, d);
        }
    }
}

static void draw_campfire(uint32_t now)
{
    int32_t i, cx = 120, base = 214;
    float f = 0.5f + ms_vis.fire;
    halo(cx, base - 4, 44, RGB(120, 60, 30), 6);
    halo(cx, base - 6, 24, RGB(200, 100, 40), 6);
    fill(cx - 14, base, 28, 4, RGB(70, 40, 25));
    fill(cx - 10, base - 3, 20, 4, RGB(90, 50, 30));
    for (i = 0; i < 7; i++) {
        int32_t h = 6 + (int32_t)(hash32((uint32_t)i * 97u + now / 70u) % (uint32_t)(6.0f + 16.0f * f));
        int32_t x = cx - 9 + i * 3;
        pine(x, base - 2, h, (int32_t)(hash32((uint32_t)i + now / 90u) % 5u) - 2, RGB(220, 70, 30));
        pine(x, base - 2, h * 2 / 3, 0, RGB(255, 150, 40));
        pine(x, base - 2, h / 3, 0, RGB(255, 230, 120));
    }
    for (i = 0; i < 6; i++) {                                         /* sparks */
        uint32_t h = hash32((uint32_t)i * 37u + 3u);
        int32_t age = (int32_t)((now / 18u + h) % 70u);
        put(cx - 6 + (int32_t)(h % 12u) + (age / 9) * ((h & 1u) ? 1 : -1), base - 10 - age, age < 40 ? RGB(255, 190, 80) : RGB(160, 80, 40));
    }
}

static void draw_weather(uint32_t now)
{
    const look_t *lk = sc.lk;
    int32_t i, n = (int32_t)(ms_vis.rain * 110.0f);
    uint16_t rc = c565(mix(lk->mid, C(220, 225, 240), 150));
    for (i = 0; i < n; i++) {
        uint32_t h = hash32((uint32_t)i * 2654435761u);
        uint32_t speed = 4u + (h >> 28);
        int32_t x = (int32_t)(((h >> 4) % 260u + 260u - (now / 30u) % 260u) % 260u);
        int32_t y = (int32_t)(((h >> 12) + now / 30u * speed) % 240u);
        cv_line(x, y, x - 1, y + 4, rc);
    }
    if (ui.flash_at && now - ui.flash_at < 220u) {
        int32_t x = 40 + (int32_t)(hash32(ui.last_flash) % 160u), y = 0;
        i = 0;
        while (y < HZ - 20 && i++ < 40) {
            int32_t nx = x + (int32_t)(hash32(ui.last_flash * 31u + (uint32_t)i) % 15u) - 7, ny = y + 6 + (int32_t)(hash32((uint32_t)i) % 8u);
            cv_line(x, y, nx, ny, RGB(255, 255, 255));
            x = nx;
            y = ny;
        }
    }
    if (ui.calls_at && now - ui.calls_at < 2500u) {                    /* a bird flies from where it called */
        int32_t age = (int32_t)(now - ui.calls_at), x = 120 + (int32_t)(ms_vis.call_pan * 90.0f) + age / 40;
        int32_t y = 40 + (int32_t)(4.0f * fm_sinf((float)age * 0.01f)), wing = (age / 120) & 1 ? 2 : 3;
        uint16_t bc = lk->top.r + lk->top.g > 200 ? RGB(40, 40, 50) : RGB(220, 220, 230);
        cv_line(x - 4, y - wing, x, y, bc);
        cv_line(x, y, x + 4, y - wing, bc);
    }
    if (ui.chirp_at && now - ui.chirp_at < 160u)
        put((int32_t)(ms_vis.chirps * 71u % 230u) + 5, 225 + (int32_t)(ms_vis.chirps % 12u), lk->light.r ? c565(lk->light) : RGB(200, 255, 120));
    if (lk->flags & F_BUTTERFLIES)
        for (i = 0; i < 4; i++) {
            float t = (float)now * 0.001f + (float)i * 9.0f;
            int32_t x = 30 + i * 60 + (int32_t)(30.0f * fm_sinf(t * 0.4f)), y = 166 + (i & 1) * 14 + (int32_t)(14.0f * fm_sinf(t * 0.7f));
            uint16_t wc = (i & 1) ? RGB(255, 230, 120) : (i & 2) ? RGB(255, 170, 200) : RGB(255, 255, 255);
            put(x - 1, y, wc), put(x + 1, y, wc);
            if ((now / 140u + (uint32_t)i) & 1u)
                put(x - 1, y - 1, wc), put(x + 1, y - 1, wc);
        }
}

static void scene_notes(void)
{
    uint32_t n = ms_vis.notes, i;
    for (i = 0; i < 16u; i++)
        sc.glow[i] *= 0.93f;
    if (n - sc.seen_notes > 16u)
        sc.seen_notes = n - 16u;
    while (sc.seen_notes != n) {
        sc.seen_notes++;
        sc.glow[(ms_vis.recent[sc.seen_notes & 15u] & 0xFFu) * 5u % 16u] = 1.0f;
    }
}

static void loop_bar(uint32_t now)
{
    static const char *const NAME[4] = {"", "REC", "FADING", "ADD"};
    uint32_t st = ms_vis.lp_state & 3u;
    const char *hint = st == MS_LOOP_REC ? (set.keys[3] == 1 ? "PAUSE TO LOOP. REC KEEPS IT" : "REC AGAIN TO KEEP IT")
                     : st == MS_LOOP_DUB ? "REC AGAIN TO KEEP IT" : "";
    uint16_t red = RGB(240, 96, 96), cream = RGB(236, 228, 206), c = st == MS_LOOP_PLAY ? cream : red;
    int32_t x0 = 54, w = 132, pos, t;
    if (hint[0]) {                            /* while it waits on you, it says what to do next */
        int32_t w = text_w(&FONT_T, hint) + 16;
        pill(120 - w / 2, 198, w, 16, RGB(14, 14, 22));
        cv_text(128 - w / 2, 200, &FONT_T, hint, red);
    }
    pill(10, 216, 220, 18, RGB(14, 14, 22));
    cv_text(16, 219, &FONT_T, ms_vis.lp_adding ? "ADD" : st == MS_LOOP_PLAY && ms_vis.lp_keep ? "KEPT" : NAME[st],
            ms_vis.lp_adding ? red : c);
    fill(x0, 223, w, 4, RGB(50, 50, 62));
    for (t = 0; t < MS_NLAYERS; t++) {        /* the layers: lit, dim when muted, dark when empty */
        int used = ms_vis.lp_tracks >> t & 1u, muted = (ms_vis.lp_mute >> t & 1u) || !ui.track_lvl[t];
        fill(190 + t * 5, 221, 3, 8, !used ? RGB(50, 50, 62) : muted ? RGB(110, 105, 95) : cream);
    }
    if (st == MS_LOOP_REC) {                  /* no length yet, so pulse */
        if (now / 300u & 1u)
            fill(x0, 223, w, 4, red);
        return;
    }
    pos = (int32_t)((float)w * ms_vis.lp_pos);
    fill(x0, 223, pos, 4, c);
    fill(x0 + pos - 1, 221, 3, 8, RGB(255, 255, 255));
}

static void scene_overlay(uint32_t now)
{
    char t[64];
    int32_t y;
    if ((int32_t)(ui.label_until - now) > 0) {
        t[0] = 0;
        text_cat(t, MS_MODE_NAME[set.mode]);
        text_cat(t, "  ");
        text_cat(t, ms_scene(set.mode, set.scene[set.mode])->name);
        pill(6, 6, text_w(&FONT_T, t) + 16, 16, RGB(14, 14, 22));
        cv_text(14, 8, &FONT_T, t, RGB(236, 228, 206));
    }
    if ((int32_t)(ui.msg_until - now) > 0) {
        int32_t w;
        msg_join(t);
        {
            const felucca_font_t *f = text_w(&FONT_B, t) > 210 ? &FONT_T : &FONT_B;   /* long ones, smaller */
            w = text_w(f, t) + 20;
            y = ms_vis.lp_state == MS_LOOP_EMPTY ? 214 : ms_vis.lp_state == MS_LOOP_PLAY ? 192 : 174;
            pill(120 - w / 2, y, w, 20, RGB(14, 14, 22));
            cv_text(130 - w / 2, y + (f == &FONT_B ? 2 : 4), f, t, RGB(236, 228, 206));
        }
    }
    if (ms_vis.lp_state != MS_LOOP_EMPTY)
        loop_bar(now);
    if (ui.frozen)
        cv_text(196, 8, &FONT_T, "FROZEN", RGB(220, 235, 255));
    else if (!ui.playing)
        cv_text(190, 8, &FONT_T, "RESTING", RGB(200, 200, 210));
}

static void scene_draw(uint32_t now)
{
    int half;
    if (!sc.lk || sc.built_mode != set.mode || sc.built_scene != set.scene[set.mode])
        scene_build(set.mode, set.scene[set.mode]);
    scene_notes();
    sky_rows(now);
    for (half = 0; half < 2; half++) {
        band_begin(half, C_BLACK);
        draw_sky(now);
        draw_land(now);
        switch (sc.lk->water_kind) {
        case W_LAKE:
            draw_lake(now);
            break;
        case W_SEA:
            draw_sea(now);
            break;
        case W_STREAM:
            draw_meadow();
            draw_stream(now);
            break;
        default:
            draw_meadow();
            break;
        }
        if (sc.lk->land == L_GARDEN)
            draw_garden(now);
        if (sc.lk->flags & F_CAMPFIRE)
            draw_campfire(now);
        if (sc.lk->flags & F_REEDS)
            draw_reeds(now);
        if (sc.lk->flags & F_FIREFLIES)
            draw_fireflies(now);
        draw_weather(now);
        scene_overlay(now);
        band_end();
    }
}
