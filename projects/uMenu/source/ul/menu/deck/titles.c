#include "titles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL_image.h>

#include "settings.h"

#define COVER_CACHE_DIR "sdmc:/ulaunch/cache/covers"

static DhTitle g_titles[DH_TITLES_MAX];
static int g_count;
static int g_ns_ok;
static int g_pdm_ok;
static NsApplicationControlData *g_ctrl;

static int is_system_title(u64 id)
{
    return (id & 0xFFFFFFFFFFFF0000ULL) == 0x0100000000000000ULL;
}

static void color_from_id(u64 id, Uint8 *r, Uint8 *g, Uint8 *b)
{
    static const Uint8 pal[][3] = {
        {26, 159, 255}, {89, 191, 64}, {232, 89, 79},
        {196, 160, 64}, {104, 159, 56}, {42, 157, 143}
    };
    int i = (int)(id % 6);
    *r = pal[i][0];
    *g = pal[i][1];
    *b = pal[i][2];
}

static void free_icon(DhTitle *e)
{
    if (e->icon) {
        SDL_DestroyTexture(e->icon);
        e->icon = NULL;
    }
    e->icon_w = 0;
    e->icon_h = 0;
    if (e->icon_state == DH_ICON_READY) {
        e->icon_state = DH_ICON_NONE;
    }
}

static void clear_all(void)
{
    for (int i = 0; i < g_count; i++) {
        free_icon(&g_titles[i]);
    }
    memset(g_titles, 0, sizeof(g_titles));
    g_count = 0;
}

static void ensure_cover_dir(void)
{
    mkdir("sdmc:/ulaunch", 0777);
    mkdir("sdmc:/ulaunch/cache", 0777);
    mkdir(COVER_CACHE_DIR, 0777);
}

static void cover_paths(u64 id, char *jpg, size_t jpg_n, char *nam, size_t nam_n)
{
    snprintf(jpg, jpg_n, COVER_CACHE_DIR "/%016llX.jpg", (unsigned long long)id);
    snprintf(nam, nam_n, COVER_CACHE_DIR "/%016llX.txt", (unsigned long long)id);
}

static void trim_nl(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = 0;
    }
}

static int load_cached_name(DhTitle *e)
{
    char nam[160];
    snprintf(nam, sizeof(nam), COVER_CACHE_DIR "/%016llX.txt", (unsigned long long)e->application_id);
    FILE *f = fopen(nam, "rb");
    if (!f) {
        return 0;
    }
    if (fgets(e->name, (int)sizeof(e->name), f)) {
        trim_nl(e->name);
        if (e->name[0]) {
            e->meta_loaded = 1;
        }
    }
    fclose(f);
    return e->meta_loaded;
}

static int load_cached_cover(DhTitle *e, SDL_Renderer *r)
{
    char jpg[160];
    snprintf(jpg, sizeof(jpg), COVER_CACHE_DIR "/%016llX.jpg", (unsigned long long)e->application_id);
    SDL_Surface *surf = IMG_Load(jpg);
    if (!surf) {
        return 0;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
        e->icon = tex;
        e->icon_w = surf->w;
        e->icon_h = surf->h;
        e->icon_state = DH_ICON_READY;
    }
    SDL_FreeSurface(surf);
    if (!e->icon) {
        return 0;
    }
    if (!e->meta_loaded) {
        load_cached_name(e);
    }
    return 1;
}

static void save_cached_cover(const DhTitle *e, const u8 *jpeg, size_t jpeg_len)
{
    if (!jpeg || jpeg_len < 4) {
        return;
    }
    ensure_cover_dir();
    char jpg[160];
    char nam[160];
    cover_paths(e->application_id, jpg, sizeof(jpg), nam, sizeof(nam));
    FILE *f = fopen(jpg, "wb");
    if (f) {
        fwrite(jpeg, 1, jpeg_len, f);
        fclose(f);
    }
    if (e->name[0]) {
        f = fopen(nam, "wb");
        if (f) {
            fprintf(f, "%s\n", e->name);
            fclose(f);
        }
    }
}

static SDL_Texture *jpeg_to_texture(SDL_Renderer *renderer, const u8 *data, size_t len, int *w, int *h)
{
    if (!data || len < 4) {
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromConstMem(data, (int)len);
    if (!rw) {
        return NULL;
    }
    SDL_Surface *surf = IMG_Load_RW(rw, 1);
    if (!surf) {
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    }
    *w = surf->w;
    *h = surf->h;
    SDL_FreeSurface(surf);
    return tex;
}

static void apply_meta(DhTitle *e, NsApplicationControlData *ctrl)
{
    NacpLanguageEntry *le = NULL;
    nacpGetLanguageEntry(&ctrl->nacp, &le);
    if (le && le->name[0]) {
        snprintf(e->name, sizeof(e->name), "%s", le->name);
        snprintf(e->publisher, sizeof(e->publisher), "%s", le->author);
    }
    snprintf(e->version, sizeof(e->version), "%s", ctrl->nacp.display_version);
    e->meta_loaded = 1;
}

static int name_cmp(const char *a, const char *b)
{
    if (!a) {
        a = "";
    }
    if (!b) {
        b = "";
    }
    for (;;) {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') {
            ca += 32;
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb += 32;
        }
        if (ca != cb) {
            return ca - cb;
        }
        if (!ca) {
            return 0;
        }
        a++;
        b++;
    }
}

static void fill_last_play(void)
{
    for (int i = 0; i < g_count; i++) {
        g_titles[i].last_play = dh_recent_game(g_titles[i].application_id);
    }
    if (!g_pdm_ok || g_count < 1) {
        return;
    }
    u64 ids[DH_TITLES_MAX];
    PdmLastPlayTime times[DH_TITLES_MAX];
    for (int i = 0; i < g_count; i++) {
        ids[i] = g_titles[i].application_id;
    }
    s32 got = 0;
    if (R_FAILED(pdmqryQueryLastPlayTime(false, times, ids, g_count, &got))) {
        return;
    }
    for (s32 i = 0; i < got; i++) {
        for (int j = 0; j < g_count; j++) {
            if (g_titles[j].application_id != times[i].application_id) {
                continue;
            }
            u64 ts = times[i].timestamp_user;
            if (!ts && times[i].flag) {
                ts = (u64)times[i].last_played_minutes;
            }
            if (ts > g_titles[j].last_play) {
                g_titles[j].last_play = ts;
            }
            break;
        }
    }
}

static int find_id(u64 id)
{
    for (int i = 0; i < g_count; i++) {
        if (g_titles[i].application_id == id) {
            return i;
        }
    }
    return -1;
}

int dh_titles_init(void)
{
    if (g_ns_ok && g_ctrl) {
        return 0;
    }
    memset(g_titles, 0, sizeof(g_titles));
    g_count = 0;
    g_ns_ok = 0;
    g_pdm_ok = 0;
    g_ctrl = (NsApplicationControlData *)malloc(sizeof(NsApplicationControlData));
    if (!g_ctrl) {
        return -1;
    }
#ifdef DH_ULAUNCH
    /* uMenu already owns ns. */
    g_ns_ok = 1;
#else
    if (R_FAILED(nsInitialize())) {
        free(g_ctrl);
        g_ctrl = NULL;
        return -1;
    }
    g_ns_ok = 1;
#endif
    if (R_SUCCEEDED(pdmqryInitialize())) {
        g_pdm_ok = 1;
    }
    dh_titles_refresh();
    return 0;
}

void dh_titles_exit(void)
{
    clear_all();
    if (g_pdm_ok) {
        pdmqryExit();
        g_pdm_ok = 0;
    }
#ifndef DH_ULAUNCH
    if (g_ns_ok) {
        nsExit();
        g_ns_ok = 0;
    }
#else
    g_ns_ok = 0;
#endif
    free(g_ctrl);
    g_ctrl = NULL;
}

int dh_titles_ok(void)
{
    return g_ns_ok;
}

int dh_titles_refresh(void)
{
    if (!g_ns_ok) {
        return -1;
    }
    DhTitle *keep = NULL;
    int keep_n = g_count;
    if (keep_n > 0) {
        keep = (DhTitle *)malloc(sizeof(DhTitle) * (size_t)keep_n);
        if (keep) {
            memcpy(keep, g_titles, sizeof(DhTitle) * (size_t)keep_n);
        } else {
            keep_n = 0;
            clear_all();
        }
    }
    g_count = 0;
    memset(g_titles, 0, sizeof(g_titles));
    s32 offset = 0;
    while (g_count < DH_TITLES_MAX) {
        NsApplicationRecord rec[32];
        s32 got = 0;
        Result rc = nsListApplicationRecord(rec, 32, offset, &got);
        if (R_FAILED(rc) || got <= 0) {
            break;
        }
        for (s32 i = 0; i < got && g_count < DH_TITLES_MAX; i++) {
            if (is_system_title(rec[i].application_id)) {
                continue;
            }
            DhTitle *e = &g_titles[g_count];
            memset(e, 0, sizeof(*e));
            e->application_id = rec[i].application_id;
            snprintf(e->id_hex, sizeof(e->id_hex), "%016llX", (unsigned long long)rec[i].application_id);
            memcpy(e->name, e->id_hex, sizeof(e->id_hex));
            color_from_id(e->application_id, &e->cr, &e->cg, &e->cb);
            g_count++;
        }
        offset += got;
        if (got < 32) {
            break;
        }
    }
    for (int i = 0; i < g_count; i++) {
        DhTitle *e = &g_titles[i];
        for (int j = 0; keep && j < keep_n; j++) {
            if (keep[j].application_id != e->application_id) {
                continue;
            }
            e->icon = keep[j].icon;
            e->icon_w = keep[j].icon_w;
            e->icon_h = keep[j].icon_h;
            e->icon_state = keep[j].icon_state;
            if (keep[j].meta_loaded) {
                memcpy(e->name, keep[j].name, sizeof(e->name));
                memcpy(e->publisher, keep[j].publisher, sizeof(e->publisher));
                memcpy(e->version, keep[j].version, sizeof(e->version));
                e->meta_loaded = 1;
            }
            keep[j].icon = NULL;
            keep[j].icon_state = DH_ICON_NONE;
            break;
        }
        if (!e->meta_loaded) {
            load_cached_name(e);
        }
    }
    for (int j = 0; keep && j < keep_n; j++) {
        free_icon(&keep[j]);
    }
    free(keep);
    fill_last_play();
    return 0;
}

int dh_titles_count(void)
{
    return g_count;
}

DhTitle *dh_titles_get(int index)
{
    if (index < 0 || index >= g_count) {
        return NULL;
    }
    return &g_titles[index];
}

void dh_titles_pump_icons(SDL_Renderer *r, int vis_lo, int vis_hi, int max_n)
{
    if (!g_ns_ok || !g_ctrl || !r) {
        return;
    }
    if (max_n < 1) {
        max_n = 1;
    }
    if (vis_lo < 0) {
        vis_lo = 0;
    }
    if (vis_hi >= g_count) {
        vis_hi = g_count - 1;
    }
    int loaded = 0;
    for (int i = vis_lo; i <= vis_hi && loaded < max_n; i++) {
        DhTitle *e = &g_titles[i];
        if (e->icon_state != DH_ICON_NONE) {
            continue;
        }
        if (load_cached_cover(e, r)) {
            loaded++;
        }
    }
    for (int i = vis_lo; i <= vis_hi; i++) {
        DhTitle *e = &g_titles[i];
        if (e->icon_state != DH_ICON_NONE) {
            continue;
        }
        u64 actual = 0;
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, e->application_id, g_ctrl, sizeof(*g_ctrl), &actual);
        if (R_FAILED(rc)) {
            e->icon_state = DH_ICON_FAIL;
            break;
        }
        apply_meta(e, g_ctrl);
        size_t jpeg_len = 0;
        if (actual > sizeof(NacpStruct)) {
            jpeg_len = (size_t)(actual - sizeof(NacpStruct));
        }
        if (jpeg_len > sizeof(g_ctrl->icon)) {
            jpeg_len = sizeof(g_ctrl->icon);
        }
        SDL_Texture *tex = jpeg_to_texture(r, g_ctrl->icon, jpeg_len, &e->icon_w, &e->icon_h);
        if (tex) {
            e->icon = tex;
            e->icon_state = DH_ICON_READY;
            save_cached_cover(e, g_ctrl->icon, jpeg_len);
        } else {
            e->icon_state = DH_ICON_FAIL;
        }
        memset(g_ctrl, 0, sizeof(*g_ctrl));
        break;
    }
}

void dh_titles_swap(int a, int b)
{
    if (a < 0 || b < 0 || a >= g_count || b >= g_count || a == b) {
        return;
    }
    DhTitle tmp = g_titles[a];
    g_titles[a] = g_titles[b];
    g_titles[b] = tmp;
}

void dh_titles_save_custom(void)
{
    u64 ids[DH_TITLES_MAX];
    for (int i = 0; i < g_count; i++) {
        ids[i] = g_titles[i].application_id;
    }
    dh_order_save_games(ids, g_count);
}

void dh_titles_touch(u64 app_id)
{
    dh_recent_touch_game(app_id);
    int i = find_id(app_id);
    if (i >= 0) {
        g_titles[i].last_play = dh_recent_game(app_id);
    }
}

static void apply_custom(void)
{
    u64 ids[DH_TITLES_MAX];
    int n = dh_order_load_games(ids, DH_TITLES_MAX);
    if (n < 1) {
        return;
    }
    DhTitle *tmp = (DhTitle *)malloc(sizeof(DhTitle) * (size_t)g_count);
    if (!tmp) {
        return;
    }
    u8 used[DH_TITLES_MAX];
    memset(used, 0, sizeof(used));
    int out = 0;
    for (int i = 0; i < n && out < g_count; i++) {
        int j = find_id(ids[i]);
        if (j < 0 || used[j]) {
            continue;
        }
        tmp[out++] = g_titles[j];
        used[j] = 1;
    }
    for (int j = 0; j < g_count; j++) {
        if (!used[j]) {
            tmp[out++] = g_titles[j];
        }
    }
    memcpy(g_titles, tmp, sizeof(DhTitle) * (size_t)out);
    free(tmp);
    g_count = out;
}

void dh_titles_apply_sort(int sort)
{
    if (g_count < 2) {
        return;
    }
    if (sort == DH_SORT_CUSTOM) {
        apply_custom();
        return;
    }
    for (int i = 1; i < g_count; i++) {
        DhTitle key = g_titles[i];
        int j = i;
        while (j > 0) {
            int less = 0;
            if (sort == DH_SORT_RECENT) {
                if (g_titles[j - 1].last_play < key.last_play) {
                    less = 1;
                } else if (g_titles[j - 1].last_play == key.last_play &&
                           name_cmp(g_titles[j - 1].name, key.name) > 0) {
                    less = 1;
                }
            } else if (sort == DH_SORT_ZA) {
                less = name_cmp(g_titles[j - 1].name, key.name) < 0;
            } else {
                less = name_cmp(g_titles[j - 1].name, key.name) > 0;
            }
            if (!less) {
                break;
            }
            g_titles[j] = g_titles[j - 1];
            j--;
        }
        g_titles[j] = key;
    }
}

static int load_ctrl(u64 app_id)
{
    if (!g_ns_ok || !g_ctrl) {
        return 0;
    }
    u64 actual = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, app_id, g_ctrl, sizeof(*g_ctrl), &actual);
    return R_SUCCEEDED(rc) && actual >= sizeof(NacpStruct);
}

int dh_titles_fill_info(u64 app_id, DhTitleInfo *out)
{
    if (!out) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->id_hex, sizeof(out->id_hex), "%016llX", (unsigned long long)app_id);
    snprintf(out->update, sizeof(out->update), "None");
    int idx = find_id(app_id);
    if (idx >= 0 && g_titles[idx].meta_loaded) {
        snprintf(out->name, sizeof(out->name), "%s", g_titles[idx].name);
        snprintf(out->publisher, sizeof(out->publisher), "%s", g_titles[idx].publisher);
        snprintf(out->version, sizeof(out->version), "%s", g_titles[idx].version);
    }
    if (load_ctrl(app_id)) {
        NacpLanguageEntry *le = NULL;
        nacpGetLanguageEntry(&g_ctrl->nacp, &le);
        if (le) {
            if (le->name[0]) {
                snprintf(out->name, sizeof(out->name), "%s", le->name);
            }
            if (le->author[0]) {
                snprintf(out->publisher, sizeof(out->publisher), "%s", le->author);
            }
        }
        if (g_ctrl->nacp.display_version[0]) {
            snprintf(out->version, sizeof(out->version), "%s", g_ctrl->nacp.display_version);
        }
    }
    if (!out->name[0]) {
        snprintf(out->name, sizeof(out->name), "%s", out->id_hex);
    }
    if (!g_ns_ok) {
        return 0;
    }
    NsApplicationContentMetaStatus list[64];
    s32 got = 0;
    if (R_SUCCEEDED(nsListApplicationContentMetaStatus(app_id, 0, list, 64, &got))) {
        for (s32 i = 0; i < got; i++) {
            if (list[i].meta_type == NcmContentMetaType_Patch) {
                snprintf(out->update, sizeof(out->update), "v%u", list[i].version);
            } else if (list[i].meta_type == NcmContentMetaType_AddOnContent && out->dlc_n < DH_INFO_DLC_MAX) {
                char *dst = out->dlc[out->dlc_n];
                snprintf(dst, sizeof(out->dlc[0]), "%016llX", (unsigned long long)list[i].application_id);
                if (load_ctrl(list[i].application_id)) {
                    NacpLanguageEntry *le = NULL;
                    nacpGetLanguageEntry(&g_ctrl->nacp, &le);
                    if (le && le->name[0]) {
                        snprintf(dst, sizeof(out->dlc[0]), "%s", le->name);
                    }
                }
                out->dlc_n++;
            }
        }
    }
    return 0;
}
