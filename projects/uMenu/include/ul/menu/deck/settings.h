#pragma once

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DH_FMT_SQUARE = 0, DH_FMT_CAPSULE = 1 };
enum { DH_SCALE_COVER = 0, DH_SCALE_CONTAIN = 1, DH_SCALE_STRETCH = 2 };
enum { DH_SIZE_LARGE = 0, DH_SIZE_MEDIUM = 1, DH_SIZE_SMALL = 2 };
enum { DH_BORDER_THIN = 0, DH_BORDER_MEDIUM = 1, DH_BORDER_THICK = 2 };
enum { DH_EDGE_WRAP = 0, DH_EDGE_ROW = 1, DH_EDGE_CATEGORY = 2 };
enum { DH_STICK_CARDINAL = 0, DH_STICK_DIAGONAL = 1 };
enum { DH_SORT_RECENT = 0, DH_SORT_AZ = 1, DH_SORT_ZA = 2, DH_SORT_CUSTOM = 3 };
enum { DH_COVER_UDECK = 0, DH_COVER_SGDB = 1 };

typedef struct {
    int format;
    int scale;
    int size;
} DhGridStyle;

typedef struct {
    DhGridStyle games;
    DhGridStyle hb;
    int border;
    int edge;
    int stick_diag;
    int sort;
    int sgdb;
    int sgdb_key_ok;
    int cover_src;
    char sgdb_key[128];
    int sfx_master;
    int sfx_nav;
    int sfx_launch;
    int sfx_startup;
    int privacy_covers;
    char lang[16];
} DhSettings;

void dh_settings_load(DhSettings *s);
void dh_settings_save(const DhSettings *s);

u64 dh_recent_game(u64 app_id);
void dh_recent_touch_game(u64 app_id);
u64 dh_recent_hb(const char *path);
void dh_recent_touch_hb(const char *path);

int dh_order_load_games(u64 *ids, int max);
void dh_order_save_games(const u64 *ids, int n);
int dh_order_load_hb(char (*paths)[768], int max);
void dh_order_save_hb(char (*paths)[768], int n);

#ifdef __cplusplus
}
#endif
