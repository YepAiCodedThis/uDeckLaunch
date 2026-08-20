#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define DH_CFG_PATH "sdmc:/switch/DeckHome/settings.cfg"
#define DH_RECENT_GAMES "sdmc:/switch/DeckHome/recent.games"
#define DH_RECENT_HB "sdmc:/switch/DeckHome/recent.hb"
#define DH_ORDER_GAMES "sdmc:/switch/DeckHome/order.games"
#define DH_ORDER_HB "sdmc:/switch/DeckHome/order.hb"

#define DH_RECENT_MAX 256

static u64 g_rg_id[DH_RECENT_MAX];
static u64 g_rg_ts[DH_RECENT_MAX];
static int g_rg_n;

static char g_rh_path[DH_RECENT_MAX][768];
static u64 g_rh_ts[DH_RECENT_MAX];
static int g_rh_n;

static void style_defaults(DhGridStyle *g)
{
    g->format = DH_FMT_CAPSULE;
    g->scale = DH_SCALE_COVER;
    g->size = DH_SIZE_MEDIUM;
}

static int parse_format(const char *v)
{
    return strstr(v, "square") ? DH_FMT_SQUARE : DH_FMT_CAPSULE;
}

static int parse_size(const char *v)
{
    if (strstr(v, "large")) {
        return DH_SIZE_LARGE;
    }
    if (strstr(v, "small")) {
        return DH_SIZE_SMALL;
    }
    return DH_SIZE_MEDIUM;
}

static int parse_scale(const char *v)
{
    if (strstr(v, "contain")) {
        return DH_SCALE_CONTAIN;
    }
    if (strstr(v, "stretch")) {
        return DH_SCALE_STRETCH;
    }
    return DH_SCALE_COVER;
}

static const char *fmt_name(int v)
{
    return v == DH_FMT_SQUARE ? "square" : "capsule";
}

static const char *size_name(int v)
{
    if (v == DH_SIZE_LARGE) {
        return "large";
    }
    if (v == DH_SIZE_SMALL) {
        return "small";
    }
    return "medium";
}

static const char *scale_name(int v)
{
    if (v == DH_SCALE_CONTAIN) {
        return "contain";
    }
    if (v == DH_SCALE_STRETCH) {
        return "stretch";
    }
    return "cover";
}

static void apply_key(DhGridStyle *g, const char *key, const char *val)
{
    if (strcmp(key, "format") == 0) {
        g->format = parse_format(val);
    } else if (strcmp(key, "size") == 0) {
        g->size = parse_size(val);
    } else if (strcmp(key, "scale") == 0) {
        g->scale = parse_scale(val);
    }
}

static void trim_nl(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = 0;
    }
}

static void load_recent_games(void)
{
    g_rg_n = 0;
    FILE *f = fopen(DH_RECENT_GAMES, "r");
    if (!f) {
        return;
    }
    char line[80];
    while (g_rg_n < DH_RECENT_MAX && fgets(line, sizeof(line), f)) {
        unsigned long long id = 0, ts = 0;
        if (sscanf(line, "%llx %llu", &id, &ts) == 2 && id) {
            g_rg_id[g_rg_n] = (u64)id;
            g_rg_ts[g_rg_n] = (u64)ts;
            g_rg_n++;
        }
    }
    fclose(f);
}

static void save_recent_games(void)
{
    mkdir("sdmc:/switch", 0755);
    mkdir("sdmc:/switch/DeckHome", 0755);
    FILE *f = fopen(DH_RECENT_GAMES, "w");
    if (!f) {
        return;
    }
    for (int i = 0; i < g_rg_n; i++) {
        fprintf(f, "%016llx %llu\n", (unsigned long long)g_rg_id[i], (unsigned long long)g_rg_ts[i]);
    }
    fclose(f);
}

static void load_recent_hb(void)
{
    g_rh_n = 0;
    FILE *f = fopen(DH_RECENT_HB, "r");
    if (!f) {
        return;
    }
    char line[800];
    while (g_rh_n < DH_RECENT_MAX && fgets(line, sizeof(line), f)) {
        trim_nl(line);
        unsigned long long ts = 0;
        char *sp = strchr(line, ' ');
        if (!sp) {
            continue;
        }
        *sp = 0;
        if (sscanf(line, "%llu", &ts) != 1) {
            continue;
        }
        snprintf(g_rh_path[g_rh_n], sizeof(g_rh_path[g_rh_n]), "%s", sp + 1);
        g_rh_ts[g_rh_n] = (u64)ts;
        g_rh_n++;
    }
    fclose(f);
}

static void save_recent_hb(void)
{
    mkdir("sdmc:/switch", 0755);
    mkdir("sdmc:/switch/DeckHome", 0755);
    FILE *f = fopen(DH_RECENT_HB, "w");
    if (!f) {
        return;
    }
    for (int i = 0; i < g_rh_n; i++) {
        fprintf(f, "%llu %s\n", (unsigned long long)g_rh_ts[i], g_rh_path[i]);
    }
    fclose(f);
}

void dh_settings_load(DhSettings *s)
{
    style_defaults(&s->games);
    style_defaults(&s->hb);
    s->border = DH_BORDER_MEDIUM;
    s->edge = DH_EDGE_WRAP;
    s->stick_diag = DH_STICK_DIAGONAL;
    s->sort = DH_SORT_RECENT;
    s->sgdb = 0;
    s->sgdb_key_ok = 0;
    s->cover_src = DH_COVER_UDECK;
    s->sgdb_key[0] = 0;
    s->sfx_master = 1;
    s->sfx_nav = 1;
    s->sfx_launch = 1;
    s->sfx_startup = 1;
    s->privacy_covers = 0;
    snprintf(s->lang, sizeof(s->lang), "system");
    load_recent_games();
    load_recent_hb();
    FILE *f = fopen(DH_CFG_PATH, "r");
    if (!f) {
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        *eq = 0;
        char *val = eq + 1;
        if (strncmp(line, "games.", 6) == 0) {
            apply_key(&s->games, line + 6, val);
        } else if (strncmp(line, "hb.", 3) == 0) {
            apply_key(&s->hb, line + 3, val);
        } else if (strcmp(line, "border") == 0) {
            if (strstr(val, "thin")) {
                s->border = DH_BORDER_THIN;
            } else if (strstr(val, "thick")) {
                s->border = DH_BORDER_THICK;
            } else {
                s->border = DH_BORDER_MEDIUM;
            }
        } else if (strcmp(line, "edge") == 0) {
            if (strstr(val, "category") || strstr(val, "tab")) {
                s->edge = DH_EDGE_CATEGORY;
            } else if (strstr(val, "row")) {
                s->edge = DH_EDGE_ROW;
            } else {
                s->edge = DH_EDGE_WRAP;
            }
        } else if (strcmp(line, "stick_diag") == 0) {
            s->stick_diag = strstr(val, "off") || strstr(val, "0") ? DH_STICK_CARDINAL : DH_STICK_DIAGONAL;
        } else if (strcmp(line, "sgdb") == 0) {
            s->sgdb = strstr(val, "on") || strstr(val, "1") ? 1 : 0;
        } else if (strcmp(line, "sgdb_key") == 0) {
            trim_nl(val);
            snprintf(s->sgdb_key, sizeof(s->sgdb_key), "%s", val);
        } else if (strcmp(line, "sgdb_key_ok") == 0) {
            s->sgdb_key_ok = strstr(val, "on") || strstr(val, "1") ? 1 : 0;
        } else if (strcmp(line, "cover_src") == 0) {
            s->cover_src = strstr(val, "udeck") ? DH_COVER_UDECK : DH_COVER_SGDB;
        } else if (strcmp(line, "sfx_master") == 0) {
            s->sfx_master = strstr(val, "off") || strstr(val, "0") ? 0 : 1;
        } else if (strcmp(line, "sfx_nav") == 0) {
            s->sfx_nav = strstr(val, "off") || strstr(val, "0") ? 0 : 1;
        } else if (strcmp(line, "sfx_launch") == 0) {
            s->sfx_launch = strstr(val, "off") || strstr(val, "0") ? 0 : 1;
        } else if (strcmp(line, "sfx_startup") == 0) {
            s->sfx_startup = strstr(val, "off") || strstr(val, "0") ? 0 : 1;
        } else if (strcmp(line, "privacy_covers") == 0) {
            s->privacy_covers = strstr(val, "on") || strstr(val, "1") ? 1 : 0;
        } else if (strcmp(line, "lang") == 0) {
            trim_nl(val);
            if (!val[0]) {
                snprintf(s->lang, sizeof(s->lang), "system");
            } else {
                snprintf(s->lang, sizeof(s->lang), "%s", val);
            }
        } else if (strcmp(line, "sort") == 0) {
            if (strstr(val, "custom")) {
                s->sort = DH_SORT_CUSTOM;
            } else if (strstr(val, "za") || strstr(val, "zyx")) {
                s->sort = DH_SORT_ZA;
            } else if (strstr(val, "az") || strstr(val, "abc") || strstr(val, "name")) {
                s->sort = DH_SORT_AZ;
            } else {
                s->sort = DH_SORT_RECENT;
            }
        } else {
            apply_key(&s->games, line, val);
            apply_key(&s->hb, line, val);
        }
    }
    fclose(f);
}

static void write_style(FILE *f, const char *prefix, const DhGridStyle *g)
{
    fprintf(f, "%s.format=%s\n", prefix, fmt_name(g->format));
    fprintf(f, "%s.size=%s\n", prefix, size_name(g->size));
    fprintf(f, "%s.scale=%s\n", prefix, scale_name(g->scale));
}

void dh_settings_save(const DhSettings *s)
{
    mkdir("sdmc:/switch", 0755);
    mkdir("sdmc:/switch/DeckHome", 0755);
    FILE *f = fopen(DH_CFG_PATH, "w");
    if (!f) {
        return;
    }
    write_style(f, "games", &s->games);
    write_style(f, "hb", &s->hb);
    fprintf(f, "border=%s\n",
        s->border == DH_BORDER_THIN ? "thin" : s->border == DH_BORDER_THICK ? "thick" : "medium");
    fprintf(f, "edge=%s\n", s->edge == DH_EDGE_CATEGORY ? "category" : s->edge == DH_EDGE_ROW ? "row" : "wrap");
    fprintf(f, "stick_diag=%s\n", s->stick_diag ? "on" : "off");
    fprintf(f, "sgdb=%s\n", s->sgdb ? "on" : "off");
    fprintf(f, "sgdb_key_ok=%s\n", s->sgdb_key_ok ? "on" : "off");
    fprintf(f, "cover_src=%s\n", s->cover_src == DH_COVER_UDECK ? "udeck" : "steamgrid");
    fprintf(f, "sgdb_key=%s\n", s->sgdb_key);
    fprintf(f, "sfx_master=%s\n", s->sfx_master ? "on" : "off");
    fprintf(f, "sfx_nav=%s\n", s->sfx_nav ? "on" : "off");
    fprintf(f, "sfx_launch=%s\n", s->sfx_launch ? "on" : "off");
    fprintf(f, "sfx_startup=%s\n", s->sfx_startup ? "on" : "off");
    fprintf(f, "privacy_covers=%s\n", s->privacy_covers ? "on" : "off");
    fprintf(f, "lang=%s\n", s->lang[0] ? s->lang : "system");
    {
        const char *sort = "recent";
        if (s->sort == DH_SORT_AZ) {
            sort = "az";
        } else if (s->sort == DH_SORT_ZA) {
            sort = "za";
        } else if (s->sort == DH_SORT_CUSTOM) {
            sort = "custom";
        }
        fprintf(f, "sort=%s\n", sort);
    }
    fclose(f);
}

u64 dh_recent_game(u64 app_id)
{
    for (int i = 0; i < g_rg_n; i++) {
        if (g_rg_id[i] == app_id) {
            return g_rg_ts[i];
        }
    }
    return 0;
}

void dh_recent_touch_game(u64 app_id)
{
    u64 now = (u64)time(NULL);
    for (int i = 0; i < g_rg_n; i++) {
        if (g_rg_id[i] == app_id) {
            g_rg_ts[i] = now;
            save_recent_games();
            return;
        }
    }
    if (g_rg_n < DH_RECENT_MAX) {
        g_rg_id[g_rg_n] = app_id;
        g_rg_ts[g_rg_n] = now;
        g_rg_n++;
    } else {
        g_rg_id[DH_RECENT_MAX - 1] = app_id;
        g_rg_ts[DH_RECENT_MAX - 1] = now;
    }
    save_recent_games();
}

u64 dh_recent_hb(const char *path)
{
    if (!path) {
        return 0;
    }
    for (int i = 0; i < g_rh_n; i++) {
        if (strcmp(g_rh_path[i], path) == 0) {
            return g_rh_ts[i];
        }
    }
    return 0;
}

void dh_recent_touch_hb(const char *path)
{
    if (!path || !path[0]) {
        return;
    }
    u64 now = (u64)time(NULL);
    for (int i = 0; i < g_rh_n; i++) {
        if (strcmp(g_rh_path[i], path) == 0) {
            g_rh_ts[i] = now;
            save_recent_hb();
            return;
        }
    }
    if (g_rh_n < DH_RECENT_MAX) {
        snprintf(g_rh_path[g_rh_n], sizeof(g_rh_path[g_rh_n]), "%s", path);
        g_rh_ts[g_rh_n] = now;
        g_rh_n++;
    } else {
        snprintf(g_rh_path[DH_RECENT_MAX - 1], sizeof(g_rh_path[0]), "%s", path);
        g_rh_ts[DH_RECENT_MAX - 1] = now;
    }
    save_recent_hb();
}

int dh_order_load_games(u64 *ids, int max)
{
    FILE *f = fopen(DH_ORDER_GAMES, "r");
    if (!f) {
        return 0;
    }
    int n = 0;
    char line[40];
    while (n < max && fgets(line, sizeof(line), f)) {
        unsigned long long id = 0;
        if (sscanf(line, "%llx", &id) == 1 && id) {
            ids[n++] = (u64)id;
        }
    }
    fclose(f);
    return n;
}

void dh_order_save_games(const u64 *ids, int n)
{
    mkdir("sdmc:/switch", 0755);
    mkdir("sdmc:/switch/DeckHome", 0755);
    FILE *f = fopen(DH_ORDER_GAMES, "w");
    if (!f) {
        return;
    }
    for (int i = 0; i < n; i++) {
        fprintf(f, "%016llx\n", (unsigned long long)ids[i]);
    }
    fclose(f);
}

int dh_order_load_hb(char (*paths)[768], int max)
{
    FILE *f = fopen(DH_ORDER_HB, "r");
    if (!f) {
        return 0;
    }
    int n = 0;
    char line[768];
    while (n < max && fgets(line, sizeof(line), f)) {
        trim_nl(line);
        if (!line[0]) {
            continue;
        }
        snprintf(paths[n], 768, "%s", line);
        n++;
    }
    fclose(f);
    return n;
}

void dh_order_save_hb(char (*paths)[768], int n)
{
    mkdir("sdmc:/switch", 0755);
    mkdir("sdmc:/switch/DeckHome", 0755);
    FILE *f = fopen(DH_ORDER_HB, "w");
    if (!f) {
        return;
    }
    for (int i = 0; i < n; i++) {
        fprintf(f, "%s\n", paths[i]);
    }
    fclose(f);
}
