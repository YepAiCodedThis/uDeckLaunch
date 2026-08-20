#include "dh_hb.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "draw.h"
#include "settings.h"

#include <SDL_image.h>

#define HB_CACHE_DIR "sdmc:/ulaunch/cache/hb"

static DhHb g_hb[DH_HB_MAX];
static int g_count;

static int ends_with_nro(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && (strcmp(name + n - 4, ".nro") == 0 || strcmp(name + n - 4, ".NRO") == 0);
}

static void color_from_name(const char *name, Uint8 *r, Uint8 *g, Uint8 *b)
{
    static const Uint8 pal[][3] = {
        {27, 40, 56}, {35, 54, 74}, {42, 47, 56},
        {24, 36, 48}, {48, 58, 72}, {22, 32, 44}
    };
    unsigned h = 0;
    for (const char *p = name; *p; p++) {
        h = h * 33u + (unsigned char)*p;
    }
    int i = (int)(h % 6);
    *r = pal[i][0];
    *g = pal[i][1];
    *b = pal[i][2];
}

static void free_icon(DhHb *e)
{
    if (e->icon) {
        SDL_DestroyTexture(e->icon);
        e->icon = NULL;
    }
    e->icon_w = 0;
    e->icon_h = 0;
}

static void ensure_hb_cache_dir(void)
{
    mkdir("sdmc:/ulaunch", 0777);
    mkdir("sdmc:/ulaunch/cache", 0777);
    mkdir(HB_CACHE_DIR, 0777);
}

static unsigned hb_key(const DhHb *e)
{
    unsigned h = (unsigned)e->size;
    for (const char *p = e->path; *p; p++) {
        h = h * 33u + (unsigned char)*p;
    }
    return h;
}

static void hb_cache_paths(const DhHb *e, char *jpg, size_t jpg_n, char *nam, size_t nam_n)
{
    unsigned k = hb_key(e);
    snprintf(jpg, jpg_n, HB_CACHE_DIR "/%08X.jpg", k);
    snprintf(nam, nam_n, HB_CACHE_DIR "/%08X.txt", k);
}

static void save_hb_cache(const DhHb *e, const u8 *jpeg, size_t jpeg_len)
{
    if (!jpeg || jpeg_len < 4) {
        return;
    }
    ensure_hb_cache_dir();
    char jpg[160];
    char nam[160];
    hb_cache_paths(e, jpg, sizeof(jpg), nam, sizeof(nam));
    FILE *f = fopen(jpg, "wb");
    if (f) {
        fwrite(jpeg, 1, jpeg_len, f);
        fclose(f);
    }
    f = fopen(nam, "wb");
    if (f) {
        fprintf(f, "%s\n%s\n%s\n", e->name, e->publisher, e->version);
        fclose(f);
    }
}

static int load_cached_hb(DhHb *e, SDL_Renderer *r)
{
    char jpg[160];
    char nam[160];
    hb_cache_paths(e, jpg, sizeof(jpg), nam, sizeof(nam));
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
    }
    SDL_FreeSurface(surf);
    if (!e->icon) {
        return 0;
    }
    FILE *f = fopen(nam, "rb");
    if (f) {
        char line[256];
        if (fgets(line, (int)sizeof(line), f)) {
            size_t n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
                line[--n] = 0;
            }
            if (line[0]) {
                snprintf(e->name, sizeof(e->name), "%s", line);
            }
        }
        if (fgets(line, (int)sizeof(line), f)) {
            size_t n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
                line[--n] = 0;
            }
            snprintf(e->publisher, sizeof(e->publisher), "%s", line);
        }
        if (fgets(line, (int)sizeof(line), f)) {
            size_t n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
                line[--n] = 0;
            }
            snprintf(e->version, sizeof(e->version), "%s", line);
        }
        fclose(f);
    }
    return 1;
}

static void add_nro(const char *dir, const char *name)
{
    if (g_count >= DH_HB_MAX) {
        return;
    }
    DhHb *e = &g_hb[g_count];
    memset(e, 0, sizeof(*e));
    snprintf(e->path, sizeof(e->path), "%s/%s", dir, name);
    snprintf(e->name, sizeof(e->name), "%s", name);
    size_t n = strlen(e->name);
    if (n > 4) {
        e->name[n - 4] = '\0';
    }
    {
        struct stat st;
        if (stat(e->path, &st) == 0) {
            e->size = (u64)st.st_size;
        }
    }
    e->last_play = dh_recent_hb(e->path);
    color_from_name(e->name, &e->cr, &e->cg, &e->cb);
    g_count++;
}

static void scan_dir(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    if (!d) {
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        char path[DH_HB_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode) && depth < 2) {
            scan_dir(path, depth + 1);
        } else if (S_ISREG(st.st_mode) && ends_with_nro(ent->d_name)) {
            add_nro(dir, ent->d_name);
        }
    }
    closedir(d);
}

static int g_scanned;

int dh_hb_scan(void)
{
    dh_hb_exit();
    scan_dir("sdmc:/switch", 0);
    g_scanned = 1;
    return 0;
}

int dh_hb_ensure(void)
{
    if (g_scanned) {
        return g_count;
    }
    return dh_hb_scan();
}

void dh_hb_exit(void)
{
    for (int i = 0; i < g_count; i++) {
        free_icon(&g_hb[i]);
    }
    memset(g_hb, 0, sizeof(g_hb));
    g_count = 0;
    g_scanned = 0;
}

int dh_hb_count(void)
{
    return g_count;
}

DhHb *dh_hb_get(int index)
{
    if (index < 0 || index >= g_count) {
        return NULL;
    }
    return &g_hb[index];
}

static int load_nro_assets(DhHb *e, SDL_Renderer *r)
{
    FILE *f = fopen(e->path, "rb");
    if (!f) {
        return -1;
    }
    NroStart start;
    NroHeader hdr;
    if (fread(&start, 1, sizeof(start), f) != sizeof(start) ||
        fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
        hdr.magic != NROHEADER_MAGIC) {
        fclose(f);
        return -1;
    }
    if (fseek(f, (long)hdr.size, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    NroAssetHeader aset;
    if (fread(&aset, 1, sizeof(aset), f) != sizeof(aset) ||
        aset.magic != NROASSETHEADER_MAGIC) {
        fclose(f);
        return -1;
    }
    if (aset.nacp.size >= sizeof(NacpStruct)) {
        NacpStruct nacp;
        if (fseek(f, (long)(hdr.size + (u32)aset.nacp.offset), SEEK_SET) == 0 &&
            fread(&nacp, 1, sizeof(nacp), f) == sizeof(nacp)) {
            NacpLanguageEntry *le = NULL;
            nacpGetLanguageEntry(&nacp, &le);
            if (le && le->name[0]) {
                snprintf(e->name, sizeof(e->name), "%s", le->name);
            }
            if (le && le->author[0]) {
                snprintf(e->publisher, sizeof(e->publisher), "%s", le->author);
            }
            if (nacp.display_version[0]) {
                snprintf(e->version, sizeof(e->version), "%s", nacp.display_version);
            }
        }
    }
    if (aset.icon.size >= 32 && aset.icon.size < 512 * 1024) {
        u8 *jpeg = (u8 *)malloc((size_t)aset.icon.size);
        if (jpeg &&
            fseek(f, (long)(hdr.size + (u32)aset.icon.offset), SEEK_SET) == 0 &&
            fread(jpeg, 1, (size_t)aset.icon.size, f) == (size_t)aset.icon.size) {
            e->icon = dh_texture_from_mem(r, jpeg, (int)aset.icon.size, &e->icon_w, &e->icon_h);
            if (e->icon) {
                save_hb_cache(e, jpeg, (size_t)aset.icon.size);
            }
        }
        free(jpeg);
    }
    fclose(f);
    return e->icon ? 0 : -1;
}

void dh_hb_pump_icons(SDL_Renderer *r, int vis_lo, int vis_hi, int max_n)
{
    if (!r || g_count <= 0) {
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
        DhHb *e = &g_hb[i];
        if (e->icon_state != DH_HB_ICON_NONE) {
            continue;
        }
        if (load_cached_hb(e, r)) {
            e->icon_state = DH_HB_ICON_READY;
            loaded++;
        }
    }
    for (int i = vis_lo; i <= vis_hi; i++) {
        DhHb *e = &g_hb[i];
        if (e->icon_state != DH_HB_ICON_NONE) {
            continue;
        }
        if (load_nro_assets(e, r) == 0) {
            e->icon_state = DH_HB_ICON_READY;
        } else {
            e->icon_state = DH_HB_ICON_FAIL;
        }
        break;
    }
}

static int hb_name_cmp(const char *a, const char *b)
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

static int find_path(const char *path)
{
    for (int i = 0; i < g_count; i++) {
        if (strcmp(g_hb[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

void dh_hb_swap(int a, int b)
{
    if (a < 0 || b < 0 || a >= g_count || b >= g_count || a == b) {
        return;
    }
    DhHb tmp = g_hb[a];
    g_hb[a] = g_hb[b];
    g_hb[b] = tmp;
}

void dh_hb_save_custom(void)
{
    char paths[DH_HB_MAX][DH_HB_PATH_MAX];
    for (int i = 0; i < g_count; i++) {
        snprintf(paths[i], DH_HB_PATH_MAX, "%s", g_hb[i].path);
    }
    dh_order_save_hb(paths, g_count);
}

void dh_hb_touch(const char *path)
{
    dh_recent_touch_hb(path);
    int i = find_path(path);
    if (i >= 0) {
        g_hb[i].last_play = dh_recent_hb(path);
    }
}

void dh_hb_apply_sort(int sort)
{
    if (g_count < 2) {
        return;
    }
    if (sort == DH_SORT_CUSTOM) {
        char paths[DH_HB_MAX][DH_HB_PATH_MAX];
        int n = dh_order_load_hb(paths, DH_HB_MAX);
        if (n < 1) {
            return;
        }
        DhHb *tmp = (DhHb *)malloc(sizeof(DhHb) * (size_t)g_count);
        if (!tmp) {
            return;
        }
        u8 used[DH_HB_MAX];
        memset(used, 0, sizeof(used));
        int out = 0;
        for (int i = 0; i < n && out < g_count; i++) {
            int j = find_path(paths[i]);
            if (j < 0 || used[j]) {
                continue;
            }
            tmp[out++] = g_hb[j];
            used[j] = 1;
        }
        for (int j = 0; j < g_count; j++) {
            if (!used[j]) {
                tmp[out++] = g_hb[j];
            }
        }
        memcpy(g_hb, tmp, sizeof(DhHb) * (size_t)out);
        free(tmp);
        g_count = out;
        return;
    }
    for (int i = 1; i < g_count; i++) {
        DhHb key = g_hb[i];
        int j = i;
        while (j > 0) {
            int less = 0;
            if (sort == DH_SORT_RECENT) {
                if (g_hb[j - 1].last_play < key.last_play) {
                    less = 1;
                } else if (g_hb[j - 1].last_play == key.last_play &&
                           hb_name_cmp(g_hb[j - 1].name, key.name) > 0) {
                    less = 1;
                }
            } else if (sort == DH_SORT_ZA) {
                less = hb_name_cmp(g_hb[j - 1].name, key.name) < 0;
            } else {
                less = hb_name_cmp(g_hb[j - 1].name, key.name) > 0;
            }
            if (!less) {
                break;
            }
            g_hb[j] = g_hb[j - 1];
            j--;
        }
        g_hb[j] = key;
    }
}

int dh_hb_uninstall(int index)
{
    DhHb *e = dh_hb_get(index);
    if (!e) {
        return -1;
    }
    if (remove(e->path) != 0) {
        return -1;
    }
    free_icon(e);
    if (index < g_count - 1) {
        memmove(&g_hb[index], &g_hb[index + 1], sizeof(DhHb) * (size_t)(g_count - index - 1));
    }
    g_count--;
    memset(&g_hb[g_count], 0, sizeof(DhHb));
    return 0;
}
