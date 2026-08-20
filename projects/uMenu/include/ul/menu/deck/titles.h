#pragma once

#include <switch.h>
#include <SDL.h>

#define DH_TITLES_MAX 256
#define DH_INFO_DLC_MAX 8

typedef enum {
    DH_ICON_NONE = 0,
    DH_ICON_READY,
    DH_ICON_FAIL
} DhIconState;

typedef struct {
    u64 application_id;
    char id_hex[17];
    char name[0x200];
    char publisher[0x100];
    char version[0x20];
    u64 last_play;
    Uint8 cr, cg, cb;
    int meta_loaded;
    SDL_Texture *icon;
    int icon_w;
    int icon_h;
    DhIconState icon_state;
} DhTitle;

typedef struct {
    char name[0x200];
    char publisher[0x100];
    char version[0x20];
    char id_hex[17];
    char update[48];
    char size[32];
    int dlc_n;
    char dlc[DH_INFO_DLC_MAX][80];
} DhTitleInfo;

int dh_titles_init(void);
void dh_titles_exit(void);
int dh_titles_ok(void);
int dh_titles_refresh(void);
int dh_titles_count(void);
DhTitle *dh_titles_get(int index);
void dh_titles_pump_icons(SDL_Renderer *r, int vis_lo, int vis_hi, int max_n);
void dh_titles_apply_sort(int sort);
void dh_titles_swap(int a, int b);
void dh_titles_save_custom(void);
void dh_titles_touch(u64 app_id);
int dh_titles_fill_info(u64 app_id, DhTitleInfo *out);
