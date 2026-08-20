#include "userpick.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_ttf.h>

#include "draw.h"
#include "layout.h"
#include "settings.h"
#include "sfx.h"
#include "titles.h"
#include <SDL_image.h>

#define UP_TILE 180
#define UP_GAP 28
#define UP_FOCUS 12
#define UP_BORDER 4
#define UP_PLUS_ARM 56
#define UP_PLUS_THICK 10
#define UP_TITLE_PX 56
#define UP_LOGO_H 96
#define UP_TILE_Y 500
#define UP_TITLE_Y 300
#define UP_MOSAIC_COLS 12
#define UP_MOSAIC_ROWS 7

typedef struct {
    AccountUid uid;
    char name[0x20];
    SDL_Texture *icon;
    int icon_w;
    int icon_h;
} DhUserSlot;

struct DhUserpick {
    SDL_Renderer *r;
    DhFonts *fonts;
    const DhTheme *th;
    TTF_Font *title_font;
    SDL_Texture *logo;
    int logo_w;
    int logo_h;
    char title[96];
    DhUserSlot users[DH_USERPICK_MAX];
    int user_n;
    int focus;
    int owned_draw;
    int owned_titles;
    int privacy_covers;
    SDL_Texture *privacy;
    int privacy_w;
    int privacy_h;
    SDL_Texture *privacy_sq[5];
    int privacy_sq_w[5];
    int privacy_sq_h[5];
};

static TTF_Font *open_title_font(void)
{
    const char *paths[] = {
        "sdmc:/switch/DeckHome/fonts/MotivaSans-ExtraBold.ttf",
        "romfs:/fonts/MotivaSans-ExtraBold.ttf",
        "sdmc:/switch/DeckHome/fonts/Inter.ttf",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        TTF_Font *f = TTF_OpenFont(paths[i], UP_TITLE_PX);
        if (f) {
            return f;
        }
    }
    return NULL;
}

static void free_users(DhUserpick *up)
{
    for (int i = 0; i < up->user_n; i++) {
        if (up->users[i].icon) {
            SDL_DestroyTexture(up->users[i].icon);
            up->users[i].icon = NULL;
        }
    }
    memset(up->users, 0, sizeof(up->users));
    up->user_n = 0;
}

static void load_users(DhUserpick *up)
{
    free_users(up);
    AccountUid uids[DH_USERPICK_MAX];
    s32 count = 0;
    if (R_FAILED(accountListAllUsers(uids, DH_USERPICK_MAX, &count)) || count < 1) {
        up->focus = 0;
        return;
    }
    if (count > DH_USERPICK_MAX) {
        count = DH_USERPICK_MAX;
    }
    for (s32 i = 0; i < count; i++) {
        DhUserSlot *s = &up->users[up->user_n];
        memset(s, 0, sizeof(*s));
        s->uid = uids[i];

        AccountProfile prof;
        if (R_FAILED(accountGetProfile(&prof, uids[i]))) {
            continue;
        }
        AccountProfileBase pbase;
        AccountUserData udata;
        if (R_SUCCEEDED(accountProfileGet(&prof, &udata, &pbase)) && pbase.nickname[0]) {
            snprintf(s->name, sizeof(s->name), "%s", pbase.nickname);
        } else {
            snprintf(s->name, sizeof(s->name), "User");
        }
        u32 img_size = 0;
        if (R_SUCCEEDED(accountProfileGetImageSize(&prof, &img_size)) && img_size > 8 && img_size < 256 * 1024) {
            void *buf = malloc(img_size);
            if (buf) {
                u32 actual = 0;
                if (R_SUCCEEDED(accountProfileLoadImage(&prof, buf, img_size, &actual)) && actual > 0) {
                    s->icon = dh_texture_from_mem(up->r, buf, (int)actual, &s->icon_w, &s->icon_h);
                }
                free(buf);
            }
        }
        accountProfileClose(&prof);
        up->user_n++;
    }
    int slots = up->user_n + 1;
    if (up->focus >= slots) {
        up->focus = slots - 1;
    }
    if (up->focus < 0) {
        up->focus = 0;
    }
}

static int slot_count(const DhUserpick *up)
{
    return up->user_n + 1;
}

static void slot_rect(const DhUserpick *up, int i, int focused, int *x, int *y, int *w, int *h)
{
    int n = slot_count(up);
    int total = n * UP_TILE + (n - 1) * UP_GAP;
    int x0 = (DH_SCREEN_W - total) / 2;
    int size = focused ? (UP_TILE + UP_FOCUS) : UP_TILE;
    int cx = x0 + i * (UP_TILE + UP_GAP) + UP_TILE / 2;
    int cy = UP_TILE_Y + UP_TILE / 2;
    *w = size;
    *h = size;
    *x = cx - size / 2;
    *y = cy - size / 2;
}

static int hit_slot(const DhUserpick *up, int tx, int ty)
{
    if (tx < 0 || ty < 0) {
        return -1;
    }
    for (int i = 0; i < slot_count(up); i++) {
        int x, y, w, h;
        slot_rect(up, i, 0, &x, &y, &w, &h);
        /* Nickname row under the tile is also tappable. */
        if (tx >= x && tx < x + w && ty >= y && ty < y + h + 40) {
            return i;
        }
    }
    return -1;
}

static void ellipsize(TTF_Font *font, const char *in, char *out, size_t out_sz, int max_w)
{
    if (!in || !out || out_sz < 4) {
        return;
    }
    if (!font || dh_text_width(font, in) <= max_w) {
        snprintf(out, out_sz, "%s", in);
        return;
    }
    char tmp[96];
    snprintf(tmp, sizeof(tmp), "%s", in);
    size_t n = strlen(tmp);
    while (n > 1) {
        n--;
        tmp[n] = 0;
        char shown[100];
        snprintf(shown, sizeof(shown), "%s...", tmp);
        if (dh_text_width(font, shown) <= max_w) {
            snprintf(out, out_sz, "%s", shown);
            return;
        }
    }
    snprintf(out, out_sz, "...");
}

static void draw_mosaic(DhUserpick *up, SDL_Renderer *r)
{
    int n = dh_titles_ok() ? dh_titles_count() : 0;
    int cw = DH_SCREEN_W / UP_MOSAIC_COLS + 8;
    int ch = DH_SCREEN_H / UP_MOSAIC_ROWS + 8;
    for (int row = 0; row < UP_MOSAIC_ROWS; row++) {
        for (int col = 0; col < UP_MOSAIC_COLS; col++) {
            int x = col * (DH_SCREEN_W / UP_MOSAIC_COLS) - 4;
            int y = row * (DH_SCREEN_H / UP_MOSAIC_ROWS) - 4;
            if (up->privacy_covers) {
                int i = (row * UP_MOSAIC_COLS + col) % 5;
                SDL_Texture *tex = up->privacy_sq[i] ? up->privacy_sq[i] : up->privacy;
                int tw = up->privacy_sq[i] ? up->privacy_sq_w[i] : up->privacy_w;
                int th = up->privacy_sq[i] ? up->privacy_sq_h[i] : up->privacy_h;
                if (tex) {
                    SDL_SetTextureColorMod(tex, 55, 55, 55);
                    dh_cover(r, tex, tw, th, x, y, cw, ch);
                    SDL_SetTextureColorMod(tex, 255, 255, 255);
                } else {
                    dh_fill(r, x, y, cw, ch, (SDL_Color){ 20, 28, 36, 255 });
                }
                continue;
            }
            if (n < 1) {
                continue;
            }
            DhTitle *t = dh_titles_get((row * UP_MOSAIC_COLS + col) % n);
            if (t && t->icon && t->icon_state == DH_ICON_READY) {
                SDL_SetTextureColorMod(t->icon, 55, 55, 55);
                dh_cover(r, t->icon, t->icon_w, t->icon_h, x, y, cw, ch);
                SDL_SetTextureColorMod(t->icon, 255, 255, 255);
            } else if (t) {
                SDL_Color c = { (Uint8)(t->cr / 3), (Uint8)(t->cg / 3), (Uint8)(t->cb / 3), 255 };
                dh_fill(r, x, y, cw, ch, c);
            }
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    dh_fill(r, 0, 0, DH_SCREEN_W, DH_SCREEN_H, (SDL_Color){ 0x0E, 0x14, 0x1B, 214 });
}

static void draw_plus(SDL_Renderer *r, int x, int y, int s, SDL_Color fg)
{
    int cx = x + s / 2;
    int cy = y + s / 2;
    int arm = (s * UP_PLUS_ARM) / UP_TILE;
    int th = (s * UP_PLUS_THICK) / UP_TILE;
    if (th < 6) {
        th = 6;
    }
    dh_fill(r, cx - arm / 2, cy - th / 2, arm, th, fg);
    dh_fill(r, cx - th / 2, cy - arm / 2, th, arm, fg);
}

static void draw_tile_border(SDL_Renderer *r, int x, int y, int s, int bw, SDL_Color c)
{
    dh_fill(r, x, y, s, bw, c);
    dh_fill(r, x, y + s - bw, s, bw, c);
    dh_fill(r, x, y, bw, s, c);
    dh_fill(r, x + s - bw, y, bw, s, c);
}

DhUserpick *dh_userpick_create(SDL_Renderer *r, DhFonts *fonts, const DhTheme *th)
{
    if (!r || !fonts || !th) {
        return NULL;
    }
    DhUserpick *up = (DhUserpick *)SDL_calloc(1, sizeof(DhUserpick));
    if (!up) {
        return NULL;
    }
    up->r = r;
    up->fonts = fonts;
    up->th = th;
    snprintf(up->title, sizeof(up->title), "Who's playing?");
    up->title_font = open_title_font();
    dh_sfx_init();
    {
        static DhSettings sfx_cfg;
        dh_settings_load(&sfx_cfg);
        dh_sfx_bind(&sfx_cfg);
        up->privacy_covers = sfx_cfg.privacy_covers;
        if (up->privacy_covers) {
            SDL_Surface *surf = IMG_Load("romfs:/Icon.png");
            if (surf) {
                up->privacy = SDL_CreateTextureFromSurface(r, surf);
                if (up->privacy) {
                    SDL_SetTextureScaleMode(up->privacy, SDL_ScaleModeLinear);
                    up->privacy_w = surf->w;
                    up->privacy_h = surf->h;
                }
                SDL_FreeSurface(surf);
            }
            for (int i = 0; i < 5; i++) {
                char path[48];
                snprintf(path, sizeof(path), "romfs:/privacy/sq%d.jpg", i + 1);
                surf = IMG_Load(path);
                if (!surf) {
                    continue;
                }
                up->privacy_sq[i] = SDL_CreateTextureFromSurface(r, surf);
                if (up->privacy_sq[i]) {
                    SDL_SetTextureScaleMode(up->privacy_sq[i], SDL_ScaleModeLinear);
                    up->privacy_sq_w[i] = surf->w;
                    up->privacy_sq_h[i] = surf->h;
                }
                SDL_FreeSurface(surf);
            }
        }
    }
    if (!dh_draw_is_init()) {
        dh_draw_init(r);
        up->owned_draw = 1;
    }
    if (!dh_titles_ok()) {
        if (dh_titles_init() == 0) {
            up->owned_titles = 1;
        }
    }
    load_users(up);
    return up;
}

void dh_userpick_detach_titles(DhUserpick *up)
{
    if (up) {
        up->owned_titles = 0;
    }
}

void dh_userpick_destroy(DhUserpick *up)
{
    if (!up) {
        return;
    }
    free_users(up);
    if (up->title_font) {
        TTF_CloseFont(up->title_font);
    }
    if (up->privacy) {
        SDL_DestroyTexture(up->privacy);
        up->privacy = NULL;
    }
    for (int i = 0; i < 5; i++) {
        if (up->privacy_sq[i]) {
            SDL_DestroyTexture(up->privacy_sq[i]);
            up->privacy_sq[i] = NULL;
        }
    }
    if (up->owned_titles) {
        dh_titles_exit();
    }
    if (up->owned_draw) {
        dh_draw_shutdown();
    }
    SDL_free(up);
}

void dh_userpick_set_title(DhUserpick *up, const char *title)
{
    if (!up || !title) {
        return;
    }
    snprintf(up->title, sizeof(up->title), "%s", title);
}

void dh_userpick_set_logo(DhUserpick *up, SDL_Texture *tex)
{
    if (!up) {
        return;
    }
    up->logo = tex;
    up->logo_w = 0;
    up->logo_h = 0;
    if (tex) {
        SDL_QueryTexture(tex, NULL, NULL, &up->logo_w, &up->logo_h);
    }
}

void dh_userpick_reload(DhUserpick *up)
{
    if (!up) {
        return;
    }
    load_users(up);
}

void dh_userpick_tick(DhUserpick *up, SDL_Renderer *r)
{
    if (!up || !r || !dh_titles_ok()) {
        return;
    }
    int n = dh_titles_count();
    if (n < 1) {
        return;
    }
    int want = UP_MOSAIC_COLS * UP_MOSAIC_ROWS;
    if (want > n) {
        want = n;
    }
    dh_titles_pump_icons(r, 0, want - 1, 8);
}

void dh_userpick_draw(DhUserpick *up, SDL_Renderer *r)
{
    if (!up || !r) {
        return;
    }
    dh_fill(r, 0, 0, DH_SCREEN_W, DH_SCREEN_H, up->th->bg);
    draw_mosaic(up, r);

    if (up->logo && up->logo_w > 0 && up->logo_h > 0) {
        int lh = UP_LOGO_H;
        int lw = (up->logo_w * lh) / up->logo_h;
        dh_stretch(r, up->logo, DH_EDGE, 36, lw, lh);
    }

    TTF_Font *title_font = up->title_font ? up->title_font : up->fonts->large;
    if (title_font && up->title[0]) {
        int tw = dh_text_width(title_font, up->title);
        dh_text(r, title_font, up->title, (DH_SCREEN_W - tw) / 2, UP_TITLE_Y, up->th->white);
    }

    TTF_Font *name_font = up->fonts->small ? up->fonts->small : up->fonts->medium;
    int n = slot_count(up);
    for (int i = 0; i < n; i++) {
        int focused = (i == up->focus);
        int x, y, w, h;
        slot_rect(up, i, focused, &x, &y, &w, &h);
        SDL_Color border = focused ? up->th->white : up->th->pill;
        int bw = focused ? UP_BORDER + 1 : 2;
        draw_tile_border(r, x, y, w, bw, border);
        int ix = x + bw;
        int iy = y + bw;
        int isz = w - bw * 2;
        if (i < up->user_n) {
            DhUserSlot *s = &up->users[i];
            if (s->icon) {
                dh_stretch(r, s->icon, ix, iy, isz, isz);
            } else {
                dh_fill(r, ix, iy, isz, isz, up->th->card);
            }
            if (name_font && s->name[0]) {
                char shown[48];
                ellipsize(name_font, s->name, shown, sizeof(shown), UP_TILE);
                int nw = dh_text_width(name_font, shown);
                int nx = x + (w - nw) / 2;
                int ny = UP_TILE_Y + UP_TILE + 14;
                dh_text(r, name_font, shown, nx, ny, focused ? up->th->white : up->th->muted);
            }
        } else {
            dh_fill(r, ix, iy, isz, isz, up->th->card);
            draw_plus(r, ix, iy, isz, focused ? up->th->white : up->th->text);
        }
    }
}

DhUserpickAction dh_userpick_handle_input(DhUserpick *up, u64 keys_down, int touch_x, int touch_y)
{
    if (!up) {
        return DH_USERPICK_NONE;
    }
    int n = slot_count(up);
    if (n < 1) {
        return DH_USERPICK_NONE;
    }
    int hit = hit_slot(up, touch_x, touch_y);
    if (hit >= 0) {
        if (up->focus != hit) {
            dh_sfx_nav();
        }
        up->focus = hit;
        if (up->focus < up->user_n) {
            return DH_USERPICK_SELECT;
        }
        return DH_USERPICK_ADD;
    }
    u64 left = HidNpadButton_Left | HidNpadButton_StickLLeft | HidNpadButton_StickRLeft;
    u64 right = HidNpadButton_Right | HidNpadButton_StickLRight | HidNpadButton_StickRRight;
    int old = up->focus;
    if (keys_down & left) {
        up->focus--;
        if (up->focus < 0) {
            up->focus = n - 1;
        }
    }
    if (keys_down & right) {
        up->focus++;
        if (up->focus >= n) {
            up->focus = 0;
        }
    }
    if (up->focus != old) {
        dh_sfx_nav();
    }
    if (keys_down & HidNpadButton_A) {
        if (up->focus < up->user_n) {
            return DH_USERPICK_SELECT;
        }
        return DH_USERPICK_ADD;
    }
    return DH_USERPICK_NONE;
}

int dh_userpick_selected_uid(const DhUserpick *up, AccountUid *out)
{
    if (!up || !out || up->focus < 0 || up->focus >= up->user_n) {
        return 0;
    }
    *out = up->users[up->focus].uid;
    return 1;
}
