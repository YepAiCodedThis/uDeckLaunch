#include "draw.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL_image.h>

#define DH_TCACHE 192
#define DH_CORNER 32
#define DH_CIRCLE 64

typedef struct {
    TTF_Font *font;
    SDL_Color c;
    char msg[160];
    SDL_Texture *tex;
    int w, h;
    uint32_t used;
} DhTEnt;

static SDL_Renderer *g_r;
static SDL_Texture *g_corner;
static SDL_Texture *g_circle;
static DhTEnt g_tcache[DH_TCACHE];
static uint32_t g_tick;
static int g_draw_refs;

static uint32_t tkey(TTF_Font *font, const char *msg, SDL_Color c)
{
    uint32_t h = (uint32_t)(uintptr_t)font;
    h ^= ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
    for (int i = 0; msg[i]; i++) {
        h = h * 16777619u ^ (uint8_t)msg[i];
    }
    return h;
}

void dh_draw_init(SDL_Renderer *r)
{
    if (!r) {
        return;
    }
    if (g_r) {
        g_draw_refs++;
        return;
    }
    g_r = r;
    g_tick = 1;
    g_draw_refs = 1;
    memset(g_tcache, 0, sizeof(g_tcache));

    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, DH_CORNER, DH_CORNER, 32, SDL_PIXELFORMAT_RGBA32);
    if (s) {
        Uint32 *px = (Uint32 *)s->pixels;
        float rad = (float)DH_CORNER;
        for (int j = 0; j < DH_CORNER; j++) {
            for (int i = 0; i < DH_CORNER; i++) {
                float ddx = (i + 0.5f) - rad;
                float ddy = (j + 0.5f) - rad;
                float dist = sqrtf(ddx * ddx + ddy * ddy);
                float a = rad - dist;
                Uint8 al = 0;
                if (a >= 1.f) {
                    al = 255;
                } else if (a > 0.f) {
                    al = (Uint8)(a * 255.f);
                }
                px[j * (s->pitch / 4) + i] = (Uint32)al << 24 | 0x00FFFFFFu;
            }
        }
        g_corner = SDL_CreateTextureFromSurface(r, s);
        SDL_FreeSurface(s);
        if (g_corner) {
            SDL_SetTextureBlendMode(g_corner, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(g_corner, SDL_ScaleModeLinear);
        }
    }

    SDL_Surface *c = SDL_CreateRGBSurfaceWithFormat(0, DH_CIRCLE, DH_CIRCLE, 32, SDL_PIXELFORMAT_RGBA32);
    if (c) {
        Uint32 *px = (Uint32 *)c->pixels;
        float cx = (DH_CIRCLE - 1) * 0.5f;
        float cy = (DH_CIRCLE - 1) * 0.5f;
        float rad = DH_CIRCLE * 0.5f - 0.5f;
        for (int j = 0; j < DH_CIRCLE; j++) {
            for (int i = 0; i < DH_CIRCLE; i++) {
                float ddx = (i + 0.5f) - cx;
                float ddy = (j + 0.5f) - cy;
                float dist = sqrtf(ddx * ddx + ddy * ddy);
                float a = rad - dist;
                Uint8 al = 0;
                if (a >= 1.f) {
                    al = 255;
                } else if (a > 0.f) {
                    al = (Uint8)(a * 255.f);
                }
                px[j * (c->pitch / 4) + i] = (Uint32)al << 24 | 0x00FFFFFFu;
            }
        }
        g_circle = SDL_CreateTextureFromSurface(r, c);
        SDL_FreeSurface(c);
        if (g_circle) {
            SDL_SetTextureBlendMode(g_circle, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(g_circle, SDL_ScaleModeLinear);
        }
    }
}

int dh_draw_is_init(void)
{
    return g_r != NULL;
}

void dh_draw_shutdown(void)
{
    if (g_draw_refs > 1) {
        g_draw_refs--;
        return;
    }
    g_draw_refs = 0;
    for (int i = 0; i < DH_TCACHE; i++) {
        if (g_tcache[i].tex) {
            SDL_DestroyTexture(g_tcache[i].tex);
            g_tcache[i].tex = NULL;
        }
    }
    if (g_corner) {
        SDL_DestroyTexture(g_corner);
        g_corner = NULL;
    }
    if (g_circle) {
        SDL_DestroyTexture(g_circle);
        g_circle = NULL;
    }
    g_r = NULL;
}

void dh_fill(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

void dh_round_rect(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color c)
{
    if (rad < 1 || w < 2 || h < 2) {
        dh_fill(r, x, y, w, h, c);
        return;
    }
    if (rad * 2 > w) {
        rad = w / 2;
    }
    if (rad * 2 > h) {
        rad = h / 2;
    }
    dh_fill(r, x + rad, y, w - rad * 2, h, c);
    dh_fill(r, x, y + rad, rad, h - rad * 2, c);
    dh_fill(r, x + w - rad, y + rad, rad, h - rad * 2, c);
    if (!g_corner) {
        return;
    }
    SDL_SetTextureColorMod(g_corner, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(g_corner, c.a);
    SDL_Rect src = {0, 0, DH_CORNER, DH_CORNER};
    SDL_Rect tl = {x, y, rad, rad};
    SDL_Rect tr = {x + w - rad, y, rad, rad};
    SDL_Rect bl = {x, y + h - rad, rad, rad};
    SDL_Rect br = {x + w - rad, y + h - rad, rad, rad};
    SDL_RenderCopy(r, g_corner, &src, &tl);
    SDL_RenderCopyEx(r, g_corner, &src, &tr, 0, NULL, SDL_FLIP_HORIZONTAL);
    SDL_RenderCopyEx(r, g_corner, &src, &bl, 0, NULL, SDL_FLIP_VERTICAL);
    SDL_RenderCopyEx(r, g_corner, &src, &br, 0, NULL, (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL));
}

void dh_circle(SDL_Renderer *r, int x, int y, int d, SDL_Color c)
{
    if (d < 2) {
        return;
    }
    if (!g_circle) {
        dh_round_rect(r, x, y, d, d, d / 2, c);
        return;
    }
    SDL_SetTextureColorMod(g_circle, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(g_circle, c.a);
    SDL_Rect dst = {x, y, d, d};
    SDL_RenderCopy(r, g_circle, NULL, &dst);
}

void dh_round_outline(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color c, int width)
{
    (void)rad;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int i = 0; i < width; i++) {
        SDL_Rect rc = {x + i, y + i, w - i * 2, h - i * 2};
        SDL_RenderDrawRect(r, &rc);
    }
}

void dh_glow_rect(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color accent)
{
    SDL_Color glow = accent;
    for (int i = 8; i >= 1; i--) {
        glow.a = (Uint8)(18 * (9 - i));
        dh_round_outline(r, x - i, y - i, w + i * 2, h + i * 2, rad + i, glow, 1);
    }
    SDL_Color inner = {255, 255, 255, 220};
    dh_round_outline(r, x, y, w, h, rad, accent, 3);
    dh_round_outline(r, x + 4, y + 4, w - 8, h - 8, rad > 4 ? rad - 4 : 2, inner, 1);
}

static DhTEnt *tlookup(TTF_Font *font, const char *msg, SDL_Color c, int create)
{
    if (!g_r || !font || !msg || !msg[0]) {
        return NULL;
    }
    uint32_t h = tkey(font, msg, c);
    int slot = (int)(h % DH_TCACHE);
    int worst = slot;
    uint32_t worst_u = UINT32_MAX;
    for (int n = 0; n < 12; n++) {
        int i = (slot + n) % DH_TCACHE;
        DhTEnt *e = &g_tcache[i];
        if (e->tex && e->font == font && e->c.r == c.r && e->c.g == c.g && e->c.b == c.b &&
            strcmp(e->msg, msg) == 0) {
            e->used = ++g_tick;
            return e;
        }
        if (!e->tex) {
            worst = i;
            worst_u = 0;
            break;
        }
        if (e->used < worst_u) {
            worst_u = e->used;
            worst = i;
        }
    }
    if (!create) {
        return NULL;
    }
    DhTEnt *e = &g_tcache[worst];
    if (e->tex) {
        SDL_DestroyTexture(e->tex);
        e->tex = NULL;
    }
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, msg, c);
    if (!s) {
        return NULL;
    }
    e->tex = SDL_CreateTextureFromSurface(g_r, s);
    e->w = s->w;
    e->h = s->h;
    SDL_FreeSurface(s);
    if (!e->tex) {
        return NULL;
    }
    e->font = font;
    e->c = c;
    snprintf(e->msg, sizeof(e->msg), "%s", msg);
    e->used = ++g_tick;
    return e;
}

void dh_text(SDL_Renderer *r, TTF_Font *font, const char *msg, int x, int y, SDL_Color c)
{
    (void)r;
    DhTEnt *e = tlookup(font, msg, c, 1);
    if (!e || !e->tex) {
        return;
    }
    SDL_Rect dst = {x, y, e->w, e->h};
    SDL_RenderCopy(g_r, e->tex, NULL, &dst);
}

int dh_text_width(TTF_Font *font, const char *msg)
{
    return dh_text_size(font, msg, NULL);
}

int dh_text_size(TTF_Font *font, const char *msg, int *h)
{
    if (h) {
        *h = 0;
    }
    if (!font || !msg) {
        return 0;
    }
    SDL_Color dummy = {255, 255, 255, 255};
    DhTEnt *e = tlookup(font, msg, dummy, 0);
    if (e) {
        if (h) {
            *h = e->h;
        }
        return e->w;
    }
    int w = 0, hh = 0;
    TTF_SizeUTF8(font, msg, &w, &hh);
    if (h) {
        *h = hh;
    }
    return w;
}

void dh_text_right(SDL_Renderer *r, TTF_Font *font, const char *msg, int right, int y, SDL_Color c)
{
    int w = dh_text_width(font, msg);
    dh_text(r, font, msg, right - w, y, c);
}

SDL_Texture *dh_texture_from_mem(SDL_Renderer *r, const void *data, int len, int *w, int *h)
{
    if (!r || !data || len < 4) {
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromConstMem(data, len);
    if (!rw) {
        return NULL;
    }
    SDL_Surface *surf = IMG_Load_RW(rw, 1);
    if (!surf) {
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    }
    if (w) {
        *w = surf->w;
    }
    if (h) {
        *h = surf->h;
    }
    SDL_FreeSurface(surf);
    return tex;
}

void dh_cover(SDL_Renderer *r, SDL_Texture *tex, int tw, int th, int dx, int dy, int dw, int dh)
{
    if (!tex || tw < 1 || th < 1 || dw < 1 || dh < 1) {
        return;
    }
    float sx = (float)dw / (float)tw;
    float sy = (float)dh / (float)th;
    float s = sx > sy ? sx : sy;
    int sw = (int)((float)dw / s + 0.5f);
    int sh = (int)((float)dh / s + 0.5f);
    if (sw < 1) {
        sw = 1;
    }
    if (sh < 1) {
        sh = 1;
    }
    if (sw > tw) {
        sw = tw;
    }
    if (sh > th) {
        sh = th;
    }
    SDL_Rect src = {(tw - sw) / 2, (th - sh) / 2, sw, sh};
    SDL_Rect dst = {dx, dy, dw, dh};
    SDL_RenderCopy(r, tex, &src, &dst);
}

void dh_stretch(SDL_Renderer *r, SDL_Texture *tex, int dx, int dy, int dw, int dh)
{
    if (!tex || dw < 1 || dh < 1) {
        return;
    }
    SDL_Rect dst = {dx, dy, dw, dh};
    SDL_RenderCopy(r, tex, NULL, &dst);
}

void dh_contain(SDL_Renderer *r, SDL_Texture *tex, int tw, int th, int dx, int dy, int dw, int dh)
{
    if (!tex || tw < 1 || th < 1 || dw < 1 || dh < 1) {
        return;
    }
    float sx = (float)dw / (float)tw;
    float sy = (float)dh / (float)th;
    float s = sx < sy ? sx : sy;
    int ow = (int)((float)tw * s + 0.5f);
    int oh = (int)((float)th * s + 0.5f);
    SDL_Rect dst = {dx + (dw - ow) / 2, dy + (dh - oh) / 2, ow, oh};
    SDL_RenderCopy(r, tex, NULL, &dst);
}

void dh_card_shadow(SDL_Renderer *r, int x, int y, int w, int h, float t)
{
    int y0 = y + h - 4 + (int)(8.f * t);
    Uint8 a = (Uint8)(50.f + 40.f * t);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, a);
    SDL_Rect rc = {x + 10, y0, w - 20, 10 + (int)(6.f * t)};
    SDL_RenderFillRect(r, &rc);
}
