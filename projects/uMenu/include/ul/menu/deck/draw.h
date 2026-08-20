#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include "theme.h"

void dh_draw_init(SDL_Renderer *r);
void dh_draw_shutdown(void);
int dh_draw_is_init(void);

void dh_fill(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c);
void dh_round_rect(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color c);
void dh_circle(SDL_Renderer *r, int x, int y, int d, SDL_Color c);
void dh_round_outline(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color c, int width);
void dh_glow_rect(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color accent);
void dh_text(SDL_Renderer *r, TTF_Font *font, const char *msg, int x, int y, SDL_Color c);
void dh_text_right(SDL_Renderer *r, TTF_Font *font, const char *msg, int right, int y, SDL_Color c);
int dh_text_width(TTF_Font *font, const char *msg);
int dh_text_size(TTF_Font *font, const char *msg, int *h);

SDL_Texture *dh_texture_from_mem(SDL_Renderer *r, const void *data, int len, int *w, int *h);
void dh_cover(SDL_Renderer *r, SDL_Texture *tex, int tw, int th, int dx, int dy, int dw, int dh);
void dh_contain(SDL_Renderer *r, SDL_Texture *tex, int tw, int th, int dx, int dy, int dw, int dh);
void dh_stretch(SDL_Renderer *r, SDL_Texture *tex, int dx, int dy, int dw, int dh);
void dh_card_shadow(SDL_Renderer *r, int x, int y, int w, int h, float t);
