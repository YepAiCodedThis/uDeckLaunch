#pragma once

#include <switch.h>
#include <SDL.h>
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DH_USERPICK_MAX ACC_USER_LIST_SIZE

typedef enum {
    DH_USERPICK_NONE = 0,
    DH_USERPICK_SELECT,
    DH_USERPICK_ADD
} DhUserpickAction;

typedef struct DhUserpick DhUserpick;

DhUserpick *dh_userpick_create(SDL_Renderer *r, DhFonts *fonts, const DhTheme *th);
void dh_userpick_destroy(DhUserpick *up);
void dh_userpick_detach_titles(DhUserpick *up);
void dh_userpick_set_title(DhUserpick *up, const char *title);
void dh_userpick_set_logo(DhUserpick *up, SDL_Texture *tex);
void dh_userpick_reload(DhUserpick *up);
void dh_userpick_tick(DhUserpick *up, SDL_Renderer *r);
void dh_userpick_draw(DhUserpick *up, SDL_Renderer *r);
DhUserpickAction dh_userpick_handle_input(DhUserpick *up, u64 keys_down, int touch_x, int touch_y);
int dh_userpick_selected_uid(const DhUserpick *up, AccountUid *out);

#ifdef __cplusplus
}
#endif
