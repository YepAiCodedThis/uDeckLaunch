#pragma once

#include <switch.h>
#include <SDL.h>
#include "settings.h"

#define DH_SGDB_PICK_MAX 50

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int busy;
    int queued;
    int done;
    int total;
    int fail;
    int pick_n;
    int pick_ready;
    char phase[48];
    char current[80];
    char error[128];
} DhSgdbStatus;

void dh_sgdb_apply(const DhSettings *s);
void dh_sgdb_exit(void);
void dh_sgdb_enqueue_all(int skip_existing);
void dh_sgdb_hint_visible(const u64 *ids, int n);
void dh_sgdb_pump(SDL_Renderer *r);
SDL_Texture *dh_sgdb_icon(u64 app_id, int *w, int *h);
int dh_sgdb_busy(void);
int dh_sgdb_done_count(void);
int dh_sgdb_has_cover(u64 app_id);
void dh_sgdb_status(DhSgdbStatus *out);
void dh_sgdb_test_key(void);
void dh_sgdb_redeem_easyapi(const char *code);
int dh_sgdb_copy_key(char *out, size_t cap);
int dh_sgdb_take_imported(char *out, size_t cap);
void dh_sgdb_reset_all(void);
void dh_sgdb_pick_begin(u64 app_id, const char *name);
SDL_Texture *dh_sgdb_pick_icon(int i, int *w, int *h);
const char *dh_sgdb_pick_label(int i);
int dh_sgdb_pick_apply(int i, u64 app_id);
void dh_sgdb_pick_clear(void);

#ifdef __cplusplus
}
#endif
