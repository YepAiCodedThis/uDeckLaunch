#pragma once

#include <switch.h>
#include <SDL.h>
#ifdef __cplusplus
struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;
#else
#include <SDL_ttf.h>
#endif
#include "theme.h"

typedef struct {
    TTF_Font *small;
    TTF_Font *medium;
    TTF_Font *large;
    TTF_Font *bold;
    TTF_Font *sub;
    TTF_Font *clock;
    TTF_Font *hint;
    TTF_Font *sidebar;
} DhFonts;

typedef struct DhUi DhUi;

DhUi *dh_ui_create(SDL_Renderer *r, DhFonts *fonts, const DhTheme *th);
void dh_ui_destroy(DhUi *ui);
void dh_ui_reload_user(DhUi *ui);
void dh_ui_handle_down(DhUi *ui, u64 keys);
void dh_ui_handle_input(DhUi *ui, u64 keys_down, u64 keys_held);
void dh_ui_set_held(DhUi *ui, u64 keys);
int dh_ui_wants_exit(const DhUi *ui);
int dh_ui_is_confirm(const DhUi *ui);
void dh_ui_tick(DhUi *ui, SDL_Renderer *r);
void dh_ui_draw(DhUi *ui, SDL_Renderer *r);
