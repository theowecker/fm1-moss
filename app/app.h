/* SPDX-License-Identifier: MPL-2.0 */
/* The app's half of the fm1-base contract (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "moss.h"

#define APP_NAME "MOSS-1"
#define APP_SPLASH_RGB RGB(120, 200, 120)
#define APP_PALETTE 0                    /* GREEN */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_OVERLOAD                     /* base calls app_overload when the render runs late */
void app_overload(void);

enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_SLOTS, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xD8000u}
#define APP_SYSEX                        /* loops out to the computer: F0 7D 4D 4F ("MO") */
int app_sysex(const uint8_t *p, uint32_t n);

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
