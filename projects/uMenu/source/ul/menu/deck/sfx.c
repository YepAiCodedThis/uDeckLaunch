#include "sfx.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include "settings.h"

#define DH_SFX_CH_NAV 0
#define DH_SFX_CH_LAUNCH 1
#define DH_SFX_CH_STARTUP 2
#define DH_SFX_LAUNCH_WAIT_MS 450

static Mix_Chunk *g_nav;
static Mix_Chunk *g_launch;
static Mix_Chunk *g_startup;
static const DhSettings *g_cfg;
static int g_inited;

static Mix_Chunk *load_chunk(const char *romfs, const char *sd)
{
    Mix_Chunk *c = Mix_LoadWAV(romfs);
    if (c) {
        return c;
    }
    return Mix_LoadWAV(sd);
}

static int master_on(void)
{
    return !g_cfg || g_cfg->sfx_master;
}

static int nav_on(void)
{
    return master_on() && (!g_cfg || g_cfg->sfx_nav);
}

static int launch_on(void)
{
    return master_on() && (!g_cfg || g_cfg->sfx_launch);
}

static int startup_on(void)
{
    return master_on() && (!g_cfg || g_cfg->sfx_startup);
}

void dh_sfx_init(void)
{
    if (g_inited) {
        return;
    }
    g_nav = load_chunk("romfs:/sfx/nav.mp3", "sdmc:/switch/uDeckLaunch/sfx/nav.mp3");
    g_launch = load_chunk("romfs:/sfx/launch.mp3", "sdmc:/switch/uDeckLaunch/sfx/launch.mp3");
    g_startup = load_chunk("romfs:/sfx/startup.mp3", "sdmc:/switch/uDeckLaunch/sfx/startup.mp3");
    if (g_nav) {
        Mix_VolumeChunk(g_nav, 96);
    }
    if (g_launch) {
        Mix_VolumeChunk(g_launch, 112);
    }
    if (g_startup) {
        Mix_VolumeChunk(g_startup, 112);
    }
    g_inited = 1;
}

void dh_sfx_exit(void)
{
    if (g_nav) {
        Mix_HaltChannel(DH_SFX_CH_NAV);
        Mix_FreeChunk(g_nav);
        g_nav = NULL;
    }
    if (g_launch) {
        Mix_HaltChannel(DH_SFX_CH_LAUNCH);
        Mix_FreeChunk(g_launch);
        g_launch = NULL;
    }
    if (g_startup) {
        Mix_HaltChannel(DH_SFX_CH_STARTUP);
        Mix_FreeChunk(g_startup);
        g_startup = NULL;
    }
    g_cfg = NULL;
    g_inited = 0;
}

void dh_sfx_bind(const DhSettings *s)
{
    g_cfg = s;
}

void dh_sfx_nav(void)
{
    if (!g_inited || !nav_on() || !g_nav) {
        return;
    }
    Mix_HaltChannel(DH_SFX_CH_NAV);
    Mix_PlayChannel(DH_SFX_CH_NAV, g_nav, 0);
}

void dh_sfx_launch(void)
{
    if (!g_inited || !launch_on() || !g_launch) {
        return;
    }
    Mix_HaltChannel(DH_SFX_CH_LAUNCH);
    Mix_PlayChannel(DH_SFX_CH_LAUNCH, g_launch, 0);
    u32 t0 = SDL_GetTicks();
    while (Mix_Playing(DH_SFX_CH_LAUNCH) && (SDL_GetTicks() - t0) < DH_SFX_LAUNCH_WAIT_MS) {
        SDL_Delay(8);
    }
}

void dh_sfx_startup(void)
{
    if (!g_inited || !startup_on() || !g_startup) {
        return;
    }
    Mix_HaltChannel(DH_SFX_CH_STARTUP);
    Mix_PlayChannel(DH_SFX_CH_STARTUP, g_startup, 0);
}
