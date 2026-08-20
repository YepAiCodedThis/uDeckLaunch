#pragma once

#include <switch.h>
#include <SDL.h>

#define DH_HB_MAX 128
#define DH_HB_PATH_MAX 768

typedef enum {
    DH_HB_ICON_NONE = 0,
    DH_HB_ICON_READY,
    DH_HB_ICON_FAIL
} DhHbIconState;

typedef struct {
    char path[DH_HB_PATH_MAX];
    char name[256];
    char publisher[128];
    char version[32];
    u64 size;
    u64 last_play;
    SDL_Texture *icon;
    int icon_w;
    int icon_h;
    Uint8 cr, cg, cb;
    DhHbIconState icon_state;
} DhHb;

int dh_hb_scan(void);
int dh_hb_ensure(void);
void dh_hb_exit(void);
int dh_hb_count(void);
DhHb *dh_hb_get(int index);
void dh_hb_pump_icons(SDL_Renderer *r, int vis_lo, int vis_hi, int max_n);
void dh_hb_apply_sort(int sort);
void dh_hb_swap(int a, int b);
void dh_hb_save_custom(void);
void dh_hb_touch(const char *path);
int dh_hb_uninstall(int index);
