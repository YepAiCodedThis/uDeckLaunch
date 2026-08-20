#include "deck_fonts.h"

#include <stdio.h>
#include <string.h>
#include <SDL_ttf.h>
#include "layout.h"

static DhFonts g_shared;
static int g_refs;

static TTF_Font *open_deck_font(const char *path, int px)
{
    TTF_Font *f = TTF_OpenFont(path, px);
    if (f) {
        return f;
    }
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char sd[288];
    snprintf(sd, sizeof(sd), "sdmc:/switch/DeckHome/fonts/%s", base);
    f = TTF_OpenFont(sd, px);
    if (f) {
        return f;
    }
    snprintf(sd, sizeof(sd), "romfs:/fonts/%s", base);
    f = TTF_OpenFont(sd, px);
    if (f) {
        return f;
    }
    f = TTF_OpenFont("romfs:/fonts/MotivaSans-Medium.ttf", px);
    if (f) {
        return f;
    }
    return TTF_OpenFont("romfs:/fonts/MotivaSans-Bold.ttf", px);
}

int deck_fonts_load(DhFonts *fonts)
{
    if (!fonts) {
        return -1;
    }
    if (g_refs > 0) {
        *fonts = g_shared;
        g_refs++;
        return 0;
    }
    memset(&g_shared, 0, sizeof(g_shared));
    g_shared.small = open_deck_font("romfs:/fonts/MotivaSans-Medium.ttf", 18);
    g_shared.medium = open_deck_font("romfs:/fonts/MotivaSans-Medium.ttf", 22);
    g_shared.bold = open_deck_font("romfs:/fonts/MotivaSans-Bold.ttf", 20);
    g_shared.large = open_deck_font("romfs:/fonts/MotivaSans-ExtraBold.ttf", 32);
    g_shared.sub = open_deck_font("romfs:/fonts/MotivaSans-Medium.ttf", 28);
    g_shared.clock = open_deck_font("romfs:/fonts/MotivaSans-Medium.ttf", DH_CLOCK_PX);
    g_shared.hint = open_deck_font("romfs:/fonts/MotivaSans-ExtraBold.ttf", 24);
    g_shared.sidebar = open_deck_font("romfs:/fonts/MotivaSans-Bold.ttf", 40);
    if (!g_shared.small || !g_shared.medium || !g_shared.large) {
        g_refs = 1;
        deck_fonts_close(&g_shared);
        memset(fonts, 0, sizeof(*fonts));
        return -1;
    }
    *fonts = g_shared;
    g_refs = 1;
    return 0;
}

void deck_fonts_close(DhFonts *fonts)
{
    if (fonts) {
        memset(fonts, 0, sizeof(*fonts));
    }
    if (g_refs > 1) {
        g_refs--;
        return;
    }
    if (g_refs <= 0) {
        return;
    }
    g_refs = 0;
    TTF_Font **slots[] = {
        &g_shared.small, &g_shared.medium, &g_shared.large, &g_shared.bold,
        &g_shared.sub, &g_shared.clock, &g_shared.hint, &g_shared.sidebar
    };
    for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) {
        if (*slots[i]) {
            TTF_CloseFont(*slots[i]);
            *slots[i] = NULL;
        }
    }
}
