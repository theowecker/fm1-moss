/* SPDX-License-Identifier: MPL-2.0 */
/* The canvas holds at most 240 x 144, so the screen is drawn twice in absolute coordinates, as two bands of 120
 * rows; each band sets cv_oy so drawing clips to it. */

typedef struct {
    uint8_t r, g, b;
} rgb_t;

static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static int32_t band_y0, band_y1;

static uint16_t c565(rgb_t c) { return RGB(c.r, c.g, c.b); }

static rgb_t mix(rgb_t a, rgb_t b, int32_t t256)
{
    rgb_t c;
    t256 = clamp(t256, 0, 256);
    c.r = (uint8_t)(a.r + ((b.r - a.r) * t256 >> 8));
    c.g = (uint8_t)(a.g + ((b.g - a.g) * t256 >> 8));
    c.b = (uint8_t)(a.b + ((b.b - a.b) * t256 >> 8));
    return c;
}

static void band_begin(int half, uint16_t bg)
{
    cv_begin(240, 120, bg);
    band_y0 = 120 * half;
    band_y1 = band_y0 + 120;
    cv_oy = -band_y0;
    cv_y0 = 0;
    cv_y1 = 120;
}

static void band_end(void) { cv_blit(0, (uint32_t)band_y0); }

static inline int row_in_band(int32_t y) { return y >= band_y0 && y < band_y1; }
static inline uint16_t *row_ptr(int32_t y) { return cv_px + (uint32_t)(y - band_y0) * 240u; }

/* level/16 of the pixels get b, the rest a, by ordered dither */
static void dither_row(int32_t y, int32_t x0, int32_t x1, uint16_t a, uint16_t b, int32_t level)
{
    uint16_t *p, sa = swap16(a), sb = swap16(b);
    const uint8_t *bay = BAYER[y & 3];
    int32_t x;
    if (!row_in_band(y))
        return;
    p = row_ptr(y);
    x0 = clamp(x0, 0, 240);
    x1 = clamp(x1, 0, 240);
    for (x = x0; x < x1; x++)
        p[x] = bay[x & 3] < level ? sb : sa;
}

/* a gradient in `steps` flat bands with dithered edges */
static void grad_at(int32_t y, int32_t y0, int32_t y1, rgb_t top, rgb_t bot, int32_t steps, uint16_t *a, uint16_t *b,
                    int32_t *level)
{
    int32_t pos = (y - y0) * steps * 16 / (y1 - y0 > 0 ? y1 - y0 : 1), band = pos >> 4;
    *a = c565(mix(top, bot, band * 256 / steps));
    *b = c565(mix(top, bot, (band + 1) * 256 / steps));
    *level = pos & 15;
}

static void put(int32_t x, int32_t y, uint16_t c) { cv_pset(x, y, c); }

static void fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y, w, h, c); }

static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t y;
    for (y = -r; y <= r; y++) {
        int32_t w = (int32_t)fm_sqrtf((float)(r * r - y * y) + 0.3f);
        cv_rect(cx - w, cy + y, 2 * w + 1, 1, c);
    }
}

static void circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x = r, y = 0, err = 1 - r;
    while (x >= y) {
        put(cx + x, cy + y, c), put(cx - x, cy + y, c), put(cx + x, cy - y, c), put(cx - x, cy - y, c);
        put(cx + y, cy + x, c), put(cx - y, cy + x, c), put(cx + y, cy - x, c), put(cx - y, cy - x, c);
        y++;
        if (err < 0)
            err += 2 * y + 1;
        else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

static void halo(int32_t cx, int32_t cy, int32_t r, uint16_t c, int32_t strength)
{
    int32_t x, y;
    for (y = -r; y <= r; y++) {
        if (!row_in_band(cy + y))
            continue;
        for (x = -r; x <= r; x++) {
            int32_t d2 = x * x + y * y, lvl;
            if (d2 > r * r)
                continue;
            lvl = strength * (r * r - d2) / (r * r);
            if (BAYER[(cy + y) & 3][(cx + x) & 3] < lvl)
                put(cx + x, cy + y, c);
        }
    }
}

static void pill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t r = h / 2;
    fill(x + r, y, w - 2 * r, h, c);
    disc(x + r, y + r, r, c);
    disc(x + w - r - 1, y + r, r, c);
}

/* 270 degrees clockwise from lower left */
static void arc(int32_t cx, int32_t cy, int32_t r, int32_t pct, uint16_t on, uint16_t track)
{
    int32_t i, n = 40;
    for (i = 0; i <= n; i++) {
        float a = (0.375f + 0.75f * (float)i / (float)n) * FM_TWO_PI;
        int32_t x = cx + (int32_t)((float)r * fm_cosf(a) + 0.5f), y = cy + (int32_t)((float)r * fm_sinf(a) + 0.5f);
        uint16_t c = i * 100 <= pct * n ? on : track;
        put(x, y, c);
        put(x + (x < cx ? 1 : -1), y, c);
    }
}

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static void centred(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(120 - text_w(f, s) / 2, y, f, s, c);
}
