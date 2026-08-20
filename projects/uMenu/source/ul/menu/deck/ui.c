#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "draw.h"
#include "dlog.h"
#include "dh_hb.h"
#include "layout.h"
#ifdef DH_ULAUNCH
#include "deck_smi.h"
#include "deck_sys.h"
#else
#include "ipc_client.h"
#ifdef DH_MENU_APPLET
#include "nro_loader.h"
#endif
#include "running.h"
#include "dh_mtp.h"
#endif
#include "settings.h"
#include "deck_lang.h"
#include <SDL_image.h>
#include "sfx.h"
#include "sgdb.h"
#include "titles.h"

#ifndef APP_VERSION
#define APP_VERSION "0.1.0"
#endif

enum { TAB_GAMES = 0, TAB_HB, TAB_COUNT };
enum { PICK_COLS = 5, PICK_SLOT = 148, PICK_GAP = 16, PICK_VIS_ROWS = 2,
       PICK_CARD_W = 1180, PICK_CARD_H = 600, PICK_GRID_Y = 164,
       PICK_PREV_W = 300, PICK_SIDE_GAP = 24 };
#ifdef DH_ULAUNCH
enum { MENU_SETTINGS = 0, MENU_SEARCH, MENU_ALBUM, MENU_SLEEP, MENU_REBOOT, MENU_POWER, MENU_COUNT };
#else
enum { MENU_OPTIONS = 0, MENU_SEARCH, MENU_USB, MENU_ALBUM, MENU_DONOR, MENU_SLEEP, MENU_REBOOT, MENU_POWER, MENU_COUNT };
#endif
#ifdef DH_ULAUNCH
enum {
    SETCAT_LIBRARY = 0,
    SETCAT_COVERS,
    SETCAT_AUDIO,
    SETCAT_DISPLAY,
    SETCAT_POWER,
    SETCAT_INTERNET,
    SETCAT_BLUETOOTH,
    SETCAT_SYSTEM,
    SETCAT_COUNT
};
#else
enum { SETCAT_LIBRARY = 0, SETCAT_COVERS, SETCAT_AUDIO, SETCAT_SYSTEM, SETCAT_COUNT };
#endif
enum { OV_NONE = 0, OV_CLOSE, OV_ITEM, OV_INFO, OV_UN1, OV_UN2, OV_MTP, OV_SGDB, OV_SGDB_PICK,
       OV_SGDB_FMT, OV_SGDB_SKIP, OV_BT };
enum { ITEM_START = 0, ITEM_INFO, ITEM_ART, ITEM_UNINSTALL, ITEM_DONOR };

struct DhUi {
    SDL_Renderer *r;
    DhFonts *fonts;
    const DhTheme *th;
    DhSettings cfg;
    int tab;
    int item[TAB_COUNT];
    int menu;
    int menu_item;
    float menu_t;
    int options;
    int opt_cat;
    int opt_row;
    int opt_nav;
    int opt_scroll;
    int bt_discover;
    int status_tick;
    int exit_flag;
    int toast_left;
    int focus_id;
    float card_t;
    float scroll_y;
    float marquee;
    int marquee_hold;
    u64 held;
    SDL_Texture *avatar;
    int avatar_w;
    int avatar_h;
    int wifi;
    char toast[160];
    char clock[16];
    char battery[8];
    u32 battery_pct;
    int touch_held;
    int touch_moved;
    int touch_x;
    int touch_y;
    char query[80];
    int overlay;
    int overlay_btn;
    int overlay_item;
    int sort_edit;
    u32 nav_ms;
    int nav_dx;
    int nav_dy;
    int nav_rep;
    u32 rnav_ms;
    int rnav_dx;
    int rnav_dy;
    int rnav_rep;
    u64 pending_app_id;
    char pending_hb[DH_HB_PATH_MAX];
    char confirm_name[80];
    char info_body[1400];
    u64 running_id;
    int setup_on;
    int setup_skip;
    int setup_i;
    int setup_n;
    int setup_k;
    u64 setup_id;
    int icons_warm;
    SDL_Texture *privacy;
    int privacy_w;
    int privacy_h;
    SDL_Texture *privacy_sq[5];
    int privacy_sq_w[5];
    int privacy_sq_h[5];
    SDL_Texture *privacy_cap[5];
    int privacy_cap_w[5];
    int privacy_cap_h[5];
};

static int str_has(const char *hay, const char *needle)
{
    if (!needle || !needle[0]) {
        return 1;
    }
    if (!hay) {
        return 0;
    }
    for (const char *s = hay; *s; s++) {
        const char *h = s;
        const char *n = needle;
        while (*h && *n) {
            int a = (unsigned char)*h;
            int b = (unsigned char)*n;
            if (a >= 'A' && a <= 'Z') {
                a += 32;
            }
            if (b >= 'A' && b <= 'Z') {
                b += 32;
            }
            if (a != b) {
                break;
            }
            h++;
            n++;
        }
        if (!*n) {
            return 1;
        }
    }
    return 0;
}

static const char *privacy_label(void)
{
    const char *s = dh_lang("privacy_name");
    return (s && s[0]) ? s : "Totally Real Game";
}

static const char *entry_name(int tab, int idx)
{
    if (tab == TAB_HB) {
        DhHb *hb = dh_hb_get(idx);
        return hb ? hb->name : NULL;
    }
    DhTitle *t = dh_titles_get(idx);
    return t ? t->name : NULL;
}

static int vis_count(const DhUi *ui, int tab)
{
    int n = tab == TAB_HB ? dh_hb_count() : dh_titles_count();
    if (!ui->query[0]) {
        return n;
    }
    int c = 0;
    for (int i = 0; i < n; i++) {
        if (str_has(entry_name(tab, i), ui->query)) {
            c++;
        }
    }
    return c;
}

static int vis_real(const DhUi *ui, int tab, int vis)
{
    int n = tab == TAB_HB ? dh_hb_count() : dh_titles_count();
    if (!ui->query[0]) {
        return vis;
    }
    int c = 0;
    for (int i = 0; i < n; i++) {
        if (str_has(entry_name(tab, i), ui->query)) {
            if (c == vis) {
                return i;
            }
            c++;
        }
    }
    return 0;
}

static int tab_len(const DhUi *ui)
{
    return vis_count(ui, ui->tab);
}

static const DhGridStyle *ui_style_c(const DhUi *ui)
{
    return ui->tab == TAB_HB ? &ui->cfg.hb : &ui->cfg.games;
}

static int grid_cols(const DhUi *ui)
{
    const DhGridStyle *st = ui_style_c(ui);
    int size = st->size;
    if (size < 0) {
        size = 0;
    }
    if (size > 2) {
        size = 2;
    }
    if (st->format == DH_FMT_SQUARE) {
        static const int cols[] = {6, 8, 10};
        return cols[size];
    }
    static const int cols[] = {4, 6, 8};
    return cols[size];
}

static int cell_w(const DhUi *ui)
{
    int cols = grid_cols(ui);
    return (DH_SCREEN_W - DH_GUTTER * 2 - DH_CAPSULE_GAP * (cols - 1)) / cols;
}

static int poster_h(const DhUi *ui)
{
    int w = cell_w(ui);
    if (ui_style_c(ui)->format == DH_FMT_SQUARE) {
        return w;
    }
    return (w * 3) / 2;
}

static int cell_h(const DhUi *ui)
{
    return poster_h(ui);
}

static int row_h(const DhUi *ui)
{
    return cell_h(ui) + DH_CAPSULE_GAP;
}

static const char *focus_name(DhUi *ui)
{
    if (ui->cfg.privacy_covers) {
        return privacy_label();
    }
    int n = tab_len(ui);
    if (n <= 0) {
        return NULL;
    }
    int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);
    return entry_name(ui->tab, idx);
}

static void toast(DhUi *ui, const char *msg)
{
    snprintf(ui->toast, sizeof(ui->toast), "%s", msg);
    ui->toast_left = 120;
}

static void open_search(DhUi *ui);
static int sgdb_alnum32(const char *s);
static int sgdb_key_ready(const DhUi *ui);
static int covers_udeck(const DhUi *ui);
static int covers_row_enabled(const DhUi *ui, int row);
static void covers_set_src(DhUi *ui, int src);
static void commit_easyapi_import(DhUi *ui);
static void set_nudge(DhUi *ui, int dir);
static void set_activate(DhUi *ui);
static void open_settings(DhUi *ui);
static int opt_row_enabled(const DhUi *ui, int row);
static void opt_move_row(DhUi *ui, int dir);

static const char *name_for_app(const DhUi *ui, u64 app_id)
{
    if (ui && ui->cfg.privacy_covers) {
        return privacy_label();
    }
    int n = dh_titles_count();
    for (int i = 0; i < n; i++) {
        DhTitle *t = dh_titles_get(i);
        if (t && t->application_id == app_id && t->name[0]) {
            return t->name;
        }
    }
    return "Current software";
}

enum { CONFIRM_W = 760, CONFIRM_H = 380, CONFIRM_BTN_W = 240, CONFIRM_BTN_H = 64 };

static void confirm_geom(int *x, int *y, int *ax, int *cx, int *by)
{
    *x = (DH_SCREEN_W - CONFIRM_W) / 2;
    *y = (DH_SCREEN_H - CONFIRM_H) / 2 - 24;
    *by = *y + CONFIRM_H - 96;
    *ax = *x + CONFIRM_W / 2 - 16 - CONFIRM_BTN_W;
    *cx = *x + CONFIRM_W / 2 + 16;
}

static void close_overlay(DhUi *ui)
{
#ifndef DH_ULAUNCH
    if (ui->overlay == OV_MTP) {
        dh_mtp_stop();
    }
#endif
    if (ui->overlay == OV_SGDB_PICK) {
        dh_sgdb_pick_clear();
    }
    ui->setup_on = 0;
    ui->overlay = OV_NONE;
    ui->overlay_btn = 0;
    ui->overlay_item = 0;
    ui->pending_app_id = 0;
    ui->pending_hb[0] = 0;
}

static void open_close_confirm(DhUi *ui, u64 running_id, u64 next_app, const char *hb_path)
{
    ui->overlay = OV_CLOSE;
    ui->overlay_btn = 1;
    ui->pending_app_id = next_app;
    if (hb_path && hb_path[0]) {
        snprintf(ui->pending_hb, sizeof(ui->pending_hb), "%s", hb_path);
    } else {
        ui->pending_hb[0] = 0;
    }
    const char *nm = name_for_app(ui, running_id);
    size_t nlen = strlen(nm);
    if (nlen >= sizeof(ui->confirm_name)) {
        nlen = sizeof(ui->confirm_name) - 1;
    }
    memcpy(ui->confirm_name, nm, nlen);
    ui->confirm_name[nlen] = 0;
}

static int item_fill(const DhUi *ui, int *ids)
{
    int n = 0;
    ids[n++] = ITEM_START;
    ids[n++] = ITEM_INFO;
    if (ui->tab == TAB_GAMES) {
        ids[n++] = ITEM_ART;
    }
    ids[n++] = ITEM_UNINSTALL;
#ifndef DH_ULAUNCH
    if (ui->tab == TAB_GAMES) {
        ids[n++] = ITEM_DONOR;
    }
#endif
    return n;
}

static int item_menu_n(const DhUi *ui)
{
    int ids[8];
    return item_fill(ui, ids);
}

static int item_id_at(const DhUi *ui, int vis)
{
    int ids[8];
    int n = item_fill(ui, ids);
    if (vis < 0 || vis >= n) {
        return ITEM_START;
    }
    return ids[vis];
}

static const char *item_lab(int id)
{
    if (id == ITEM_INFO) {
        return "Info";
    }
    if (id == ITEM_ART) {
        return "Choose cover";
    }
    if (id == ITEM_UNINSTALL) {
        return "Uninstall";
    }
    if (id == ITEM_DONOR) {
        return "Set as homebrew donor";
    }
    return "Start";
}

static void open_sgdb_progress(DhUi *ui)
{
    ui->overlay = OV_SGDB;
    ui->overlay_btn = 0;
    ui->overlay_item = 0;
}

#ifdef DH_ULAUNCH
static void report_smi(DhUi *ui, const char *what, int rc)
{
    if (rc != 0) {
        dlog("ui", "%s rc=0x%x", what, rc);
        char buf[80];
        snprintf(buf, sizeof(buf), "%s rc=0x%x", what, rc);
        toast(ui, buf);
    }
}

static int query_running(DhUi *ui, u64 *out_id)
{
    u64 id = 0;
    char hb[DH_HB_PATH_MAX];
    hb[0] = 0;
    int open = deck_smi_is_suspended(&id, hb, sizeof(hb));
    if (open) {
        ui->running_id = id;
        if (out_id) {
            *out_id = id;
        }
        return 1;
    }
    ui->running_id = 0;
    if (out_id) {
        *out_id = 0;
    }
    return 0;
}

static u64 first_title_id(void)
{
    int n = dh_titles_count();
    for (int i = 0; i < n; i++) {
        DhTitle *t = dh_titles_get(i);
        if (t && t->application_id) {
            return t->application_id;
        }
    }
    return 0;
}

static void launch_hb(DhUi *ui, const char *path)
{
    dh_hb_touch(path);
    dh_sfx_launch();
    if (!deck_smi_get_hb_takeover()) {
        u64 take = first_title_id();
        if (take) {
            deck_smi_ensure_hb_takeover(take);
        }
    }
    int rc = deck_smi_launch_hb_app(path);
    if (rc == 0) {
        ui->exit_flag = 1;
        return;
    }
    dlog("ui", "hb as app failed rc=0x%x, try applet", rc);
    rc = deck_smi_launch_hb_applet(path);
    if (rc == 0) {
        ui->exit_flag = 1;
        return;
    }
    report_smi(ui, "Launch HB", rc);
}
#else
static void report_ipc(DhUi *ui, const char *what, DhIpcStatus st);

static int query_running(DhUi *ui, u64 *out_id)
{
    int open = 0;
    u64 id = 0;
    DhIpcStatus q = dh_ipc_query_app(&open, &id);
    if (q == DH_IPC_OK) {
        if (open && id) {
            ui->running_id = id;
            if (out_id) {
                *out_id = id;
            }
            return 1;
        }
        if (ui->running_id) {
            dh_running_set(0);
        }
        ui->running_id = 0;
        if (out_id) {
            *out_id = 0;
        }
        return 0;
    }
    ui->running_id = 0;
    if (out_id) {
        *out_id = 0;
    }
    return 0;
}

static void report_ipc(DhUi *ui, const char *what, DhIpcStatus st)
{
    if (st != DH_IPC_OK) {
        dlog("ui", "%s status=%u last_rc=0x%x", what, st, dh_ipc_last_rc());
        char buf[80];
        snprintf(buf, sizeof(buf), "%s st=%u rc=0x%x", what, (unsigned)st, dh_ipc_last_rc());
        toast(ui, buf);
    }
}

static void launch_hb(DhUi *ui, const char *path)
{
    dh_hb_touch(path);
    dh_sfx_launch();
    if (envHasNextLoad()) {
        char args[DH_HB_PATH_MAX + 8];
        snprintf(args, sizeof(args), "%s", path);
        envSetNextLoad(path, args);
        ui->exit_flag = 1;
        return;
    }
#ifdef DH_MENU_APPLET
    int as_app = (ui->held & HidNpadButton_L) == 0;
    if (dh_nro_request_ex(path, as_app)) {
        ui->exit_flag = 1;
        return;
    }
    toast(ui, "Could not queue homebrew");
#else
    toast(ui, "Cannot chain-load (need hbmenu or DeckHome sys)");
#endif
}
#endif

static void start_hb(DhUi *ui, const char *path)
{
    u64 running = 0;
#ifdef DH_ULAUNCH
    char cur_hb[DH_HB_PATH_MAX];
    cur_hb[0] = 0;
    if (deck_smi_is_suspended(&running, cur_hb, sizeof(cur_hb))) {
        if (cur_hb[0] && path && strcmp(cur_hb, path) == 0) {
            report_smi(ui, "Resume", deck_smi_resume_app());
            return;
        }
        open_close_confirm(ui, running, 0, path);
        return;
    }
#else
    if (query_running(ui, &running)) {
        open_close_confirm(ui, running, 0, path);
        return;
    }
#endif
    launch_hb(ui, path);
}

static void launch_game(DhUi *ui, u64 app_id, const char *name)
{
    dh_titles_touch(app_id);
#ifdef DH_ULAUNCH
    u64 running = 0;
    char cur_hb[DH_HB_PATH_MAX];
    cur_hb[0] = 0;
    if (deck_smi_is_suspended(&running, cur_hb, sizeof(cur_hb))) {
        if (!cur_hb[0] && running == app_id) {
            report_smi(ui, "Resume", deck_smi_resume_app());
            return;
        }
        open_close_confirm(ui, running ? running : app_id, app_id, NULL);
        return;
    }
    dh_sfx_launch();
    int rc = deck_smi_launch_app(app_id);
    if (rc == 0) {
        ui->exit_flag = 1;
        return;
    }
    report_smi(ui, "Launch", rc);
#else
    dh_sfx_launch();
    DhIpcStatus st = dh_ipc_launch_app(app_id);
    if (st == DH_IPC_BUSY) {
        u64 running = dh_ipc_last_app_id();
        if (!running) {
            running = ui->running_id;
        }
        open_close_confirm(ui, running ? running : app_id, app_id, NULL);
        return;
    }
    report_ipc(ui, "Launch", st);
#endif
}

static void confirm_yes(DhUi *ui)
{
    u64 next = ui->pending_app_id;
    char hb[DH_HB_PATH_MAX];
    snprintf(hb, sizeof(hb), "%s", ui->pending_hb);
    close_overlay(ui);
#ifdef DH_ULAUNCH
    deck_smi_terminate_app();
#endif
    if (hb[0]) {
        launch_hb(ui, hb);
        return;
    }
    if (next) {
        launch_game(ui, next, NULL);
    }
}

static void launch_current(DhUi *ui);

static void clamp_item(DhUi *ui)
{
    int n = tab_len(ui);
    if (n < 1) {
        ui->item[ui->tab] = 0;
        return;
    }
    if (ui->item[ui->tab] >= n) {
        ui->item[ui->tab] = n - 1;
    }
    if (ui->item[ui->tab] < 0) {
        ui->item[ui->tab] = 0;
    }
}

static void load_avatar(DhUi *ui)
{
    AccountUid uid;
    if (R_FAILED(accountGetPreselectedUser(&uid))) {
        return;
    }
    AccountProfile profile;
    if (R_FAILED(accountGetProfile(&profile, uid))) {
        return;
    }
    u32 sz = 0;
    if (R_FAILED(accountProfileGetImageSize(&profile, &sz)) || sz < 8 || sz > 256 * 1024) {
        accountProfileClose(&profile);
        return;
    }
    void *buf = malloc(sz);
    if (!buf) {
        accountProfileClose(&profile);
        return;
    }
    u32 actual = 0;
    Result rc = accountProfileLoadImage(&profile, buf, sz, &actual);
    accountProfileClose(&profile);
    if (R_SUCCEEDED(rc) && actual > 0) {
        ui->avatar = dh_texture_from_mem(ui->r, buf, (int)actual, &ui->avatar_w, &ui->avatar_h);
    }
    free(buf);
}

DhUi *dh_ui_create(SDL_Renderer *r, DhFonts *fonts, const DhTheme *th)
{
    DhUi *ui = (DhUi *)SDL_calloc(1, sizeof(DhUi));
    if (!ui) {
        return NULL;
    }
    ui->r = r;
    ui->fonts = fonts;
    ui->th = th;
    ui->focus_id = -1;
    dh_draw_init(r);
    dh_settings_load(&ui->cfg);
    dh_sfx_init();
    dh_sfx_bind(&ui->cfg);
    dh_titles_apply_sort(ui->cfg.sort);
    dh_sgdb_apply(&ui->cfg);
    dh_lang_apply(ui->cfg.lang);
    dh_hb_ensure();
    dh_hb_apply_sort(ui->cfg.sort);
    load_avatar(ui);
    {
        SDL_Surface *surf = IMG_Load("romfs:/Icon.png");
        if (surf) {
            ui->privacy = SDL_CreateTextureFromSurface(r, surf);
            if (ui->privacy) {
                SDL_SetTextureScaleMode(ui->privacy, SDL_ScaleModeLinear);
                ui->privacy_w = surf->w;
                ui->privacy_h = surf->h;
            }
            SDL_FreeSurface(surf);
        }
        for (int i = 0; i < 5; i++) {
            char path[48];
            snprintf(path, sizeof(path), "romfs:/privacy/sq%d.jpg", i + 1);
            surf = IMG_Load(path);
            if (surf) {
                ui->privacy_sq[i] = SDL_CreateTextureFromSurface(r, surf);
                if (ui->privacy_sq[i]) {
                    SDL_SetTextureScaleMode(ui->privacy_sq[i], SDL_ScaleModeLinear);
                    ui->privacy_sq_w[i] = surf->w;
                    ui->privacy_sq_h[i] = surf->h;
                }
                SDL_FreeSurface(surf);
            }
            snprintf(path, sizeof(path), "romfs:/privacy/cap%d.jpg", i + 1);
            surf = IMG_Load(path);
            if (surf) {
                ui->privacy_cap[i] = SDL_CreateTextureFromSurface(r, surf);
                if (ui->privacy_cap[i]) {
                    SDL_SetTextureScaleMode(ui->privacy_cap[i], SDL_ScaleModeLinear);
                    ui->privacy_cap_w[i] = surf->w;
                    ui->privacy_cap_h[i] = surf->h;
                }
                SDL_FreeSurface(surf);
            }
        }
    }
    return ui;
}

void dh_ui_reload_user(DhUi *ui)
{
    if (!ui) {
        return;
    }
    if (ui->avatar) {
        SDL_DestroyTexture(ui->avatar);
        ui->avatar = NULL;
        ui->avatar_w = 0;
        ui->avatar_h = 0;
    }
    load_avatar(ui);
}

void dh_ui_destroy(DhUi *ui)
{
    if (!ui) {
        return;
    }
#ifndef DH_ULAUNCH
    dh_mtp_stop();
#endif
    if (ui->avatar) {
        SDL_DestroyTexture(ui->avatar);
    }
    if (ui->privacy) {
        SDL_DestroyTexture(ui->privacy);
    }
    for (int i = 0; i < 5; i++) {
        if (ui->privacy_sq[i]) {
            SDL_DestroyTexture(ui->privacy_sq[i]);
        }
        if (ui->privacy_cap[i]) {
            SDL_DestroyTexture(ui->privacy_cap[i]);
        }
    }
    dh_sgdb_exit();
    dh_sfx_exit();
    dh_draw_shutdown();
    dh_hb_exit();
    SDL_free(ui);
}

void dh_ui_set_held(DhUi *ui, u64 keys)
{
    if (ui) {
        ui->held = keys;
    }
}

int dh_ui_wants_exit(const DhUi *ui)
{
    return ui && ui->exit_flag;
}

int dh_ui_is_confirm(const DhUi *ui)
{
    return ui && (ui->overlay == OV_CLOSE || ui->overlay == OV_UN1 || ui->overlay == OV_UN2
        || ui->overlay == OV_MTP || ui->overlay == OV_SGDB_FMT || ui->overlay == OV_SGDB_SKIP
        || ui->overlay == OV_BT);
}

static void apply_library_sort(DhUi *ui)
{
    dh_titles_apply_sort(ui->cfg.sort);
    dh_hb_apply_sort(ui->cfg.sort);
    clamp_item(ui);
}

static void set_tab(DhUi *ui, int tab)
{
    if (tab < 0 || tab >= TAB_COUNT) {
        return;
    }
    int changed = ui->tab != tab;
    ui->tab = tab;
    ui->scroll_y = 0.f;
    ui->card_t = 0.f;
    ui->marquee = 0.f;
    ui->marquee_hold = DH_MARQUEE_HOLD;
    ui->sort_edit = 0;
    if (tab == TAB_HB) {
        dh_hb_ensure();
        dh_hb_apply_sort(ui->cfg.sort);
    }
    if (changed) {
        dh_sfx_nav();
    }
}

#ifndef DH_ULAUNCH
static void launch_usb_mtp(DhUi *ui)
{
    if (dh_mtp_start() != 0) {
        toast(ui, "USB MTP failed");
        return;
    }
    ui->overlay = OV_MTP;
    toast(ui, "USB MTP on");
}
#endif

static void launch_current(DhUi *ui)
{
    clamp_item(ui);
    int n = tab_len(ui);
    if (n <= 0) {
        toast(ui, "Nothing to launch");
        return;
    }
    int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);

    if (ui->tab == TAB_HB) {
        DhHb *hb = dh_hb_get(idx);
        if (!hb) {
            return;
        }
        start_hb(ui, hb->path);
        return;
    }

    DhTitle *t = dh_titles_get(idx);
    if (!t) {
        return;
    }
    int open = 0;
    u64 running = 0;
    open = query_running(ui, &running);
    if (open) {
        if (running == t->application_id) {
#ifdef DH_ULAUNCH
            report_smi(ui, "Resume", deck_smi_resume_app());
#else
            report_ipc(ui, "Resume", dh_ipc_resume_app());
#endif
            return;
        }
        open_close_confirm(ui, running, t->application_id, NULL);
        return;
    }
    launch_game(ui, t->application_id, t->name);
}

static void open_item_menu(DhUi *ui)
{
    clamp_item(ui);
    if (tab_len(ui) <= 0) {
        toast(ui, "Nothing selected");
        return;
    }
    ui->overlay = OV_ITEM;
    ui->overlay_item = ITEM_START;
}

static void fill_info_overlay(DhUi *ui)
{
    int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);
    ui->info_body[0] = 0;
    char *b = ui->info_body;
    size_t cap = sizeof(ui->info_body);
    size_t n = 0;
    if (ui->cfg.privacy_covers) {
        snprintf(b, cap, "%s\n", privacy_label());
        return;
    }
    if (ui->tab == TAB_HB) {
        DhHb *hb = dh_hb_get(idx);
        if (!hb) {
            return;
        }
        n += (size_t)snprintf(b + n, cap - n, "%s\n", hb->name);
        if (hb->publisher[0]) {
            n += (size_t)snprintf(b + n, cap - n, "Author  %s\n", hb->publisher);
        }
        if (hb->version[0]) {
            n += (size_t)snprintf(b + n, cap - n, "Version  %s\n", hb->version);
        }
        n += (size_t)snprintf(b + n, cap - n, "Path  %s\n", hb->path);
        if (hb->size) {
            n += (size_t)snprintf(b + n, cap - n, "Size  %llu KB\n", (unsigned long long)(hb->size / 1024ull));
        }
        (void)n;
        return;
    }
    DhTitle *t = dh_titles_get(idx);
    if (!t) {
        return;
    }
    DhTitleInfo info;
    dh_titles_fill_info(t->application_id, &info);
    n += (size_t)snprintf(b + n, cap - n, "%s\n", info.name);
    if (info.publisher[0]) {
        n += (size_t)snprintf(b + n, cap - n, "Publisher  %s\n", info.publisher);
    }
    if (info.version[0]) {
        n += (size_t)snprintf(b + n, cap - n, "Version  %s\n", info.version);
    }
    n += (size_t)snprintf(b + n, cap - n, "Title ID  %s\n", info.id_hex);
    n += (size_t)snprintf(b + n, cap - n, "Update  %s\n", info.update);
    n += (size_t)snprintf(b + n, cap - n, "DLC  %d installed\n", info.dlc_n);
    for (int i = 0; i < info.dlc_n; i++) {
        n += (size_t)snprintf(b + n, cap - n, "  %s\n", info.dlc[i]);
        if (n + 8 >= cap) {
            break;
        }
    }
}

static void do_uninstall(DhUi *ui)
{
    int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);
    close_overlay(ui);
    if (ui->tab == TAB_HB) {
        DhHb *hb = dh_hb_get(idx);
        if (!hb) {
            return;
        }
        if (dh_hb_uninstall(idx) == 0) {
            toast(ui, "Homebrew removed");
            apply_library_sort(ui);
        } else {
            toast(ui, "Could not delete NRO");
        }
        return;
    }
    DhTitle *t = dh_titles_get(idx);
    if (!t) {
        return;
    }
    u64 id = t->application_id;
    u64 running = 0;
    if (query_running(ui, &running) && running == id) {
#ifdef DH_ULAUNCH
        deck_smi_terminate_app();
#else
        dh_ipc_terminate_app();
#endif
    }
#ifdef DH_ULAUNCH
    toast(ui, "Uninstall games in Settings");
    return;
#else
    DhIpcStatus st = dh_ipc_delete_app(id);
    if (st != DH_IPC_OK) {
        report_ipc(ui, "Uninstall", st);
        return;
    }
#endif
    toast(ui, "Uninstalled");
    dh_titles_refresh();
    apply_library_sort(ui);
}

#ifdef DH_MENU_APPLET
static void set_donor_from_selection(DhUi *ui);
#endif

static void item_activate(DhUi *ui)
{
    int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);
    int id = item_id_at(ui, ui->overlay_item);
    if (id == ITEM_START) {
        close_overlay(ui);
        launch_current(ui);
        return;
    }
    if (id == ITEM_INFO) {
        fill_info_overlay(ui);
        ui->overlay = OV_INFO;
        return;
    }
    if (id == ITEM_ART) {
        DhTitle *t = dh_titles_get(idx);
        if (!t) {
            return;
        }
        if (covers_udeck(ui)) {
            dh_sgdb_apply(&ui->cfg);
            dh_sgdb_pick_begin(t->application_id, t->name);
            open_sgdb_progress(ui);
            return;
        }
        if (!sgdb_key_ready(ui)) {
            toast(ui, "Set an API key in Settings → Covers");
            return;
        }
        dh_sgdb_apply(&ui->cfg);
        dh_sgdb_pick_begin(t->application_id, t->name);
        ui->overlay = OV_SGDB_PICK;
        ui->overlay_item = 0;
        return;
    }
#ifdef DH_MENU_APPLET
    if (id == ITEM_DONOR && ui->tab == TAB_GAMES) {
        close_overlay(ui);
        set_donor_from_selection(ui);
        return;
    }
#endif
    const char *nm = ui->cfg.privacy_covers ? privacy_label() : entry_name(ui->tab, idx);
    snprintf(ui->confirm_name, sizeof(ui->confirm_name), "%s", nm ? nm : "this");
    ui->overlay = OV_UN1;
    ui->overlay_btn = 1;
}

#ifdef DH_MENU_APPLET
static void set_donor_from_selection(DhUi *ui)
{
    if (ui->tab != TAB_GAMES) {
        toast(ui, "Select a game first");
        return;
    }
    int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);
    DhTitle *t = dh_titles_get(idx);
    if (!t || !t->application_id) {
        toast(ui, "Select a game first");
        return;
    }
    dh_nro_set_donor(t->application_id);
    char buf[96];
    snprintf(buf, sizeof(buf), "Donor: %s", ui->cfg.privacy_covers ? privacy_label() : (t->name[0] ? t->name : t->id_hex));
    toast(ui, buf);
}
#endif

static void menu_activate(DhUi *ui)
{
#ifdef DH_ULAUNCH
    switch (ui->menu_item) {
        case MENU_SETTINGS:
            open_settings(ui);
            return;
        case MENU_SEARCH:
            ui->menu = 0;
            open_search(ui);
            return;
        case MENU_SLEEP:
            deck_smi_sleep();
            break;
        case MENU_REBOOT:
            deck_smi_reboot();
            break;
        case MENU_POWER:
            deck_smi_power_off();
            break;
        case MENU_ALBUM:
            if (ui->held & HidNpadButton_R) {
                deck_smi_launch_hb_applet("sdmc:/hbmenu.nro");
            } else {
                deck_smi_open_album();
            }
            break;
        default:
            break;
    }
#else
    DhIpcStatus st = DH_IPC_NO_SYS;
    switch (ui->menu_item) {
        case MENU_OPTIONS:
            ui->menu = 0;
            ui->options = 1;
            ui->opt_cat = SETCAT_LIBRARY;
            ui->opt_row = 0;
            ui->opt_nav = 1;
            return;
        case MENU_SEARCH:
            ui->menu = 0;
            open_search(ui);
            return;
        case MENU_USB:
            ui->menu = 0;
            launch_usb_mtp(ui);
            return;
        case MENU_SLEEP:
            st = dh_ipc_sleep();
            if (st == DH_IPC_NO_SYS) {
                toast(ui, "Sleep needs DeckHome sys");
            }
            break;
        case MENU_REBOOT:
            st = dh_ipc_reboot();
            if (st == DH_IPC_NO_SYS) {
                toast(ui, "Reboot needs DeckHome sys");
            }
            break;
        case MENU_POWER:
            st = dh_ipc_power_off();
            if (st == DH_IPC_NO_SYS) {
                toast(ui, "Power off needs DeckHome sys");
            }
            break;
        case MENU_ALBUM:
            st = dh_ipc_open_album();
            if (st == DH_IPC_NO_SYS) {
                toast(ui, "Album needs DeckHome sys");
            }
            break;
#ifdef DH_MENU_APPLET
        case MENU_DONOR:
            ui->menu = 0;
            set_donor_from_selection(ui);
            return;
#endif
        default:
            break;
    }
#endif
}

static void apply_query(DhUi *ui, const char *q)
{
    snprintf(ui->query, sizeof(ui->query), "%s", q ? q : "");
    ui->item[TAB_GAMES] = 0;
    ui->item[TAB_HB] = 0;
    ui->scroll_y = 0.f;
    ui->card_t = 0.f;
    ui->marquee = 0.f;
    ui->marquee_hold = DH_MARQUEE_HOLD;
}

static void open_search(DhUi *ui)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) {
        toast(ui, "Keyboard unavailable");
        return;
    }
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetGuideText(&kbd, "Search games and homebrew");
    if (ui->query[0]) {
        swkbdConfigSetInitialText(&kbd, ui->query);
    }
    swkbdConfigSetStringLenMax(&kbd, (u32)sizeof(ui->query) - 1);
    char out[80];
    out[0] = 0;
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_SUCCEEDED(rc)) {
        apply_query(ui, out);
    }
}

static int sgdb_alnum32(const char *s)
{
    int n = 0;
    if (!s) {
        return 0;
    }
    for (; s[n]; n++) {
        char c = s[n];
        int ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!ok) {
            return 0;
        }
    }
    return n == 32;
}

static int sgdb_key_ready(const DhUi *ui)
{
    return ui && sgdb_alnum32(ui->cfg.sgdb_key);
}

static int covers_udeck(const DhUi *ui)
{
    return ui && ui->cfg.cover_src == DH_COVER_UDECK;
}

static void sgdb_compact(const char *in, char *out, size_t cap)
{
    size_t n = 0;
    if (!in || cap == 0) {
        if (out && cap) {
            out[0] = 0;
        }
        return;
    }
    for (; *in && n + 1 < cap; in++) {
        if (*in == ' ' || *in == '\n' || *in == '\r' || *in == '\t') {
            continue;
        }
        out[n++] = *in;
    }
    out[n] = 0;
}

static void apply_sgdb_input(DhUi *ui, const char *raw)
{
    char buf[64];
    sgdb_compact(raw, buf, sizeof(buf));
    if (!buf[0]) {
        ui->cfg.sgdb_key[0] = 0;
        ui->cfg.sgdb_key_ok = 0;
        dh_settings_save(&ui->cfg);
        dh_sgdb_apply(&ui->cfg);
        toast(ui, "API key cleared");
        return;
    }
    if (strlen(buf) == 4) {
        for (char *p = buf; *p; p++) {
            if (*p >= 'a' && *p <= 'z') {
                *p = (char)(*p - 32);
            }
        }
        dh_sgdb_redeem_easyapi(buf);
        open_sgdb_progress(ui);
        return;
    }
    if (!sgdb_alnum32(buf)) {
        toast(ui, "Need 32 letters/numbers, or a 4-character EasyAPI code");
        return;
    }
    snprintf(ui->cfg.sgdb_key, sizeof(ui->cfg.sgdb_key), "%s", buf);
    ui->cfg.sgdb_key_ok = 0;
    dh_settings_save(&ui->cfg);
    dh_sgdb_apply(&ui->cfg);
    toast(ui, "API key saved");
}

static int open_sgdb_swkbd(DhUi *ui, const char *guide, const char *initial, char *out, size_t out_n)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) {
        toast(ui, "Keyboard unavailable");
        return 0;
    }
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_All);
    swkbdConfigSetGuideText(&kbd, guide);
    if (initial && initial[0]) {
        swkbdConfigSetInitialText(&kbd, initial);
    }
    swkbdConfigSetStringLenMax(&kbd, 32);
    out[0] = 0;
    Result rc = swkbdShow(&kbd, out, out_n);
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc);
}

static void open_easyapi_code(DhUi *ui)
{
    char out[64];
    if (!open_sgdb_swkbd(ui, "4-character EasyAPI code, or paste the 32-character key", NULL, out, sizeof(out))) {
        return;
    }
    if (!out[0]) {
        return;
    }
    apply_sgdb_input(ui, out);
}

static void open_sgdb_key(DhUi *ui)
{
    char out[64];
    if (!open_sgdb_swkbd(ui, "Paste or type the 32-character SteamGridDB key",
            ui->cfg.sgdb_key[0] ? ui->cfg.sgdb_key : NULL, out, sizeof(out))) {
        return;
    }
    apply_sgdb_input(ui, out);
}

static void open_sgdb_key_file(DhUi *ui)
{
    static const char *paths[] = {
        "sdmc:/switch/uDeckLaunch/sgdb.key",
        "sdmc:/switch/uDeckLaunch/sgdb_key.txt",
    };
    char raw[96];
    raw[0] = 0;
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *f = fopen(paths[i], "rb");
        if (!f) {
            continue;
        }
        size_t n = fread(raw, 1, sizeof(raw) - 1, f);
        fclose(f);
        raw[n] = 0;
        break;
    }
    if (!raw[0]) {
        toast(ui, "Put the key in switch/uDeckLaunch/sgdb.key");
        return;
    }
    apply_sgdb_input(ui, raw);
}

static DhTitle *setup_title(DhUi *ui)
{
    if (!ui->setup_on) {
        return NULL;
    }
    return dh_titles_get(ui->setup_i);
}

static int setup_should_visit(DhUi *ui, DhTitle *t)
{
    if (!t) {
        return 0;
    }
    if (ui->setup_skip && dh_sgdb_has_cover(t->application_id)) {
        return 0;
    }
    return 1;
}

static int setup_count(DhUi *ui)
{
    int n = 0;
    int t = dh_titles_count();
    for (int i = 0; i < t; i++) {
        if (setup_should_visit(ui, dh_titles_get(i))) {
            n++;
        }
    }
    return n;
}

static void setup_next(DhUi *ui)
{
    int n = dh_titles_count();
    for (;;) {
        ui->setup_i++;
        if (ui->setup_i >= n) {
            dh_sgdb_pick_clear();
            ui->setup_on = 0;
            ui->overlay = OV_NONE;
            ui->overlay_item = 0;
            toast(ui, "Setup done");
            return;
        }
        DhTitle *t = dh_titles_get(ui->setup_i);
        if (!setup_should_visit(ui, t)) {
            continue;
        }
        ui->setup_id = t->application_id;
        ui->setup_k++;
        dh_sgdb_pick_begin(t->application_id, t->name);
        ui->overlay = OV_SGDB_PICK;
        ui->overlay_item = 0;
        return;
    }
}

static void setup_begin_queue(DhUi *ui)
{
    ui->setup_on = 1;
    ui->setup_i = -1;
    ui->setup_k = 0;
    ui->setup_id = 0;
    ui->setup_n = setup_count(ui);
    if (ui->setup_n < 1) {
        ui->setup_on = 0;
        ui->overlay = OV_NONE;
        toast(ui, "Nothing to set up");
        return;
    }
    setup_next(ui);
}

static void start_sgdb_download(DhUi *ui)
{
    commit_easyapi_import(ui);
    if (!covers_udeck(ui) && !sgdb_key_ready(ui)) {
        toast(ui, "Set an API key first");
        return;
    }
    ui->setup_on = 0;
    ui->overlay = OV_SGDB_FMT;
    ui->overlay_btn = ui->cfg.games.format == DH_FMT_SQUARE ? 0 : 1;
    ui->overlay_item = 0;
}

static void commit_easyapi_import(DhUi *ui)
{
    char imported[128];
    if (!dh_sgdb_take_imported(imported, sizeof(imported))) {
        return;
    }
    snprintf(ui->cfg.sgdb_key, sizeof(ui->cfg.sgdb_key), "%s", imported);
    ui->cfg.sgdb_key_ok = 0;
    dh_settings_save(&ui->cfg);
    dh_sgdb_apply(&ui->cfg);
    dlog("sgdb", "EasyAPI saved key_len=%u", (unsigned)strlen(imported));
    toast(ui, "Key imported");
}

static void start_sgdb_test(DhUi *ui)
{
    commit_easyapi_import(ui);
    if (!sgdb_alnum32(ui->cfg.sgdb_key)) {
        toast(ui, "Set an API key first");
        return;
    }
    dh_sgdb_apply(&ui->cfg);
    dh_sgdb_test_key();
    open_sgdb_progress(ui);
}

enum { LIB_ROW_N = 10 };
enum {
    RK_HEAD = 0,
    RK_SWITCH,
    RK_SEG,
    RK_ACTION,
    RK_INFO,
    RK_CYCLE
};
enum {
    RID_AUD_MASTER = 0,
    RID_AUD_NAV,
    RID_AUD_LAUNCH,
    RID_AUD_STARTUP,
    RID_LOCKSCREEN,
    RID_ALBUM,
    RID_THEME,
    RID_SLEEP_HH,
    RID_SLEEP_DOCK,
    RID_SLEEP_MEDIA,
    RID_SLEEP_WAKE,
    RID_WIFI,
    RID_WLAN,
    RID_AUTO_UPD,
    RID_AUTO_DL,
    RID_INFO_UP,
    RID_IP,
    RID_MAC,
    RID_BT_EN,
    RID_BT_FIND,
    RID_BT_DEV,
    RID_NICK,
    RID_LANG,
    RID_USB30,
    RID_NFC,
    RID_CAPTURE,
    RID_HB_APP,
    RID_TAKEOVER,
    RID_DONOR,
    RID_ABOUT_HEAD,
    RID_VER,
    RID_GAMES,
    RID_HB,
    RID_FW,
    RID_AMS,
    RID_EMUMMC,
    RID_REGION,
    RID_TZ,
    RID_SERIAL,
    RID_BATTERY,
    RID_AUDSVC
};
enum {
    COVERS_ROW_SRC = 0,
    COVERS_ROW_EASY,
    COVERS_ROW_KEY,
    COVERS_ROW_FILE,
    COVERS_ROW_TEST,
    COVERS_ROW_SETUP,
    COVERS_ROW_RESET,
    COVERS_ROW_PRIVACY,
    COVERS_ROW_N = 8
};

static DhGridStyle *lib_style(DhUi *ui, int row)
{
    if (row <= 0 || row >= 7) {
        return &ui->cfg.games;
    }
    return row <= 3 ? &ui->cfg.games : &ui->cfg.hb;
}

static const DhGridStyle *lib_style_c(const DhUi *ui, int row)
{
    if (row <= 0 || row >= 7) {
        return &ui->cfg.games;
    }
    return row <= 3 ? &ui->cfg.games : &ui->cfg.hb;
}

static int lib_field(int row)
{
    if (row <= 0 || row >= 7) {
        return 3;
    }
    return (row - 1) % 3;
}

static int lib_seg_count(int row)
{
    if (row == 0) {
        return 3;
    }
    if (row == 7) {
        return 3;
    }
    if (row == 8) {
        return 2;
    }
    if (row == 9) {
        return 4;
    }
    return lib_field(row) == 0 ? 2 : 3;
}

static int lib_value(const DhUi *ui, int row)
{
    if (row == 0) {
        return ui->cfg.border;
    }
    if (row == 7) {
        return ui->cfg.edge;
    }
    if (row == 8) {
        return ui->cfg.stick_diag ? 1 : 0;
    }
    if (row == 9) {
        return ui->cfg.sort;
    }
    const DhGridStyle *st = lib_style_c(ui, row);
    int f = lib_field(row);
    if (f == 0) {
        return st->format;
    }
    if (f == 1) {
        return st->size;
    }
    return st->scale;
}

static void lib_apply(DhUi *ui, int row, int value)
{
    if (row == 0) {
        if (value < 0) {
            value = 0;
        }
        if (value > 2) {
            value = 2;
        }
        ui->cfg.border = value;
        dh_settings_save(&ui->cfg);
        return;
    }
    if (row == 7) {
        if (value < 0) {
            value = 0;
        }
        if (value > 2) {
            value = 2;
        }
        ui->cfg.edge = value;
        dh_settings_save(&ui->cfg);
        return;
    }
    if (row == 8) {
        ui->cfg.stick_diag = value ? DH_STICK_DIAGONAL : DH_STICK_CARDINAL;
        dh_settings_save(&ui->cfg);
        return;
    }
    if (row == 9) {
        if (value < 0) {
            value = 0;
        }
        if (value > 3) {
            value = 3;
        }
        ui->cfg.sort = value;
        ui->sort_edit = 0;
        apply_library_sort(ui);
        dh_settings_save(&ui->cfg);
        return;
    }
    DhGridStyle *st = lib_style(ui, row);
    int f = lib_field(row);
    int n = lib_seg_count(row);
    if (value < 0) {
        value = 0;
    }
    if (value >= n) {
        value = n - 1;
    }
    if (f == 0) {
        st->format = value;
        ui->scroll_y = 0.f;
        ui->card_t = 0.f;
        clamp_item(ui);
        if (value == DH_FMT_SQUARE && lib_field(ui->opt_row) == 2 && lib_style(ui, ui->opt_row) == st) {
            ui->opt_row = row <= 3 ? 2 : 5;
        }
        if (st == &ui->cfg.games) {
            dh_sgdb_apply(&ui->cfg);
        }
    } else if (f == 1) {
        st->size = value;
        ui->scroll_y = 0.f;
        ui->card_t = 0.f;
        clamp_item(ui);
    } else {
        st->scale = value;
    }
    dh_settings_save(&ui->cfg);
}

static int options_body_y(void)
{
    return DH_OPT_TOP_H + 52;
}

static int options_nav_y(void)
{
    return DH_OPT_TOP_H + 16;
}

static int lib_row_hidden(const DhUi *ui, int row)
{
    if (row == 0 || row >= 7) {
        return 0;
    }
    return lib_field(row) == 2 && lib_style_c(ui, row)->format == DH_FMT_SQUARE;
}

static void lib_pack_ys(const DhUi *ui, int head_y[2], int row_y[10], int vis[10])
{
    int y = options_body_y();
    vis[0] = 1;
    row_y[0] = y;
    y += DH_SET_CHOICE_H + DH_SET_ROW_GAP;
    for (int block = 0; block < 2; block++) {
        head_y[block] = y;
        y += DH_SET_HEAD_H;
        for (int i = 0; i < 3; i++) {
            int row = 1 + block * 3 + i;
            if (lib_row_hidden(ui, row)) {
                vis[row] = 0;
                row_y[row] = 0;
                continue;
            }
            vis[row] = 1;
            row_y[row] = y;
            y += DH_SET_CHOICE_H + DH_SET_ROW_GAP;
        }
    }
    vis[7] = 1;
    row_y[7] = y;
    y += DH_SET_CHOICE_H + DH_SET_ROW_GAP;
    vis[8] = 1;
    row_y[8] = y;
    y += DH_SET_CHOICE_H + DH_SET_ROW_GAP;
    vis[9] = 1;
    row_y[9] = y;
}

static void seg_pill_rect(int row_x, int row_y, int row_w, int *px, int *py, int *pw, int *ph)
{
    *ph = DH_SEG_H;
    *pw = DH_SET_PILL_W;
    if (*pw > row_w - DH_SEG_INSET * 2) {
        *pw = row_w - DH_SEG_INSET * 2;
    }
    *px = row_x + row_w - DH_SEG_INSET - *pw;
    *py = row_y + (DH_SET_CHOICE_H - DH_SEG_H) / 2;
}

static int *sys_sfx_field(DhUi *ui, int id)
{
    if (id == RID_AUD_MASTER) {
        return &ui->cfg.sfx_master;
    }
    if (id == RID_AUD_NAV) {
        return &ui->cfg.sfx_nav;
    }
    if (id == RID_AUD_LAUNCH) {
        return &ui->cfg.sfx_launch;
    }
    if (id == RID_AUD_STARTUP) {
        return &ui->cfg.sfx_startup;
    }
    return NULL;
}

static void sys_set_sfx(DhUi *ui, int id, int dir)
{
    int *p = sys_sfx_field(ui, id);
    if (!p) {
        return;
    }
    int next = *p;
    if (dir == 0) {
        next = !*p;
    } else if (dir < 0) {
        next = 0;
    } else {
        next = 1;
    }
    if (dir < 0 && *p == 0) {
        ui->opt_nav = 1;
        return;
    }
    if (*p == next) {
        return;
    }
    *p = next;
    dh_settings_save(&ui->cfg);
    dh_sfx_nav();
}

static void opt_nudge(DhUi *ui, int dir)
{
    if (ui->opt_cat == SETCAT_COVERS && ui->opt_row == COVERS_ROW_PRIVACY) {
        int next = ui->cfg.privacy_covers;
        if (dir == 0) {
            next = !next;
        } else if (dir < 0) {
            next = 0;
        } else {
            next = 1;
        }
        if (dir < 0 && ui->cfg.privacy_covers == 0) {
            ui->opt_nav = 1;
            return;
        }
        if (next == ui->cfg.privacy_covers) {
            return;
        }
        ui->cfg.privacy_covers = next;
        dh_settings_save(&ui->cfg);
        dh_sfx_nav();
        return;
    }
    if (ui->opt_cat == SETCAT_COVERS && ui->opt_row == COVERS_ROW_SRC) {
        int next = ui->cfg.cover_src + dir;
        if (next < 0) {
            ui->opt_nav = 1;
            return;
        }
        if (next > 1) {
            return;
        }
        covers_set_src(ui, next);
        dh_sfx_nav();
        return;
    }
    if (ui->opt_cat == SETCAT_COVERS) {
        if (dir < 0) {
            ui->opt_nav = 1;
        }
        return;
    }
    if (ui->opt_cat != SETCAT_LIBRARY) {
        set_nudge(ui, dir);
        return;
    }
    int cur = lib_value(ui, ui->opt_row);
    int n = lib_seg_count(ui->opt_row);
    int next = cur + dir;
    if (next < 0) {
        ui->opt_nav = 1;
        return;
    }
    if (next >= n) {
        return;
    }
    lib_apply(ui, ui->opt_row, next);
    dh_sfx_nav();
}

static int covers_row_enabled(const DhUi *ui, int row)
{
    if (row == COVERS_ROW_PRIVACY) {
        return 1;
    }
    if (row == COVERS_ROW_SRC) {
        return 1;
    }
    if (covers_udeck(ui)) {
        return row == COVERS_ROW_SETUP || row == COVERS_ROW_RESET;
    }
    if (row == COVERS_ROW_EASY || row == COVERS_ROW_KEY || row == COVERS_ROW_FILE || row == COVERS_ROW_TEST) {
        return 1;
    }
    if (row == COVERS_ROW_SETUP || row == COVERS_ROW_RESET) {
        return sgdb_key_ready(ui);
    }
    return 0;
}

static int covers_row_h(int row)
{
    return row == COVERS_ROW_SRC ? DH_SET_CHOICE_H : DH_SET_ROW_H;
}

static int covers_hint_h(const DhUi *ui)
{
    (void)ui;
    return 56;
}

static int covers_row_count(const DhUi *ui)
{
    (void)ui;
    return COVERS_ROW_N;
}

static void covers_set_src(DhUi *ui, int src)
{
    if (src < 0) {
        src = 0;
    }
    if (src > 1) {
        src = 1;
    }
    if (ui->cfg.cover_src == src) {
        return;
    }
    ui->cfg.cover_src = src;
    dh_settings_save(&ui->cfg);
    dh_sgdb_apply(&ui->cfg);
    if (!covers_row_enabled(ui, ui->opt_row)) {
        ui->opt_row = COVERS_ROW_SRC;
    }
}

static void covers_activate(DhUi *ui, int row)
{
    if (row == COVERS_ROW_SRC) {
        covers_set_src(ui, ui->cfg.cover_src ? 0 : 1);
    } else if (row == COVERS_ROW_EASY) {
        open_easyapi_code(ui);
    } else if (row == COVERS_ROW_KEY) {
        open_sgdb_key(ui);
    } else if (row == COVERS_ROW_FILE) {
        open_sgdb_key_file(ui);
    } else if (row == COVERS_ROW_TEST) {
        start_sgdb_test(ui);
    } else if (row == COVERS_ROW_SETUP) {
        start_sgdb_download(ui);
    } else if (row == COVERS_ROW_RESET) {
        dh_sgdb_reset_all();
        toast(ui, "All custom covers removed");
    } else if (row == COVERS_ROW_PRIVACY) {
        ui->cfg.privacy_covers = !ui->cfg.privacy_covers;
        dh_settings_save(&ui->cfg);
        dh_sfx_nav();
    }
}

typedef struct {
    int kind;
    int id;
    int extra;
    const char *title;
    const char *sub;
    char value[96];
    int on;
} DhSetRow;

enum { SET_ROWS_MAX = 40 };

static DhSetRow g_set_rows[SET_ROWS_MAX];
static int g_set_n;
#ifdef DH_ULAUNCH
static char g_bt_names[DH_SYS_BT_MAX][64];
#endif

static int set_kind_h(int kind)
{
    if (kind == RK_HEAD) {
        return DH_SET_HEAD_H;
    }
    if (kind == RK_SEG) {
        return DH_SET_CHOICE_H;
    }
    return DH_SET_ROW_H;
}

static void set_add(int kind, int id, const char *title, const char *sub, const char *value, int on)
{
    if (g_set_n >= SET_ROWS_MAX) {
        return;
    }
    DhSetRow *r = &g_set_rows[g_set_n++];
    r->kind = kind;
    r->id = id;
    r->extra = 0;
    r->title = title;
    r->sub = sub;
    r->on = on;
    r->value[0] = 0;
    if (value) {
        snprintf(r->value, sizeof(r->value), "%s", value);
    }
}

#ifdef DH_ULAUNCH
static void set_add_sys_str(int kind, int id, const char *title, const char *sub, int str_id)
{
    char buf[96];
    buf[0] = 0;
    deck_sys_copy(str_id, buf, sizeof(buf));
    set_add(kind, id, title, sub, buf, 0);
}
#endif

static const char *title_name_opt(u64 app_id)
{
    int n = dh_titles_count();
    for (int i = 0; i < n; i++) {
        DhTitle *t = dh_titles_get(i);
        if (t && t->application_id == app_id && t->name[0]) {
            return t->name;
        }
    }
    return NULL;
}

static int set_build_rows(DhUi *ui)
{
    g_set_n = 0;
    if (ui->opt_cat == SETCAT_AUDIO) {
        set_add(RK_SWITCH, RID_AUD_MASTER, "All sounds", "Mute or unmute navigation, launch and startup", NULL, ui->cfg.sfx_master);
        set_add(RK_SWITCH, RID_AUD_NAV, "Navigation", "Cursor moves, tabs, settings fields", NULL, ui->cfg.sfx_nav);
        set_add(RK_SWITCH, RID_AUD_LAUNCH, "Game launch", "Plays when starting a game or homebrew", NULL, ui->cfg.sfx_launch);
        set_add(RK_SWITCH, RID_AUD_STARTUP, "Startup", "Plays after picking a user", NULL, ui->cfg.sfx_startup);
        return g_set_n;
    }
#ifdef DH_ULAUNCH
    if (ui->opt_cat == SETCAT_DISPLAY) {
        set_add(RK_SWITCH, RID_LOCKSCREEN, "Lockscreen", "Show lockscreen when waking from sleep", NULL, deck_sys_bool(DH_SYS_LOCKSCREEN));
        set_add(RK_SEG, RID_ALBUM, "Album storage", NULL, NULL, deck_sys_album_get());
        return g_set_n;
    }
    if (ui->opt_cat == SETCAT_POWER) {
        set_add_sys_str(RK_CYCLE, RID_SLEEP_HH, "Handheld sleep", "L / R to change", DH_SYS_STR_SLEEP_HH);
        set_add_sys_str(RK_CYCLE, RID_SLEEP_DOCK, "Dock sleep", "L / R to change", DH_SYS_STR_SLEEP_DOCK);
        set_add(RK_SWITCH, RID_SLEEP_MEDIA, "Sleep during media", "Allow sleep while video or music is playing", NULL, deck_sys_bool(DH_SYS_SLEEP_MEDIA));
        set_add(RK_SWITCH, RID_SLEEP_WAKE, "Wake on power change", "Wake when a charger is connected or removed", NULL, deck_sys_bool(DH_SYS_SLEEP_WAKE));
        return g_set_n;
    }
    if (ui->opt_cat == SETCAT_INTERNET) {
        set_add_sys_str(RK_ACTION, RID_WIFI, "Wi-Fi", "A: open Nintendo network settings", DH_SYS_STR_SSID);
        set_add(RK_SWITCH, RID_WLAN, "Wireless LAN", NULL, NULL, deck_sys_bool(DH_SYS_WLAN));
        set_add_sys_str(RK_INFO, RID_IP, "IP address", NULL, DH_SYS_STR_IP);
        set_add_sys_str(RK_INFO, RID_MAC, "MAC address", NULL, DH_SYS_STR_MAC);
        return g_set_n;
    }
    if (ui->opt_cat == SETCAT_BLUETOOTH) {
        DhSysBtDev devs[DH_SYS_BT_MAX];
        int dn;
        if (ui->bt_discover) {
            set_add(RK_HEAD, RID_BT_FIND, "Nearby devices", "B: stop searching", NULL, 0);
            dn = deck_sys_bt_list(1, devs, DH_SYS_BT_MAX);
            if (dn < 1) {
                set_add(RK_INFO, RID_BT_FIND, "Searching…", "Keep the accessory in pairing mode", NULL, 0);
            }
            for (int i = 0; i < dn; i++) {
                snprintf(g_bt_names[i], sizeof(g_bt_names[i]), "%s", devs[i].name);
                DhSetRow *r;
                set_add(RK_ACTION, RID_BT_DEV, g_bt_names[i], devs[i].connected ? "Connected" : "Tap to connect", NULL, 0);
                r = &g_set_rows[g_set_n - 1];
                r->extra = i;
            }
            return g_set_n;
        }
        set_add(RK_SWITCH, RID_BT_EN, "Bluetooth", NULL, NULL, deck_sys_bool(DH_SYS_BT));
        set_add(RK_ACTION, RID_BT_FIND, "Find devices", "A: search for nearby audio accessories", "Search", 0);
        dn = deck_sys_bt_list(0, devs, DH_SYS_BT_MAX);
        if (dn < 1) {
            set_add(RK_INFO, RID_BT_DEV, "No paired devices", NULL, NULL, 0);
        }
        for (int i = 0; i < dn; i++) {
            snprintf(g_bt_names[i], sizeof(g_bt_names[i]), "%s", devs[i].name);
            set_add(RK_ACTION, RID_BT_DEV, g_bt_names[i],
                devs[i].connected ? "Connected" : "Paired", NULL, 0);
            g_set_rows[g_set_n - 1].extra = i;
        }
        return g_set_n;
    }
#endif
    if (ui->opt_cat == SETCAT_SYSTEM) {
#ifdef DH_ULAUNCH
        char nick[96];
        char take[96];
        nick[0] = take[0] = 0;
        deck_sys_copy(DH_SYS_STR_NICK, nick, sizeof(nick));
        deck_sys_copy(DH_SYS_STR_TAKEOVER, take, sizeof(take));
        {
            u64 tid = deck_sys_takeover_id();
            const char *nm = tid ? (ui->cfg.privacy_covers ? privacy_label() : title_name_opt(tid)) : NULL;
            if (nm) {
                snprintf(take, sizeof(take), "%s", nm);
            }
        }
        set_add(RK_ACTION, RID_NICK, "Nickname", "A: rename this console", nick, 0);
        set_add(RK_CYCLE, RID_LANG, dh_lang("set_ui_lang"), dh_lang("set_ui_lang_sub"),
            dh_lang_name(dh_lang_index(ui->cfg.lang)), 0);
        set_add(RK_SWITCH, RID_USB30, "USB 3.0", NULL, NULL, deck_sys_bool(DH_SYS_USB30));
        set_add(RK_SWITCH, RID_NFC, "NFC", NULL, NULL, deck_sys_bool(DH_SYS_NFC));
        set_add(RK_SWITCH, RID_CAPTURE, "USB capture", "Reboot required after changing", NULL, deck_sys_bool(DH_SYS_CAPTURE));
        set_add(RK_SWITCH, RID_HB_APP, "Launch homebrew as app", "Open NROs through the takeover title", NULL, deck_sys_bool(DH_SYS_HB_APP));
        set_add(RK_ACTION, RID_TAKEOVER, "Homebrew takeover", "A: reset to none", take, 0);
        set_add(RK_ACTION, RID_DONOR, "Set takeover from game", "A: use the selected game", "Set", 0);
        set_add(RK_HEAD, RID_ABOUT_HEAD, "About", NULL, NULL, 0);
        set_add_sys_str(RK_INFO, RID_VER, "Version", NULL, DH_SYS_STR_VERSION);
#else
        set_add(RK_INFO, RID_VER, "Version", NULL, APP_VERSION, 0);
#endif
        {
            char games[32];
            char hb[32];
            snprintf(games, sizeof(games), "%d", dh_titles_count());
            snprintf(hb, sizeof(hb), "%d", dh_hb_count());
            set_add(RK_INFO, RID_GAMES, "Games", NULL, games, 0);
            set_add(RK_INFO, RID_HB, "Homebrew", NULL, hb, 0);
        }
#ifndef DH_ULAUNCH
        {
            char donor[80];
            snprintf(donor, sizeof(donor), "Auto");
#ifdef DH_MENU_APPLET
            u64 did = dh_nro_read_donor();
            if (did) {
                const char *nm = ui->cfg.privacy_covers ? privacy_label() : title_name_opt(did);
                snprintf(donor, sizeof(donor), "%s", nm ? nm : "set");
            }
#endif
            set_add(RK_ACTION, RID_DONOR, "Homebrew donor", "A: use selected game", donor, 0);
        }
#else
        set_add_sys_str(RK_INFO, RID_FW, "Firmware", NULL, DH_SYS_STR_FW);
        set_add_sys_str(RK_INFO, RID_AMS, "Atmosphere", NULL, DH_SYS_STR_AMS);
        set_add_sys_str(RK_INFO, RID_EMUMMC, "emuMMC", NULL, DH_SYS_STR_EMUMMC);
        set_add_sys_str(RK_INFO, RID_REGION, "Region", NULL, DH_SYS_STR_REGION);
        set_add_sys_str(RK_INFO, RID_TZ, "Timezone", NULL, DH_SYS_STR_TZ);
        set_add_sys_str(RK_INFO, RID_SERIAL, "Serial", NULL, DH_SYS_STR_SERIAL);
        set_add_sys_str(RK_INFO, RID_BATTERY, "Battery lot", NULL, DH_SYS_STR_BATTERY);
        set_add_sys_str(RK_INFO, RID_AUDSVC, "Audio service", NULL, DH_SYS_STR_AUDSVC);
#endif
        return g_set_n;
    }
    return g_set_n;
}

static const DhSetRow *set_row_at(DhUi *ui, int row)
{
    set_build_rows(ui);
    if (row < 0 || row >= g_set_n) {
        return NULL;
    }
    return &g_set_rows[row];
}

static int set_uses_table(const DhUi *ui)
{
    return ui->opt_cat != SETCAT_LIBRARY && ui->opt_cat != SETCAT_COVERS;
}

static void set_stop_bt(DhUi *ui)
{
#ifdef DH_ULAUNCH
    if (ui->bt_discover) {
        deck_sys_bt_discover_stop();
        ui->bt_discover = 0;
    }
#else
    (void)ui;
#endif
}

static void open_settings(DhUi *ui)
{
    ui->menu = 0;
    ui->options = 1;
    ui->opt_cat = SETCAT_LIBRARY;
    ui->opt_row = 0;
    ui->opt_nav = 1;
    ui->opt_scroll = 0;
    set_stop_bt(ui);
}

#ifdef DH_ULAUNCH
static void set_apply_bool(DhUi *ui, int sys_id)
{
    int rc = deck_sys_bool_toggle(sys_id);
    if (rc == 2) {
        toast(ui, "Reboot to apply");
        dh_sfx_nav();
    } else if (rc) {
        dh_sfx_nav();
    }
}
#endif

static void set_nudge(DhUi *ui, int dir)
{
    const DhSetRow *row = set_row_at(ui, ui->opt_row);
    if (!row) {
        if (dir < 0) {
            ui->opt_nav = 1;
        }
        return;
    }
    if (row->kind == RK_SWITCH) {
        if (row->id >= RID_AUD_MASTER && row->id <= RID_AUD_STARTUP) {
            sys_set_sfx(ui, row->id, dir);
            return;
        }
#ifdef DH_ULAUNCH
        if (dir < 0 && !row->on) {
            ui->opt_nav = 1;
            return;
        }
        if (dir < 0 && row->on) {
            deck_sys_bool_set(
                row->id == RID_LOCKSCREEN ? DH_SYS_LOCKSCREEN :
                row->id == RID_WLAN ? DH_SYS_WLAN :
                row->id == RID_BT_EN ? DH_SYS_BT :
                row->id == RID_NFC ? DH_SYS_NFC :
                row->id == RID_USB30 ? DH_SYS_USB30 :
                row->id == RID_CAPTURE ? DH_SYS_CAPTURE :
                row->id == RID_HB_APP ? DH_SYS_HB_APP :
                row->id == RID_SLEEP_MEDIA ? DH_SYS_SLEEP_MEDIA :
                DH_SYS_SLEEP_WAKE, 0);
            dh_sfx_nav();
            return;
        }
        if (dir > 0 && !row->on) {
            int sys_id =
                row->id == RID_LOCKSCREEN ? DH_SYS_LOCKSCREEN :
                row->id == RID_WLAN ? DH_SYS_WLAN :
                row->id == RID_BT_EN ? DH_SYS_BT :
                row->id == RID_NFC ? DH_SYS_NFC :
                row->id == RID_USB30 ? DH_SYS_USB30 :
                row->id == RID_CAPTURE ? DH_SYS_CAPTURE :
                row->id == RID_HB_APP ? DH_SYS_HB_APP :
                row->id == RID_SLEEP_MEDIA ? DH_SYS_SLEEP_MEDIA :
                DH_SYS_SLEEP_WAKE;
            int rc = deck_sys_bool_set(sys_id, 1);
            if (rc == 2) {
                toast(ui, "Reboot to apply");
            }
            dh_sfx_nav();
            return;
        }
#endif
        if (dir < 0) {
            ui->opt_nav = 1;
        }
        return;
    }
#ifdef DH_ULAUNCH
    if (row->kind == RK_SEG && row->id == RID_ALBUM) {
        int next = deck_sys_album_get() + dir;
        if (next < 0) {
            ui->opt_nav = 1;
            return;
        }
        if (next > 1) {
            return;
        }
        deck_sys_album_set(next);
        dh_sfx_nav();
        return;
    }
    if (row->kind == RK_CYCLE) {
        if (row->id == RID_LANG) {
            int cur = dh_lang_index(ui->cfg.lang);
            int n = dh_lang_count();
            int next = cur + dir;
            if (next < 0) {
                ui->opt_nav = 1;
                return;
            }
            if (next >= n) {
                return;
            }
            dh_lang_code(next, ui->cfg.lang, sizeof(ui->cfg.lang));
            dh_lang_apply(ui->cfg.lang);
            dh_settings_save(&ui->cfg);
            dh_sfx_nav();
            return;
        }
        int dock = (row->id == RID_SLEEP_DOCK);
        int next = deck_sys_sleep_get(dock) + dir;
        if (next < 0) {
            ui->opt_nav = 1;
            return;
        }
        if (next > 5) {
            return;
        }
        deck_sys_sleep_set(dock, next);
        dh_sfx_nav();
        return;
    }
#endif
    if (dir < 0) {
        ui->opt_nav = 1;
    }
}

static void set_activate(DhUi *ui)
{
    const DhSetRow *row = set_row_at(ui, ui->opt_row);
    if (!row || row->kind == RK_HEAD || row->kind == RK_INFO) {
        return;
    }
    if (row->kind == RK_SWITCH) {
        if (row->id >= RID_AUD_MASTER && row->id <= RID_AUD_STARTUP) {
            sys_set_sfx(ui, row->id, 0);
            return;
        }
#ifdef DH_ULAUNCH
        int sys_id =
            row->id == RID_LOCKSCREEN ? DH_SYS_LOCKSCREEN :
            row->id == RID_WLAN ? DH_SYS_WLAN :
            row->id == RID_BT_EN ? DH_SYS_BT :
            row->id == RID_NFC ? DH_SYS_NFC :
            row->id == RID_USB30 ? DH_SYS_USB30 :
            row->id == RID_CAPTURE ? DH_SYS_CAPTURE :
            row->id == RID_HB_APP ? DH_SYS_HB_APP :
            row->id == RID_SLEEP_MEDIA ? DH_SYS_SLEEP_MEDIA :
            DH_SYS_SLEEP_WAKE;
        set_apply_bool(ui, sys_id);
#endif
        return;
    }
#ifdef DH_ULAUNCH
    if (row->kind == RK_SEG && row->id == RID_ALBUM) {
        deck_sys_album_set(deck_sys_album_get() ? 0 : 1);
        dh_sfx_nav();
        return;
    }
    if (row->kind == RK_CYCLE) {
        if (row->id == RID_LANG) {
            int cur = dh_lang_index(ui->cfg.lang);
            int n = dh_lang_count();
            int next = (cur + 1) % n;
            dh_lang_code(next, ui->cfg.lang, sizeof(ui->cfg.lang));
            dh_lang_apply(ui->cfg.lang);
            dh_settings_save(&ui->cfg);
            dh_sfx_nav();
            return;
        }
        int dock = (row->id == RID_SLEEP_DOCK);
        int next = (deck_sys_sleep_get(dock) + 1) % 6;
        deck_sys_sleep_set(dock, next);
        dh_sfx_nav();
        return;
    }
    if (row->id == RID_WIFI) {
        deck_sys_open_wifi();
        return;
    }
    if (row->id == RID_NICK) {
        deck_sys_edit_nick();
        return;
    }
    if (row->id == RID_TAKEOVER) {
        if (deck_sys_reset_takeover()) {
            toast(ui, "Homebrew takeover reset");
            dh_sfx_nav();
        }
        return;
    }
    if (row->id == RID_DONOR) {
        if (ui->tab == TAB_GAMES) {
            int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);
            DhTitle *t = dh_titles_get(idx);
            if (t && t->application_id) {
                if (deck_sys_set_takeover(t->application_id) == 1) {
                    char buf[96];
                    snprintf(buf, sizeof(buf), "Takeover: %s", ui->cfg.privacy_covers ? privacy_label() : (t->name[0] ? t->name : t->id_hex));
                    toast(ui, buf);
                    dh_sfx_nav();
                    return;
                }
            }
        }
        toast(ui, "Select a game first");
        return;
    }
    if (row->id == RID_BT_FIND && !ui->bt_discover) {
        ui->bt_discover = 1;
        ui->opt_row = 0;
        ui->opt_scroll = 0;
        deck_sys_bt_discover_start();
        if (!opt_row_enabled(ui, ui->opt_row)) {
            opt_move_row(ui, 1);
        } else {
            dh_sfx_nav();
        }
        return;
    }
    if (row->id == RID_BT_DEV) {
        if (deck_sys_bt_pick(ui->bt_discover, row->extra) != 0) {
            return;
        }
        ui->overlay = OV_BT;
        ui->overlay_item = 0;
        ui->overlay_btn = 0;
        snprintf(ui->confirm_name, sizeof(ui->confirm_name), "%s", row->title ? row->title : "Device");
        dh_sfx_nav();
        return;
    }
#else
    if (row->id == RID_DONOR) {
#ifdef DH_MENU_APPLET
        set_donor_from_selection(ui);
#endif
        return;
    }
#endif
}

static int opt_row_enabled(const DhUi *ui, int row)
{
    if (ui->opt_cat == SETCAT_LIBRARY) {
        return !lib_row_hidden(ui, row);
    }
    if (ui->opt_cat == SETCAT_COVERS) {
        return covers_row_enabled(ui, row);
    }
    set_build_rows((DhUi *)ui);
    if (row < 0 || row >= g_set_n) {
        return 0;
    }
    return g_set_rows[row].kind != RK_HEAD;
}

static int opt_row_count(const DhUi *ui)
{
    if (ui->opt_cat == SETCAT_LIBRARY) {
        return LIB_ROW_N;
    }
    if (ui->opt_cat == SETCAT_COVERS) {
        return covers_row_count(ui);
    }
    set_build_rows((DhUi *)ui);
    return g_set_n;
}

static void opt_ensure_visible(DhUi *ui)
{
    if (!set_uses_table(ui) && ui->opt_cat != SETCAT_COVERS && ui->opt_cat != SETCAT_LIBRARY) {
        return;
    }
    if (!set_uses_table(ui)) {
        return;
    }
    set_build_rows(ui);
    int body_y = options_body_y();
    int view_h = DH_FOOTER_Y - 8 - body_y;
    int y = 0;
    int row_y = 0;
    int row_h = DH_SET_ROW_H;
    int total = 0;
    for (int i = 0; i < g_set_n; i++) {
        int h = set_kind_h(g_set_rows[i].kind);
        if (i == ui->opt_row) {
            row_y = y;
            row_h = h;
        }
        y += h + DH_SET_ROW_GAP;
        total = y;
    }
    int max_s = total - view_h;
    if (max_s < 0) {
        max_s = 0;
    }
    if (row_y < ui->opt_scroll) {
        ui->opt_scroll = row_y;
    }
    if (row_y + row_h > ui->opt_scroll + view_h) {
        ui->opt_scroll = row_y + row_h - view_h;
    }
    if (ui->opt_scroll < 0) {
        ui->opt_scroll = 0;
    }
    if (ui->opt_scroll > max_s) {
        ui->opt_scroll = max_s;
    }
}

static void opt_move_row(DhUi *ui, int dir)
{
    int n = opt_row_count(ui);
    if (n < 1) {
        return;
    }
    int old = ui->opt_row;
    for (int k = 0; k < n; k++) {
        ui->opt_row = (ui->opt_row + dir + n) % n;
        if (opt_row_enabled(ui, ui->opt_row)) {
            if (ui->opt_row != old) {
                dh_sfx_nav();
            }
            opt_ensure_visible(ui);
            return;
        }
    }
}

static u64 lstick_as_dpad(u64 keys)
{
    if (keys & HidNpadButton_StickLLeft) {
        keys |= HidNpadButton_Left;
    }
    if (keys & HidNpadButton_StickLRight) {
        keys |= HidNpadButton_Right;
    }
    if (keys & HidNpadButton_StickLUp) {
        keys |= HidNpadButton_Up;
    }
    if (keys & HidNpadButton_StickLDown) {
        keys |= HidNpadButton_Down;
    }
    return keys;
}

static void axis_from(u64 keys, u64 left, u64 right, u64 up, u64 down, int *dx, int *dy)
{
    *dx = 0;
    *dy = 0;
    if (keys & left) {
        *dx -= 1;
    }
    if (keys & right) {
        *dx += 1;
    }
    if (keys & up) {
        *dy -= 1;
    }
    if (keys & down) {
        *dy += 1;
    }
}

static int analog_dir(s32 v)
{
    const s32 dead = 20000;
    if (v > dead) {
        return 1;
    }
    if (v < -dead) {
        return -1;
    }
    return 0;
}

static void analog_sticks(int *ldx, int *ldy, int *rdx, int *rdy, s32 *lax, s32 *lay, s32 *rax, s32 *ray)
{
    static PadState pad;
    static int ok;
    if (!ok) {
        padInitializeDefault(&pad);
        ok = 1;
    }
    padUpdate(&pad);
    HidAnalogStickState l = padGetStickPos(&pad, 0);
    HidAnalogStickState r = padGetStickPos(&pad, 1);
    *ldx = analog_dir(l.x);
    *ldy = -analog_dir(l.y);
    *rdx = analog_dir(r.x);
    *rdy = -analog_dir(r.y);
    *lax = l.x;
    *lay = l.y;
    *rax = r.x;
    *ray = r.y;
}

static void flatten_cardinal(int *dx, int *dy, s32 ax, s32 ay)
{
    if (!*dx || !*dy) {
        return;
    }
    s32 mx = ax < 0 ? -ax : ax;
    s32 my = ay < 0 ? -ay : ay;
    if (mx >= my) {
        *dy = 0;
    } else {
        *dx = 0;
    }
}

static int nav_pulse(u32 *stamp, int *ldx, int *ldy, int *rep, int dx, int dy, u32 now)
{
    if (!dx && !dy) {
        *stamp = 0;
        *ldx = 0;
        *ldy = 0;
        *rep = 0;
        return 0;
    }
    if (dx != *ldx || dy != *ldy || *stamp == 0) {
        *ldx = dx;
        *ldy = dy;
        *stamp = now;
        *rep = 0;
        return 1;
    }
    u32 wait = *rep ? 90u : 280u;
    if (now - *stamp >= wait) {
        *stamp = now;
        *rep = 1;
        return 1;
    }
    return 0;
}

static void swap_vis(DhUi *ui, int a, int b)
{
    int ra = vis_real(ui, ui->tab, a);
    int rb = vis_real(ui, ui->tab, b);
    if (ui->tab == TAB_HB) {
        dh_hb_swap(ra, rb);
        dh_hb_save_custom();
    } else {
        dh_titles_swap(ra, rb);
        dh_titles_save_custom();
    }
}

static void grid_apply_move(DhUi *ui, int dx, int dy, int swap)
{
    int n = tab_len(ui);
    if (n < 1 || (!dx && !dy)) {
        return;
    }
    int cols = grid_cols(ui);
    int i = ui->item[ui->tab];
    int col = i % cols;
    int row = i / cols;
    int ncol = col + dx;
    int nrow = row + dy;
    int ni = nrow * cols + ncol;
    int row_end = (dx != 0) && (ncol < 0 || ncol >= cols || ni < 0 || ni >= n);

    if (row_end && !swap) {
        if (ui->cfg.edge == DH_EDGE_CATEGORY && !ui->query[0] && dy == 0) {
            set_tab(ui, (ui->tab + (dx > 0 ? 1 : TAB_COUNT - 1)) % TAB_COUNT);
            int tn = tab_len(ui);
            int tc = grid_cols(ui);
            if (tn < 1) {
                return;
            }
            if (dx > 0) {
                int dest = row * tc;
                ui->item[ui->tab] = dest < tn ? dest : 0;
            } else {
                int dest = row * tc + (tc - 1);
                if (dest >= tn) {
                    dest = tn - 1;
                }
                ui->item[ui->tab] = dest;
            }
            return;
        }
        if (ui->cfg.edge == DH_EDGE_WRAP && dy == 0) {
            ncol = dx > 0 ? 0 : cols - 1;
            ni = row * cols + ncol;
            if (ni >= n) {
                ni = n - 1;
            }
            if (ni < 0) {
                ni = 0;
            }
            ui->item[ui->tab] = ni;
            return;
        }
        ni = i + dx;
        if (ni < 0) {
            ni = 0;
        }
        if (ni >= n) {
            ni = n - 1;
        }
        if (swap && ni != i) {
            swap_vis(ui, i, ni);
        }
        ui->item[ui->tab] = ni;
        return;
    }

    if (ncol < 0 || ncol >= cols || ni < 0 || ni >= n) {
        return;
    }
    if (swap && ni != i) {
        swap_vis(ui, i, ni);
    }
    ui->item[ui->tab] = ni;
}

static void grid_nav_tick(DhUi *ui, u64 keys_down, u64 keys_held)
{
    u64 keys = keys_down | keys_held;
    int dx = 0;
    int dy = 0;
    axis_from(keys, HidNpadButton_Left | HidNpadButton_StickLLeft,
        HidNpadButton_Right | HidNpadButton_StickLRight,
        HidNpadButton_Up | HidNpadButton_StickLUp,
        HidNpadButton_Down | HidNpadButton_StickLDown, &dx, &dy);
    int rdx = 0;
    int rdy = 0;
    axis_from(keys, HidNpadButton_StickRLeft, HidNpadButton_StickRRight,
        HidNpadButton_StickRUp, HidNpadButton_StickRDown, &rdx, &rdy);
    int adx = 0;
    int ady = 0;
    int arx = 0;
    int ary = 0;
    s32 lax = 0;
    s32 lay = 0;
    s32 rax = 0;
    s32 ray = 0;
    analog_sticks(&adx, &ady, &arx, &ary, &lax, &lay, &rax, &ray);
    if (adx) {
        dx = adx;
    }
    if (ady) {
        dy = ady;
    }
    if (arx) {
        rdx = arx;
    }
    if (ary) {
        rdy = ary;
    }
    if (!ui->cfg.stick_diag) {
        flatten_cardinal(&dx, &dy, lax, lay);
        flatten_cardinal(&rdx, &rdy, rax, ray);
    }

    u32 now = SDL_GetTicks();
    int old_tab = ui->tab;
    int old_item = ui->item[ui->tab];
    if (nav_pulse(&ui->nav_ms, &ui->nav_dx, &ui->nav_dy, &ui->nav_rep, dx, dy, now)) {
        grid_apply_move(ui, dx, dy, 0);
    }
    if (ui->sort_edit && nav_pulse(&ui->rnav_ms, &ui->rnav_dx, &ui->rnav_dy, &ui->rnav_rep, rdx, rdy, now)) {
        grid_apply_move(ui, rdx, rdy, 1);
    }
    if (ui->tab == old_tab && ui->item[ui->tab] != old_item) {
        dh_sfx_nav();
    }
}

void dh_ui_handle_down(DhUi *ui, u64 keys)
{
    if (ui->overlay == OV_MTP) {
        if (keys & (HidNpadButton_B | HidNpadButton_X | HidNpadButton_Minus)) {
            close_overlay(ui);
        }
        return;
    }
    if (ui->overlay == OV_SGDB) {
        commit_easyapi_import(ui);
        DhSgdbStatus st;
        dh_sgdb_status(&st);
        if (strstr(st.phase, "Key works")) {
            ui->cfg.sgdb_key_ok = 1;
            dh_settings_save(&ui->cfg);
        }
        if (keys & (HidNpadButton_B | HidNpadButton_A | HidNpadButton_X)) {
            ui->overlay = OV_NONE;
        }
        return;
    }
    if (ui->overlay == OV_SGDB_FMT || ui->overlay == OV_SGDB_SKIP) {
        int old_btn = ui->overlay_btn;
        if (keys & HidNpadButton_Left) {
            ui->overlay_btn = 0;
        }
        if (keys & HidNpadButton_Right) {
            ui->overlay_btn = 1;
        }
        if (ui->overlay_btn != old_btn) {
            dh_sfx_nav();
        }
        if (keys & HidNpadButton_B) {
            close_overlay(ui);
            return;
        }
        if (keys & HidNpadButton_A) {
            if (ui->overlay == OV_SGDB_FMT) {
                ui->cfg.games.format = ui->overlay_btn == 0 ? DH_FMT_SQUARE : DH_FMT_CAPSULE;
                dh_settings_save(&ui->cfg);
                dh_sgdb_apply(&ui->cfg);
                ui->overlay = OV_SGDB_SKIP;
                ui->overlay_btn = 0;
                return;
            }
            ui->setup_skip = ui->overlay_btn == 0;
            if (covers_udeck(ui)) {
                ui->setup_on = 0;
                dh_sgdb_apply(&ui->cfg);
                dh_sgdb_enqueue_all(ui->setup_skip);
                open_sgdb_progress(ui);
                return;
            }
            setup_begin_queue(ui);
        }
        return;
    }
    if (ui->overlay == OV_SGDB_PICK) {
        DhSgdbStatus st;
        dh_sgdb_status(&st);
        if (keys & HidNpadButton_B) {
            if (ui->setup_on) {
                close_overlay(ui);
            } else {
                dh_sgdb_pick_clear();
                ui->overlay = OV_ITEM;
                ui->overlay_item = 0;
            }
            return;
        }
        if (ui->setup_on && (keys & HidNpadButton_Y)) {
            setup_next(ui);
            return;
        }
        if (st.pick_ready && st.pick_n > 0) {
            int dx = 0, dy = 0;
            if (keys & HidNpadButton_Left) {
                dx = -1;
            }
            if (keys & HidNpadButton_Right) {
                dx = 1;
            }
            if (keys & HidNpadButton_Up) {
                dy = -1;
            }
            if (keys & HidNpadButton_Down) {
                dy = 1;
            }
            if (dx || dy) {
                int old_pick = ui->overlay_item;
                int cols = PICK_COLS;
                int rows = (st.pick_n + cols - 1) / cols;
                int r = ui->overlay_item / cols;
                int c = ui->overlay_item % cols;
                r += dy;
                c += dx;
                if (r < 0) {
                    r = rows - 1;
                }
                if (r >= rows) {
                    r = 0;
                }
                if (c < 0) {
                    c = cols - 1;
                }
                if (c >= cols) {
                    c = 0;
                }
                int ni = r * cols + c;
                if (ni < 0) {
                    ni = 0;
                }
                if (ni >= st.pick_n) {
                    ni = st.pick_n - 1;
                }
                ui->overlay_item = ni;
                if (ui->overlay_item != old_pick) {
                    dh_sfx_nav();
                }
            }
            if (keys & HidNpadButton_A) {
                DhTitle *t = ui->setup_on ? setup_title(ui) : dh_titles_get(vis_real(ui, ui->tab, ui->item[ui->tab]));
                u64 app_id = t ? t->application_id : (ui->setup_on ? ui->setup_id : 0);
                if (app_id && dh_sgdb_pick_apply(ui->overlay_item, app_id) == 0) {
                    toast(ui, "Saving cover…");
                    if (ui->setup_on) {
                        setup_next(ui);
                    } else {
                        dh_sgdb_pick_clear();
                        ui->overlay = OV_NONE;
                    }
                }
            }
        }
        return;
    }
    if (ui->overlay == OV_CLOSE || ui->overlay == OV_UN1 || ui->overlay == OV_UN2) {
        int old_btn = ui->overlay_btn;
        if (keys & HidNpadButton_Left) {
            ui->overlay_btn = 0;
        }
        if (keys & HidNpadButton_Right) {
            ui->overlay_btn = 1;
        }
        if (ui->overlay_btn != old_btn) {
            dh_sfx_nav();
        }
        if (keys & HidNpadButton_A) {
            if (ui->overlay_btn == 0) {
                if (ui->overlay == OV_CLOSE) {
                    confirm_yes(ui);
                } else if (ui->overlay == OV_UN1) {
                    ui->overlay = OV_UN2;
                    ui->overlay_btn = 1;
                } else {
                    do_uninstall(ui);
                }
            } else {
                close_overlay(ui);
            }
        } else if (keys & HidNpadButton_B) {
            close_overlay(ui);
        }
        return;
    }
    if (ui->overlay == OV_INFO) {
        if (keys & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Y)) {
            ui->overlay = OV_ITEM;
        }
        return;
    }
    if (ui->overlay == OV_ITEM) {
        if (keys & (HidNpadButton_B | HidNpadButton_Y)) {
            close_overlay(ui);
            return;
        }
        int old_item = ui->overlay_item;
        if (keys & HidNpadButton_Up) {
            int n = item_menu_n(ui);
            ui->overlay_item = (ui->overlay_item + n - 1) % n;
        }
        if (keys & HidNpadButton_Down) {
            int n = item_menu_n(ui);
            ui->overlay_item = (ui->overlay_item + 1) % n;
        }
        if (ui->overlay_item != old_item) {
            dh_sfx_nav();
        }
        if (keys & HidNpadButton_A) {
            item_activate(ui);
        }
        return;
    }
    if (ui->overlay == OV_BT) {
#ifdef DH_ULAUNCH
        int n = ui->bt_discover ? 1 : 3;
        int old = ui->overlay_item;
        if (keys & HidNpadButton_Up) {
            ui->overlay_item = (ui->overlay_item + n - 1) % n;
        }
        if (keys & HidNpadButton_Down) {
            ui->overlay_item = (ui->overlay_item + 1) % n;
        }
        if (ui->overlay_item != old) {
            dh_sfx_nav();
        }
        if (keys & HidNpadButton_B) {
            close_overlay(ui);
            return;
        }
        if (keys & HidNpadButton_A) {
            int act = ui->bt_discover ? DH_SYS_BT_CONNECT : ui->overlay_item;
            int rc = deck_sys_bt_act(act);
            close_overlay(ui);
            if (rc == 0) {
                if (act == DH_SYS_BT_CONNECT) {
                    toast(ui, "Connecting…");
                } else if (act == DH_SYS_BT_DISCONNECT) {
                    toast(ui, "Disconnecting…");
                } else {
                    toast(ui, "Unpairing…");
                }
            } else {
                char err[80];
                snprintf(err, sizeof(err), "Bluetooth error 0x%x", (unsigned)rc);
                toast(ui, err);
            }
        }
#endif
        return;
    }
    if (keys & HidNpadButton_X) {
        if (ui->options) {
            set_stop_bt(ui);
            ui->options = 0;
            return;
        }
        ui->menu = !ui->menu;
        return;
    }
    if (ui->options) {
        if (keys & HidNpadButton_B) {
#ifdef DH_ULAUNCH
            if (!ui->opt_nav && ui->opt_cat == SETCAT_BLUETOOTH && ui->bt_discover) {
                set_stop_bt(ui);
                ui->opt_row = 0;
                ui->opt_scroll = 0;
                dh_sfx_nav();
                return;
            }
#endif
            if (!ui->opt_nav) {
                ui->opt_nav = 1;
            } else {
                set_stop_bt(ui);
                ui->options = 0;
            }
            return;
        }
        if (ui->opt_nav) {
            int old_cat = ui->opt_cat;
            if (keys & HidNpadButton_Up) {
                ui->opt_cat = (ui->opt_cat + SETCAT_COUNT - 1) % SETCAT_COUNT;
                ui->opt_row = 0;
                ui->opt_scroll = 0;
            }
            if (keys & HidNpadButton_Down) {
                ui->opt_cat = (ui->opt_cat + 1) % SETCAT_COUNT;
                ui->opt_row = 0;
                ui->opt_scroll = 0;
            }
            if (ui->opt_cat != old_cat) {
                set_stop_bt(ui);
                dh_sfx_nav();
            }
            if (keys & (HidNpadButton_A | HidNpadButton_Right)) {
                ui->opt_nav = 0;
                ui->opt_row = 0;
                ui->opt_scroll = 0;
                if (!opt_row_enabled(ui, ui->opt_row)) {
                    opt_move_row(ui, 1);
                } else {
                    dh_sfx_nav();
                }
                opt_ensure_visible(ui);
            }
            return;
        }
        if (keys & HidNpadButton_Up) {
            opt_move_row(ui, -1);
        }
        if (keys & HidNpadButton_Down) {
            opt_move_row(ui, 1);
        }
        if (keys & HidNpadButton_Left) {
            opt_nudge(ui, -1);
            return;
        }
        if (keys & HidNpadButton_Right) {
            opt_nudge(ui, 1);
            return;
        }
        if (keys & HidNpadButton_A) {
            if (ui->opt_cat == SETCAT_COVERS) {
                covers_activate(ui, ui->opt_row);
            } else if (set_uses_table(ui)) {
                set_activate(ui);
            }
            return;
        }
        return;
    }
    if (ui->menu) {
        if (keys & HidNpadButton_B) {
            ui->menu = 0;
            return;
        }
        int old_item = ui->menu_item;
        if (keys & HidNpadButton_Up) {
            ui->menu_item = (ui->menu_item + MENU_COUNT - 1) % MENU_COUNT;
        }
        if (keys & HidNpadButton_Down) {
            ui->menu_item = (ui->menu_item + 1) % MENU_COUNT;
        }
        if (ui->menu_item != old_item) {
            dh_sfx_nav();
        }
        if (keys & HidNpadButton_A) {
            menu_activate(ui);
        }
        return;
    }

    if (keys & HidNpadButton_Minus) {
        set_tab(ui, TAB_GAMES);
        return;
    }
    if (keys & HidNpadButton_Plus) {
        set_tab(ui, TAB_HB);
        return;
    }
    if ((keys & HidNpadButton_StickR) && ui->cfg.sort == DH_SORT_CUSTOM && !ui->query[0]) {
        ui->sort_edit = !ui->sort_edit;
        toast(ui, ui->sort_edit ? "Rearrange on" : "Rearrange off");
        return;
    }

    if (keys & HidNpadButton_B) {
        if (ui->sort_edit) {
            ui->sort_edit = 0;
            return;
        }
        if (ui->query[0]) {
            apply_query(ui, "");
            return;
        }
#ifdef DH_ULAUNCH
        u64 running = 0;
        if (query_running(ui, &running)) {
            report_smi(ui, "Resume", deck_smi_resume_app());
        }
        return;
#elif defined(DH_MENU_APPLET)
        u64 running = 0;
        if (query_running(ui, &running)) {
            report_ipc(ui, "Resume", dh_ipc_resume_app());
        }
        return;
#else
        ui->exit_flag = 1;
        return;
#endif
    }
    if (keys & HidNpadButton_A) {
        launch_current(ui);
        return;
    }
    if (keys & HidNpadButton_Y) {
        open_item_menu(ui);
        return;
    }
}

void dh_ui_handle_input(DhUi *ui, u64 keys_down, u64 keys_held)
{
    if (!ui) {
        return;
    }
    ui->held = keys_held;
    int blocking = ui->overlay || ui->options || ui->menu;
    if (blocking) {
        u64 down = lstick_as_dpad(keys_down);
        int dx = 0;
        int dy = 0;
        axis_from(keys_held, HidNpadButton_StickLLeft, HidNpadButton_StickLRight,
            HidNpadButton_StickLUp, HidNpadButton_StickLDown, &dx, &dy);
        if (nav_pulse(&ui->nav_ms, &ui->nav_dx, &ui->nav_dy, &ui->nav_rep, dx, dy, SDL_GetTicks())) {
            u64 fake = 0;
            if (dx < 0) {
                fake |= HidNpadButton_Left;
            }
            if (dx > 0) {
                fake |= HidNpadButton_Right;
            }
            if (dy < 0) {
                fake |= HidNpadButton_Up;
            }
            if (dy > 0) {
                fake |= HidNpadButton_Down;
            }
            if (fake && !(down & fake)) {
                down |= fake;
            }
        }
        if (down) {
            dh_ui_handle_down(ui, down);
        }
        return;
    }
    if (keys_down) {
        dh_ui_handle_down(ui, keys_down);
    }
    if (!ui->overlay && !ui->options && !ui->menu) {
        grid_nav_tick(ui, keys_down, keys_held);
    }
}

static void refresh_status(DhUi *ui)
{
    ui->status_tick++;
    if (ui->overlay != OV_MTP && (ui->status_tick == 45 || (ui->status_tick % 90) == 0)) {
        query_running(ui, NULL);
    }
    if (ui->status_tick > 1 && (ui->status_tick % 30) != 0) {
        return;
    }
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (tm) {
        snprintf(ui->clock, sizeof(ui->clock), "%02d:%02d", tm->tm_hour, tm->tm_min);
    }
    u32 pct = 0;
    if (R_SUCCEEDED(psmGetBatteryChargePercentage(&pct))) {
        ui->battery_pct = pct;
        snprintf(ui->battery, sizeof(ui->battery), "%u%%", (unsigned)pct);
    } else {
        ui->battery_pct = 0;
        snprintf(ui->battery, sizeof(ui->battery), "--");
    }
    NifmInternetConnectionType ctype = 0;
    u32 strength = 0;
    NifmInternetConnectionStatus cst = 0;
    ui->wifi = 0;
    if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&ctype, &strength, &cst))) {
        if (cst == NifmInternetConnectionStatus_Connected) {
            ui->wifi = (int)strength;
            if (ui->wifi < 1) {
                ui->wifi = 1;
            }
        }
    }
}

static void poll_touch(DhUi *ui);

void dh_ui_tick(DhUi *ui, SDL_Renderer *r)
{
    poll_touch(ui);
    commit_easyapi_import(ui);
    refresh_status(ui);
#ifdef DH_ULAUNCH
    if (deck_sys_take_overlay()) {
        close_overlay(ui);
        open_settings(ui);
    }
    if (ui->options && ui->opt_cat == SETCAT_BLUETOOTH) {
        deck_sys_bt_poll();
    }
#endif
    if (ui->toast_left > 0) {
        ui->toast_left--;
    }
    clamp_item(ui);

    float menu_step = 1.f / (DH_MENU_ANIM_SEC * 60.f);
    if (ui->menu) {
        ui->menu_t += menu_step;
        if (ui->menu_t > 1.f) {
            ui->menu_t = 1.f;
        }
    } else {
        ui->menu_t -= menu_step;
        if (ui->menu_t < 0.f) {
            ui->menu_t = 0.f;
        }
    }

    int fid = ui->tab * 10000 + ui->item[ui->tab];
    if (fid != ui->focus_id) {
        ui->focus_id = fid;
        ui->card_t = 0.f;
        ui->marquee = 0.f;
        ui->marquee_hold = DH_MARQUEE_HOLD;
    }
    if (ui->marquee_hold > 0) {
        ui->marquee_hold--;
    } else {
        ui->marquee += 1.4f;
    }
    ui->card_t += 1.f / (DH_CARD_ANIM_SEC * 60.f);
    if (ui->card_t > 1.f) {
        ui->card_t = 1.f;
    }

    int n = tab_len(ui);
    int cols = grid_cols(ui);
    int rh = row_h(ui);
    int focus_row = n > 0 ? ui->item[ui->tab] / cols : 0;
    int rows = n > 0 ? (n + cols - 1) / cols : 0;
    int view_rows = DH_CONTENT_H / rh;
    if (view_rows < 1) {
        view_rows = 1;
    }
    int max_scroll_row = rows - view_rows;
    if (max_scroll_row < 0) {
        max_scroll_row = 0;
    }
    int target_row = focus_row;
    if (target_row > max_scroll_row) {
        target_row = max_scroll_row;
    }
    float target = (float)(target_row * rh);
    ui->scroll_y += (target - ui->scroll_y) * 0.28f;

    int vis0 = (int)(ui->scroll_y / (float)rh) * cols;
    int vis1 = vis0 + (view_rows + 2) * cols;
    if (!ui->options) {
        int burst = ui->icons_warm ? 12 : 32;
        if (ui->query[0]) {
            if (ui->tab == TAB_HB) {
                dh_hb_pump_icons(r, 0, dh_hb_count(), burst);
            } else {
                dh_titles_pump_icons(r, 0, dh_titles_count(), burst);
            }
        } else if (ui->tab == TAB_HB) {
            dh_hb_pump_icons(r, vis0, vis1, burst);
        } else {
            dh_titles_pump_icons(r, vis0, vis1, burst);
        }
        ui->icons_warm = 1;
        if (ui->tab == TAB_GAMES) {
            u64 vis_ids[48];
            int vis_n = 0;
            int n_all = tab_len(ui);
            int a = vis0;
            int b = vis1;
            if (a < 0) {
                a = 0;
            }
            if (b > n_all) {
                b = n_all;
            }
            for (int i = a; i < b && vis_n < 48; i++) {
                int idx = vis_real(ui, ui->tab, i);
                DhTitle *t = dh_titles_get(idx);
                if (t) {
                    vis_ids[vis_n++] = t->application_id;
                }
            }
            dh_sgdb_hint_visible(vis_ids, vis_n);
        }
        dh_sgdb_pump(r);
    } else if (ui->overlay == OV_SGDB_PICK) {
        /* Settings stay open during Setup All Covers; still load thumbs + cache. */
        if (ui->setup_on && ui->setup_id) {
            dh_sgdb_hint_visible(&ui->setup_id, 1);
            dh_titles_pump_icons(r, ui->setup_i, ui->setup_i, 1);
        } else if (ui->tab == TAB_GAMES) {
            DhTitle *t = dh_titles_get(vis_real(ui, ui->tab, ui->item[ui->tab]));
            if (t) {
                dh_sgdb_hint_visible(&t->application_id, 1);
            }
        }
        dh_sgdb_pump(r);
    }
}

static TTF_Font *ui_bold(DhUi *ui)
{
    return ui->fonts->bold ? ui->fonts->bold : ui->fonts->small;
}

static void blit_icon(DhUi *ui, SDL_Texture *icon, int iw, int ih, int dx, int dy, int dw, int dh)
{
    const DhGridStyle *st = ui_style_c(ui);
    if (st->format == DH_FMT_SQUARE || st->scale == DH_SCALE_COVER) {
        dh_cover(ui->r, icon, iw, ih, dx, dy, dw, dh);
        return;
    }
    if (st->scale == DH_SCALE_CONTAIN) {
        dh_contain(ui->r, icon, iw, ih, dx, dy, dw, dh);
        return;
    }
    dh_stretch(ui->r, icon, dx, dy, dw, dh);
}

static void draw_capsule(DhUi *ui, int x, int y, int w, int h, int focused, int live, const char *name, SDL_Texture *icon, int iw, int ih, Uint8 cr, Uint8 cg, Uint8 cb)
{
    const DhTheme *th = ui->th;
    SDL_Renderer *r = ui->r;
    float t = focused ? ui->card_t : 0.f;
    float e = t * t * (3.f - 2.f * t);
    int pressed = focused && (ui->held & HidNpadButton_A);
    int dx = x, dy = y, dw = w, dhgt = h;
    if (focused && e > 0.01f) {
        dw = w + (int)(10.f * e);
        dhgt = h + (int)(10.f * e);
        dx = x - (dw - w) / 2;
        dy = y;
        dh_card_shadow(r, dx, dy, dw, dhgt, e);
    }
    int ph = poster_h(ui) + (dhgt - h);

    if (icon && iw > 0 && ih > 0) {
        if (pressed) {
            SDL_SetTextureColorMod(icon, 204, 204, 204);
        }
        blit_icon(ui, icon, iw, ih, dx, dy, dw, ph);
        if (pressed) {
            SDL_SetTextureColorMod(icon, 255, 255, 255);
        }
    } else {
        dh_fill(r, dx, dy, dw, ph, (SDL_Color){cr, cg, cb, 255});
        if (name && name[0]) {
            char letter[8];
            snprintf(letter, sizeof(letter), "%c", name[0]);
            TTF_Font *lf = ui->fonts->sub ? ui->fonts->sub : ui->fonts->medium;
            int lw = dh_text_width(lf, letter);
            dh_text(r, lf, letter, dx + (dw - lw) / 2, dy + ph / 2 - 16, th->white);
        }
    }
    if (live) {
        dh_fill(r, dx, dy + dhgt - 8, dw, 8, th->green);
        dh_circle(r, dx + 14, dy + 14, 12, th->green);
    }
    if (focused) {
        int bw = 2 * (ui->cfg.border + 1);
        SDL_Color oc = ui->sort_edit ? th->accent : th->white;
        dh_round_outline(r, dx, dy, dw, dhgt, DH_CAPSULE_RADIUS, oc, bw);
        if (ui->cfg.border >= 1) {
            dh_round_outline(r, dx - 3, dy - 3, dw + 6, dhgt + 6, DH_CAPSULE_RADIUS + 3, oc, 2);
        }
        if (ui->cfg.border >= 2 || ui->sort_edit) {
            dh_glow_rect(r, dx, dy, dw, dhgt, DH_CAPSULE_RADIUS, th->accent);
        }
    }
}

static void draw_lr_chip(DhUi *ui, int x, int y, int w, int h, const char *lab)
{
    const DhTheme *th = ui->th;
    TTF_Font *f = ui->fonts->hint ? ui->fonts->hint : ui_bold(ui);
    dh_round_rect(ui->r, x, y, w, h, 10, th->white);
    int tw, tht;
    dh_text_size(f, lab, &tht);
    tw = dh_text_width(f, lab);
    dh_text(ui->r, f, lab, x + (w - tw) / 2, y + (h - tht) / 2, th->header);
}

static TTF_Font *tab_name_font(DhUi *ui, int on)
{
    if (on && ui->fonts->large) {
        return ui->fonts->large;
    }
    if (ui->fonts->sub) {
        return ui->fonts->sub;
    }
    return ui_bold(ui);
}

static TTF_Font *tab_count_font(DhUi *ui)
{
    return ui->fonts->medium ? ui->fonts->medium : ui->fonts->small;
}

static int tab_width(DhUi *ui, const char *name, int count, int on)
{
    TTF_Font *tabf = tab_name_font(ui, on);
    TTF_Font *nf = tab_count_font(ui);
    char num[16];
    snprintf(num, sizeof(num), "%d", count);
    int pad = on ? DH_TAB_PAD_ON : DH_TAB_PAD_OFF;
    return dh_text_width(tabf, name) + DH_TAB_GAP + dh_text_width(nf, num) + pad * 2;
}

static void draw_tab(DhUi *ui, int *x, int py, const char *name, int count, int on)
{
    const DhTheme *th = ui->th;
    TTF_Font *tabf = tab_name_font(ui, on);
    TTF_Font *nf = tab_count_font(ui);
    char num[16];
    snprintf(num, sizeof(num), "%d", count);
    int nh = 0, ch = 0;
    int nw = dh_text_size(tabf, name, &nh);
    int cw = dh_text_size(nf, num, &ch);
    int pad = on ? DH_TAB_PAD_ON : DH_TAB_PAD_OFF;
    int ph = DH_TAB_PILL_H;
    int tw = nw + DH_TAB_GAP + cw + pad * 2;
    if (on) {
        dh_round_rect(ui->r, *x, py, tw, ph, ph / 2, th->pill);
    }
    int name_y = py + (ph - nh) / 2;
    int num_y = py + (ph - ch) / 2;
    dh_text(ui->r, tabf, name, *x + pad, name_y, on ? th->white : th->muted);
    dh_text(ui->r, nf, num, *x + pad + nw + DH_TAB_GAP, num_y, th->muted);
    *x += tw + DH_TAB_GAP;
}

static void draw_wifi(DhUi *ui, int x, int y)
{
    const DhTheme *th = ui->th;
    int bars = ui->wifi;
    for (int i = 0; i < 3; i++) {
        int h = 10 + i * 8;
        SDL_Color c = (bars > i) ? th->white : th->muted;
        dh_fill(ui->r, x + i * 10, y + 28 - h, 7, h, c);
    }
}

static void draw_battery_icon(DhUi *ui, int x, int y)
{
    const DhTheme *th = ui->th;
    dh_round_rect(ui->r, x, y + 4, 36, 22, 4, th->white);
    dh_fill(ui->r, x + 3, y + 7, 30, 16, th->header);
    dh_fill(ui->r, x + 36, y + 10, 5, 10, th->white);
    int fill = (int)((30.f * (float)ui->battery_pct) / 100.f);
    if (fill < 0) {
        fill = 0;
    }
    if (fill > 30) {
        fill = 30;
    }
    dh_fill(ui->r, x + 3, y + 7, fill, 16, th->green);
}

static void draw_avatar(DhUi *ui, int x, int y)
{
    const DhTheme *th = ui->th;
    int s = DH_AVATAR;
    dh_round_rect(ui->r, x, y, s, s, 8, th->pill);
    if (ui->avatar) {
        SDL_Rect dst = {x + 2, y + 2, s - 4, s - 4};
        SDL_RenderCopy(ui->r, ui->avatar, NULL, &dst);
        return;
    }
    dh_round_rect(ui->r, x + 12, y + 8, 16, 16, 8, th->white);
    dh_round_rect(ui->r, x + 8, y + 24, 24, 14, 8, th->white);
}

static void draw_status_cluster(DhUi *ui, int bar_h)
{
    const DhTheme *th = ui->th;
    TTF_Font *cf = ui->fonts->clock ? ui->fonts->clock : ui->fonts->medium;
    int ch;
    dh_text_size(cf, ui->clock, &ch);
    int ty = (bar_h - ch) / 2;
    int rx = DH_SCREEN_W - DH_EDGE;
    int ay = (bar_h - DH_AVATAR) / 2;
    draw_avatar(ui, rx - DH_AVATAR, ay);
    rx -= DH_AVATAR + 20;
    int cw = dh_text_width(cf, ui->clock);
    dh_text(ui->r, cf, ui->clock, rx - cw, ty, th->white);
    rx -= cw + 22;
    int bw = dh_text_width(cf, ui->battery);
    dh_text(ui->r, cf, ui->battery, rx - bw, ty, th->white);
    rx -= bw + 12;
    draw_battery_icon(ui, rx - 44, ty - 2);
    rx -= 62;
    draw_wifi(ui, rx - 34, ty - 2);
}

static void draw_topbar(DhUi *ui)
{
    const DhTheme *th = ui->th;
    if (ui->options) {
        dh_fill(ui->r, 0, 0, DH_SCREEN_W, DH_OPT_TOP_H, th->bg);
        draw_status_cluster(ui, DH_OPT_TOP_H);
        return;
    }
    dh_fill(ui->r, 0, 0, DH_SCREEN_W, DH_CONTENT_Y, th->bg);

    int mid = (DH_TOPBAR_H - DH_SEARCH_H) / 2;
    TTF_Font *sf = ui->fonts->medium;
    dh_round_rect(ui->r, DH_EDGE, mid, DH_SEARCH_W, DH_SEARCH_H, DH_SEARCH_H / 2, th->row_on);
    dh_circle(ui->r, DH_EDGE + 18, mid + (DH_SEARCH_H - 22) / 2, 22, th->muted);
    dh_circle(ui->r, DH_EDGE + 22, mid + (DH_SEARCH_H - 14) / 2, 14, th->row_on);
    dh_fill(ui->r, DH_EDGE + 36, mid + DH_SEARCH_H / 2 + 4, 8, 3, th->muted);
    const char *lab = ui->query[0] ? ui->query : "Search games...";
    int sh;
    dh_text_size(sf, lab, &sh);
    dh_text(ui->r, sf, lab, DH_EDGE + 52, mid + (DH_SEARCH_H - sh) / 2, ui->query[0] ? th->white : th->muted);
    if (ui->query[0]) {
        int cx = DH_EDGE + DH_SEARCH_W - 36;
        int cy = mid + (DH_SEARCH_H - 26) / 2;
        dh_circle(ui->r, cx, cy, 26, th->muted);
        TTF_Font *xf = ui->fonts->hint ? ui->fonts->hint : ui->fonts->medium;
        int xw, xh;
        xw = dh_text_size(xf, "x", &xh);
        dh_text(ui->r, xf, "x", cx + (26 - xw) / 2, cy + (26 - xh) / 2, th->header);
    }
    draw_status_cluster(ui, DH_TOPBAR_H);
}

static void draw_tabs(DhUi *ui)
{
    int mid_y = DH_TOPBAR_H + DH_TABS_H / 2;
    int py = mid_y - DH_TAB_PILL_H / 2;
    int chip_y = mid_y - DH_TAB_CHIP_H / 2;
    int gc = vis_count(ui, TAB_GAMES);
    int hc = vis_count(ui, TAB_HB);
    const char *gname = dh_lang("tab_games");
    const char *hname = dh_lang("tab_homebrew");
    int gw = tab_width(ui, gname, gc, ui->tab == TAB_GAMES);
    int hw = tab_width(ui, hname, hc, ui->tab == TAB_HB);
    int total = DH_TAB_CHIP_W + 16 + gw + hw + DH_TAB_GAP + 16 + DH_TAB_CHIP_W;
    int x = (DH_SCREEN_W - total) / 2;
    draw_lr_chip(ui, x, chip_y, DH_TAB_CHIP_W, DH_TAB_CHIP_H, "-");
    x += DH_TAB_CHIP_W + 16;
    draw_tab(ui, &x, py, gname, gc, ui->tab == TAB_GAMES);
    draw_tab(ui, &x, py, hname, hc, ui->tab == TAB_HB);
    draw_lr_chip(ui, x + 4, chip_y, DH_TAB_CHIP_W, DH_TAB_CHIP_H, "+");
}

static int privacy_tex(const DhUi *ui, int idx, SDL_Texture **tex, int *tw, int *th)
{
    int i = idx % 5;
    if (i < 0) {
        i += 5;
    }
    int square = ui_style_c(ui)->format == DH_FMT_SQUARE;
    if (square && ui->privacy_sq[i]) {
        *tex = ui->privacy_sq[i];
        *tw = ui->privacy_sq_w[i];
        *th = ui->privacy_sq_h[i];
        return 1;
    }
    if (!square && ui->privacy_cap[i]) {
        *tex = ui->privacy_cap[i];
        *tw = ui->privacy_cap_w[i];
        *th = ui->privacy_cap_h[i];
        return 1;
    }
    if (ui->privacy_cap[i]) {
        *tex = ui->privacy_cap[i];
        *tw = ui->privacy_cap_w[i];
        *th = ui->privacy_cap_h[i];
        return 1;
    }
    if (ui->privacy_sq[i]) {
        *tex = ui->privacy_sq[i];
        *tw = ui->privacy_sq_w[i];
        *th = ui->privacy_sq_h[i];
        return 1;
    }
    if (ui->privacy) {
        *tex = ui->privacy;
        *tw = ui->privacy_w;
        *th = ui->privacy_h;
        return 1;
    }
    return 0;
}

static void draw_entry(DhUi *ui, int idx, int x, int y, int focused)
{
    int w = cell_w(ui);
    int h = cell_h(ui);
    SDL_Texture *ptex = NULL;
    int pw = 0, ph = 0;
    if (ui->cfg.privacy_covers && privacy_tex(ui, idx, &ptex, &pw, &ph)) {
        draw_capsule(ui, x, y, w, h, focused, 0, privacy_label(), ptex, pw, ph, 27, 40, 56);
        return;
    }
    if (ui->tab == TAB_HB) {
        DhHb *hb = dh_hb_get(idx);
        draw_capsule(ui, x, y, w, h, focused, 0, hb ? hb->name : "?", hb ? hb->icon : NULL,
                     hb ? hb->icon_w : 0, hb ? hb->icon_h : 0,
                     hb ? hb->cr : 27, hb ? hb->cg : 40, hb ? hb->cb : 56);
    } else {
        DhTitle *t = dh_titles_get(idx);
        if (!t) {
            return;
        }
        int live = ui->running_id && t->application_id == ui->running_id;
        int cw = 0, ch = 0;
        SDL_Texture *cover = dh_sgdb_icon(t->application_id, &cw, &ch);
        if (cover && cw > 0 && ch > 0) {
            draw_capsule(ui, x, y, w, h, focused, live, t->name, cover, cw, ch, t->cr, t->cg, t->cb);
        } else {
            draw_capsule(ui, x, y, w, h, focused, live, t->name, t->icon, t->icon_w, t->icon_h,
                         t->cr, t->cg, t->cb);
        }
    }
}

static void draw_grid(DhUi *ui)
{
    const DhTheme *th = ui->th;
    int n = tab_len(ui);
    int cols = grid_cols(ui);
    int cw = cell_w(ui);
    int ch = cell_h(ui);
    int rh = row_h(ui);
    int gy = DH_CONTENT_Y - (int)(ui->scroll_y + 0.5f);
    SDL_Rect clip = {0, DH_CONTENT_Y, DH_SCREEN_W, DH_CONTENT_H};
    SDL_RenderSetClipRect(ui->r, &clip);
    if (n <= 0) {
        dh_text(ui->r, ui->fonts->medium, ui->query[0] ? "No matches" : "Empty", DH_GUTTER, gy + 40, th->muted);
        SDL_RenderSetClipRect(ui->r, NULL);
        return;
    }
    int focus = ui->item[ui->tab];
    int focus_x = 0, focus_y = 0;
    int have_focus = 0;
    for (int idx = 0; idx < n; idx++) {
        int col = idx % cols;
        int row = idx / cols;
        int x = DH_GUTTER + col * (cw + DH_CAPSULE_GAP);
        int y = gy + row * rh;
        if (y + ch < DH_CONTENT_Y || y > DH_FOOTER_Y) {
            continue;
        }
        if (idx == focus) {
            focus_x = x;
            focus_y = y;
            have_focus = 1;
            continue;
        }
        draw_entry(ui, vis_real(ui, ui->tab, idx), x, y, 0);
    }
    if (have_focus) {
        draw_entry(ui, vis_real(ui, ui->tab, focus), focus_x, focus_y, 1);
    }
    SDL_RenderSetClipRect(ui->r, NULL);
}

static int hint_item_w(DhUi *ui, const char *key, const char *label)
{
    (void)key;
    TTF_Font *lf = ui->fonts->medium;
    return DH_HINT_D + 12 + dh_text_width(lf, label);
}

static int draw_hint_left(DhUi *ui, int x, int y, const char *key, const char *label)
{
    const DhTheme *th = ui->th;
    TTF_Font *kf = ui->fonts->hint ? ui->fonts->hint : ui_bold(ui);
    TTF_Font *lf = ui->fonts->medium;
    int d = DH_HINT_D;
    dh_circle(ui->r, x, y, d, th->white);
    int kh;
    int kw = dh_text_size(kf, key, &kh);
    dh_text(ui->r, kf, key, x + (d - kw) / 2, y + (d - kh) / 2, th->header);
    int lh;
    dh_text_size(lf, label, &lh);
    dh_text(ui->r, lf, label, x + d + 12, y + (d - lh) / 2, th->text);
    return x + hint_item_w(ui, key, label);
}

static int draw_hint_from_right(DhUi *ui, int right, int y, const char *key, const char *label)
{
    int w = hint_item_w(ui, key, label);
    draw_hint_left(ui, right - w, y, key, label);
    return right - w;
}

static void draw_footer_name(DhUi *ui, int left, int right, int y)
{
    const char *name = focus_name(ui);
    int max_w = right - left;
    if (!name || !name[0] || max_w < 24) {
        return;
    }
    TTF_Font *font = ui->fonts->sub ? ui->fonts->sub : ui->fonts->medium;
    const DhTheme *th = ui->th;
    int tw = dh_text_width(font, name);
    int fh = TTF_FontHeight(font);
    int ty = y + (DH_HINT_D - fh) / 2;
    int draw_w = tw < max_w ? tw : max_w;
    int x = (DH_SCREEN_W - draw_w) / 2;
    if (x < left) {
        x = left;
    }
    if (x + draw_w > right) {
        x = right - draw_w;
    }
    SDL_Rect clip = {left, y, max_w, DH_HINT_D};
    SDL_RenderSetClipRect(ui->r, &clip);
    if (tw > max_w) {
        int gap = DH_MARQUEE_GAP;
        int cycle = tw + gap;
        float m = ui->marquee;
        while (m > (float)cycle) {
            m -= (float)cycle;
        }
        int ox = x - (int)m;
        dh_text(ui->r, font, name, ox, ty, th->white);
        dh_text(ui->r, font, name, ox + cycle, ty, th->white);
    } else {
        dh_text(ui->r, font, name, x, ty, th->white);
    }
    SDL_RenderSetClipRect(ui->r, NULL);
}

static void draw_footer(DhUi *ui)
{
    SDL_Color bar = {8, 10, 14, 255};
    dh_fill(ui->r, 0, DH_FOOTER_Y, DH_SCREEN_W, DH_FOOTER_H, bar);
    int y = DH_FOOTER_Y + (DH_FOOTER_H - DH_HINT_D) / 2;
    int right = DH_SCREEN_W - DH_EDGE;
    int gap = 32;
    if (ui->overlay == OV_CLOSE || ui->overlay == OV_UN1 || ui->overlay == OV_UN2
        || ui->overlay == OV_SGDB_FMT || ui->overlay == OV_SGDB_SKIP || ui->overlay == OV_BT) {
        right = draw_hint_from_right(ui, right, y, "B", "BACK");
        draw_hint_from_right(ui, right - gap, y, "A", "OK");
        return;
    }
    if (ui->overlay == OV_MTP) {
        draw_hint_from_right(ui, right, y, "B", "STOP");
        return;
    }
    if (ui->overlay == OV_SGDB) {
        draw_hint_from_right(ui, right, y, "B", "HIDE");
        return;
    }
    if (ui->overlay == OV_SGDB_PICK) {
        right = draw_hint_from_right(ui, right, y, "B", ui->setup_on ? "STOP" : "BACK");
        if (ui->setup_on) {
            right = draw_hint_from_right(ui, right - gap, y, "Y", "SKIP");
        }
        draw_hint_from_right(ui, right - gap, y, "A", "USE");
        return;
    }
    if (ui->overlay == OV_INFO) {
        draw_hint_from_right(ui, right, y, "B", "BACK");
        return;
    }
    if (ui->overlay == OV_ITEM) {
        right = draw_hint_from_right(ui, right, y, "B", "BACK");
        draw_hint_from_right(ui, right - gap, y, "A", "SELECT");
        return;
    }
    if (ui->options) {
        right = draw_hint_from_right(ui, right, y, "X", "CLOSE");
        right = draw_hint_from_right(ui, right - gap, y, "B", "BACK");
        if (!ui->opt_nav) {
            draw_hint_from_right(ui, right - gap, y, "A", "SELECT");
        }
        return;
    }
    if (ui->menu) {
        right = draw_hint_from_right(ui, right, y, "B", "BACK");
        draw_hint_from_right(ui, right - gap, y, "A", "SELECT");
        return;
    }
    int after = draw_hint_left(ui, DH_EDGE, y, "X", "MENU");
    right = draw_hint_from_right(ui, right, y, "B", "BACK");
    right = draw_hint_from_right(ui, right - gap, y, "A", "SELECT");
    right = draw_hint_from_right(ui, right - gap, y, "Y", "OPTIONS");
    if (ui->sort_edit) {
        right = draw_hint_from_right(ui, right - gap, y, "R3", "DONE");
    } else if (ui->cfg.sort == DH_SORT_CUSTOM) {
        right = draw_hint_from_right(ui, right - gap, y, "R3", "MOVE");
    }
    draw_footer_name(ui, after + 28, right - 28, y);
}

typedef struct {
    int card_x;
    int card_y;
    int sx;
    int sy;
    int lx;
    int rx;
    int py;
    int pw;
    int ph;
} PickGeom;

static void pick_geom(const DhUi *ui, PickGeom *g)
{
    g->pw = PICK_PREV_W;
    g->ph = ui->cfg.games.format == DH_FMT_SQUARE ? PICK_PREV_W : (PICK_PREV_W * 3) / 2;
    int total = PICK_PREV_W * 2 + PICK_SIDE_GAP * 2 + PICK_CARD_W;
    int left = (DH_SCREEN_W - total) / 2;
    g->card_x = left + PICK_PREV_W + PICK_SIDE_GAP;
    g->card_y = (DH_SCREEN_H - PICK_CARD_H) / 2 - 12;
    int grid_w = PICK_COLS * PICK_SLOT + (PICK_COLS - 1) * PICK_GAP;
    g->sx = g->card_x + (PICK_CARD_W - grid_w) / 2;
    g->sy = g->card_y + PICK_GRID_Y;
    g->lx = left;
    g->rx = g->card_x + PICK_CARD_W + PICK_SIDE_GAP;
    g->py = g->card_y + (PICK_CARD_H - g->ph) / 2;
}

static int pick_scroll_row(int focus, int n)
{
    int rows = (n + PICK_COLS - 1) / PICK_COLS;
    int max_s = rows - PICK_VIS_ROWS;
    if (max_s < 0) {
        max_s = 0;
    }
    int fr = focus / PICK_COLS;
    int s = fr - PICK_VIS_ROWS + 1;
    if (s < 0) {
        s = 0;
    }
    if (fr < s) {
        s = fr;
    }
    if (s > max_s) {
        s = max_s;
    }
    return s;
}

static int hit(int px, int py, int x, int y, int w, int h)
{
    return px >= x && py >= y && px < x + w && py < y + h;
}

static void handle_tap(DhUi *ui, int px, int py)
{
    int fy = DH_FOOTER_Y + (DH_FOOTER_H - DH_HINT_D) / 2;
    if (ui->overlay == OV_MTP) {
        if (py >= DH_FOOTER_Y) {
            close_overlay(ui);
        }
        return;
    }
    if (ui->overlay == OV_SGDB) {
        commit_easyapi_import(ui);
        ui->overlay = OV_NONE;
        return;
    }
    if (ui->overlay == OV_SGDB_FMT || ui->overlay == OV_SGDB_SKIP) {
        int fy2 = DH_FOOTER_Y + (DH_FOOTER_H - DH_HINT_D) / 2;
        int a_w = hint_item_w(ui, "A", "OK");
        int b_w = hint_item_w(ui, "B", "BACK");
        int b_x = DH_SCREEN_W - DH_EDGE - b_w;
        int a_x = b_x - 32 - a_w;
        if (py >= DH_FOOTER_Y) {
            if (hit(px, py, a_x, fy2, a_w, DH_HINT_D)) {
                dh_ui_handle_down(ui, HidNpadButton_A);
            } else if (hit(px, py, b_x, fy2, b_w, DH_HINT_D)) {
                close_overlay(ui);
            }
            return;
        }
        int x, y, ax, cx, by;
        confirm_geom(&x, &y, &ax, &cx, &by);
        if (hit(px, py, ax, by, CONFIRM_BTN_W, CONFIRM_BTN_H)) {
            ui->overlay_btn = 0;
            dh_ui_handle_down(ui, HidNpadButton_A);
        } else if (hit(px, py, cx, by, CONFIRM_BTN_W, CONFIRM_BTN_H)) {
            ui->overlay_btn = 1;
            dh_ui_handle_down(ui, HidNpadButton_A);
        }
        return;
    }
    if (ui->overlay == OV_SGDB_PICK) {
        DhSgdbStatus st;
        dh_sgdb_status(&st);
        PickGeom g;
        pick_geom(ui, &g);
        int scroll = pick_scroll_row(ui->overlay_item, st.pick_n);
        int vis0 = scroll * PICK_COLS;
        int vis1 = vis0 + PICK_COLS * PICK_VIS_ROWS;
        if (vis1 > st.pick_n) {
            vis1 = st.pick_n;
        }
        for (int i = vis0; i < vis1; i++) {
            int local = i - vis0;
            int col = local % PICK_COLS;
            int row = local / PICK_COLS;
            int cx = g.sx + col * (PICK_SLOT + PICK_GAP);
            int cy = g.sy + row * (PICK_SLOT + PICK_GAP);
            if (hit(px, py, cx, cy, PICK_SLOT, PICK_SLOT)) {
                ui->overlay_item = i;
                dh_ui_handle_down(ui, HidNpadButton_A);
                return;
            }
        }
        if (hit(px, py, g.rx, g.py, g.pw, g.ph) && st.pick_ready && st.pick_n > 0) {
            dh_ui_handle_down(ui, HidNpadButton_A);
            return;
        }
        if (hit(px, py, g.lx, g.py, g.pw, g.ph) || hit(px, py, g.card_x, g.card_y, PICK_CARD_W, PICK_CARD_H)) {
            return;
        }
        dh_ui_handle_down(ui, HidNpadButton_B);
        return;
    }
    if (ui->overlay == OV_CLOSE || ui->overlay == OV_UN1 || ui->overlay == OV_UN2) {
        int a_w = hint_item_w(ui, "A", "OK");
        int b_w = hint_item_w(ui, "B", "BACK");
        int b_x = DH_SCREEN_W - DH_EDGE - b_w;
        int a_x = b_x - 32 - a_w;
        if (py >= DH_FOOTER_Y) {
            if (hit(px, py, a_x, fy, a_w, DH_HINT_D)) {
                dh_ui_handle_down(ui, HidNpadButton_A);
            } else if (hit(px, py, b_x, fy, b_w, DH_HINT_D)) {
                close_overlay(ui);
            }
            return;
        }
        int x, y, ax, cx, by;
        confirm_geom(&x, &y, &ax, &cx, &by);
        if (hit(px, py, ax, by, CONFIRM_BTN_W, CONFIRM_BTN_H)) {
            ui->overlay_btn = 0;
            dh_ui_handle_down(ui, HidNpadButton_A);
        } else if (hit(px, py, cx, by, CONFIRM_BTN_W, CONFIRM_BTN_H)) {
            close_overlay(ui);
        }
        return;
    }
    if (ui->overlay == OV_INFO) {
        ui->overlay = OV_ITEM;
        return;
    }
    if (ui->overlay == OV_ITEM) {
        int card_w = 560;
        int n = item_menu_n(ui);
        int card_h = 72 * n + 48;
        int x = (DH_SCREEN_W - card_w) / 2;
        int y = (DH_SCREEN_H - card_h) / 2;
        for (int i = 0; i < n; i++) {
            int ry = y + 24 + i * 72;
            if (hit(px, py, x + 20, ry, card_w - 40, 64)) {
                ui->overlay_item = i;
                item_activate(ui);
                return;
            }
        }
        close_overlay(ui);
        return;
    }
    if (ui->overlay == OV_BT) {
#ifdef DH_ULAUNCH
        int n = ui->bt_discover ? 1 : 3;
        int card_w = 560;
        int card_h = 72 * n + 96;
        int x = (DH_SCREEN_W - card_w) / 2;
        int y = (DH_SCREEN_H - card_h) / 2;
        for (int i = 0; i < n; i++) {
            int ry = y + 72 + i * 72;
            if (hit(px, py, x + 20, ry, card_w - 40, 64)) {
                ui->overlay_item = i;
                dh_ui_handle_down(ui, HidNpadButton_A);
                return;
            }
        }
#endif
        close_overlay(ui);
        return;
    }
    int menu_w = hint_item_w(ui, "X", "MENU");
    int a_w = hint_item_w(ui, "A", "SELECT");
    int b_w = hint_item_w(ui, "B", "BACK");
    int y_w = hint_item_w(ui, "Y", "OPTIONS");
    int b_x = DH_SCREEN_W - DH_EDGE - b_w;
    int a_x = b_x - 32 - a_w;
    int y_x = a_x - 32 - y_w;
    if (py >= DH_FOOTER_Y) {
        if (hit(px, py, DH_EDGE, fy, menu_w + 8, DH_HINT_D)) {
            dh_ui_handle_down(ui, HidNpadButton_X);
            return;
        }
        if (hit(px, py, a_x, fy, a_w, DH_HINT_D)) {
            dh_ui_handle_down(ui, HidNpadButton_A);
            return;
        }
        if (hit(px, py, b_x, fy, b_w, DH_HINT_D)) {
            dh_ui_handle_down(ui, HidNpadButton_B);
            return;
        }
        if (!ui->options && !ui->menu && hit(px, py, y_x, fy, y_w, DH_HINT_D)) {
            dh_ui_handle_down(ui, HidNpadButton_Y);
            return;
        }
        return;
    }

    if (!ui->options && py < DH_TOPBAR_H) {
        int mid = (DH_TOPBAR_H - DH_SEARCH_H) / 2;
        if (ui->query[0] && hit(px, py, DH_EDGE + DH_SEARCH_W - 44, mid, 40, DH_SEARCH_H)) {
            apply_query(ui, "");
            return;
        }
        if (hit(px, py, DH_EDGE, mid, DH_SEARCH_W, DH_SEARCH_H)) {
            open_search(ui);
            return;
        }
        return;
    }

    if (ui->options) {
        int nav_y = options_nav_y();
        for (int i = 0; i < SETCAT_COUNT; i++) {
            int y = nav_y + i * DH_SET_NAV_H;
            if (hit(px, py, 16, y, DH_SET_NAV_W - 32, DH_SET_NAV_H - 4)) {
                if (ui->opt_cat != i) {
                    set_stop_bt(ui);
                    dh_sfx_nav();
                } else if (!ui->opt_nav) {
                    dh_sfx_nav();
                }
                ui->opt_cat = i;
                ui->opt_nav = 1;
                ui->opt_row = 0;
                ui->opt_scroll = 0;
                return;
            }
        }
        int cx = DH_SET_NAV_W + 24;
        int cw = DH_SCREEN_W - cx - 48;
        if (ui->opt_cat == SETCAT_LIBRARY) {
            int head_y[2], row_y[10], vis[10];
            lib_pack_ys(ui, head_y, row_y, vis);
            for (int i = 0; i < LIB_ROW_N; i++) {
                if (!vis[i] || !hit(px, py, cx, row_y[i], cw, DH_SET_CHOICE_H)) {
                    continue;
                }
                int moved = ui->opt_nav || ui->opt_row != i;
                ui->opt_nav = 0;
                ui->opt_row = i;
                int spx, spy, spw, sph;
                seg_pill_rect(cx, row_y[i], cw, &spx, &spy, &spw, &sph);
                if (hit(px, py, spx, spy, spw, sph) && spw > 0) {
                    int n = lib_seg_count(i);
                    int seg = ((px - spx) * n) / spw;
                    int old = lib_value(ui, i);
                    lib_apply(ui, i, seg);
                    if (lib_value(ui, i) != old) {
                        dh_sfx_nav();
                    }
                } else if (moved) {
                    dh_sfx_nav();
                }
                return;
            }
            return;
        }
        if (ui->opt_cat == SETCAT_COVERS) {
            int cy = options_body_y();
            if (covers_udeck(ui) || !sgdb_key_ready(ui)) {
                cy += covers_hint_h(ui);
            }
            for (int row = 0; row < COVERS_ROW_N; row++) {
                if (!covers_row_enabled(ui, row)) {
                    continue;
                }
                int rh = covers_row_h(row);
                if (!hit(px, py, cx, cy, cw, rh)) {
                    cy += rh + DH_SET_ROW_GAP;
                    continue;
                }
                int again = (ui->opt_row == row && !ui->opt_nav);
                int moved = ui->opt_row != row || ui->opt_nav;
                ui->opt_nav = 0;
                ui->opt_row = row;
                if (row == COVERS_ROW_SRC) {
                    int spx, spy, spw, sph;
                    seg_pill_rect(cx, cy, cw, &spx, &spy, &spw, &sph);
                    if (hit(px, py, spx, spy, spw, sph) && spw > 0) {
                        covers_set_src(ui, ((px - spx) * 2) / spw);
                        dh_sfx_nav();
                    } else if (again) {
                        covers_activate(ui, row);
                    } else if (moved) {
                        dh_sfx_nav();
                    }
                    return;
                }
                if (again) {
                    covers_activate(ui, row);
                } else if (moved) {
                    dh_sfx_nav();
                }
                return;
            }
            return;
        }
        if (set_uses_table(ui)) {
            set_build_rows(ui);
            int cy = options_body_y() - ui->opt_scroll;
            for (int i = 0; i < g_set_n; i++) {
                int rh = set_kind_h(g_set_rows[i].kind);
                if (g_set_rows[i].kind == RK_HEAD) {
                    cy += rh + DH_SET_ROW_GAP;
                    continue;
                }
                if (hit(px, py, cx, cy, cw, rh)) {
                    int again = (ui->opt_row == i && !ui->opt_nav);
                    int moved = ui->opt_row != i || ui->opt_nav;
                    ui->opt_nav = 0;
                    ui->opt_row = i;
                    opt_ensure_visible(ui);
#ifdef DH_ULAUNCH
                    if (g_set_rows[i].kind == RK_SEG && g_set_rows[i].id == RID_ALBUM) {
                        int spx, spy, spw, sph;
                        seg_pill_rect(cx, cy, cw, &spx, &spy, &spw, &sph);
                        if (hit(px, py, spx, spy, spw, sph) && spw > 0) {
                            deck_sys_album_set(((px - spx) * 2) / spw);
                            dh_sfx_nav();
                            return;
                        }
                    }
#endif
                    if (again) {
                        set_activate(ui);
                    } else if (moved) {
                        dh_sfx_nav();
                    }
                    return;
                }
                cy += rh + DH_SET_ROW_GAP;
            }
        }
        return;
    }

    if (ui->menu && ui->menu_t > 0.4f) {
        if (px > DH_SIDEBAR_W) {
            ui->menu = 0;
            return;
        }
        int top = DH_TOPBAR_H + 8;
        for (int i = 0; i < MENU_COUNT; i++) {
            int y = top + i * DH_SIDEBAR_ITEM_H;
            if (hit(px, py, 0, y, DH_SIDEBAR_W, DH_SIDEBAR_ITEM_H)) {
                if (ui->menu_item == i) {
                    menu_activate(ui);
                } else {
                    ui->menu_item = i;
                    dh_sfx_nav();
                }
                return;
            }
        }
        return;
    }

    if (py >= DH_TOPBAR_H && py < DH_CONTENT_Y) {
        int y = DH_TOPBAR_H + (DH_TABS_H - DH_TAB_PILL_H) / 2;
        int gc = vis_count(ui, TAB_GAMES);
        int hc = vis_count(ui, TAB_HB);
        int gw = tab_width(ui, "GAMES", gc, ui->tab == TAB_GAMES);
        int hw = tab_width(ui, "HOMEBREW", hc, ui->tab == TAB_HB);
        int total = DH_TAB_CHIP_W + 16 + gw + hw + DH_TAB_GAP + 16 + DH_TAB_CHIP_W;
        int x = (DH_SCREEN_W - total) / 2;
        int new_tab = ui->tab;
        if (hit(px, py, x - 8, y, DH_TAB_CHIP_W + 16 + gw, DH_TAB_PILL_H)) {
            new_tab = TAB_GAMES;
        } else if (hit(px, py, x + DH_TAB_CHIP_W + 16 + gw, y, hw + 16 + DH_TAB_CHIP_W, DH_TAB_PILL_H)) {
            new_tab = TAB_HB;
        }
        if (new_tab != ui->tab) {
            set_tab(ui, new_tab);
        }
        return;
    }

    if (py >= DH_CONTENT_Y && py < DH_FOOTER_Y) {
        int n = tab_len(ui);
        int cols = grid_cols(ui);
        int cw = cell_w(ui);
        int ch = cell_h(ui);
        int rh = row_h(ui);
        int gy = DH_CONTENT_Y - (int)(ui->scroll_y + 0.5f);
        for (int idx = 0; idx < n; idx++) {
            int col = idx % cols;
            int row = idx / cols;
            int x = DH_GUTTER + col * (cw + DH_CAPSULE_GAP);
            int y = gy + row * rh;
            if (hit(px, py, x, y, cw, ch)) {
                if (ui->item[ui->tab] == idx) {
                    launch_current(ui);
                } else {
                    ui->item[ui->tab] = idx;
                    dh_sfx_nav();
                }
                return;
            }
        }
    }
}

static void poll_touch(DhUi *ui)
{
    HidTouchScreenState st;
    memset(&st, 0, sizeof(st));
    if (hidGetTouchScreenStates(&st, 1) < 1) {
        return;
    }
    if (st.count > 0) {
        int px = (int)((st.touches[0].x * (u64)DH_SCREEN_W) / 1280u);
        int py = (int)((st.touches[0].y * (u64)DH_SCREEN_H) / 720u);
        if (!ui->touch_held) {
            ui->touch_held = 1;
            ui->touch_moved = 0;
            ui->touch_x = px;
            ui->touch_y = py;
        } else {
            int dx = px - ui->touch_x;
            int dy = py - ui->touch_y;
            if (dx * dx + dy * dy > 40 * 40) {
                ui->touch_moved = 1;
            }
        }
        return;
    }
    if (ui->touch_held) {
        ui->touch_held = 0;
        if (!ui->touch_moved) {
            handle_tap(ui, ui->touch_x, ui->touch_y);
        }
    }
}

static void draw_sidebar(DhUi *ui)
{
    if (ui->menu_t <= 0.001f) {
        return;
    }
    const DhTheme *th = ui->th;
    float e = ui->menu_t * ui->menu_t * (3.f - 2.f * ui->menu_t);
    SDL_Color dim = {8, 10, 14, (Uint8)(200.f * e)};
    dh_fill(ui->r, 0, 0, DH_SCREEN_W, DH_SCREEN_H, dim);

    int x = (int)(-DH_SIDEBAR_W * (1.f - e));
    dh_fill(ui->r, x, 0, DH_SIDEBAR_W, DH_SCREEN_H, th->panel);

#ifdef DH_ULAUNCH
    static const char *labels[] = {
        "Settings", "Search", "Album", "Sleep", "Reboot", "Power Off"
    };
#else
    static const char *labels[] = {
        "Options", "Search", "USB MTP", "Album", "Set homebrew donor", "Sleep", "Reboot", "Power Off"
    };
#endif
    TTF_Font *font = ui->fonts->sidebar ? ui->fonts->sidebar : ui->fonts->large;
    int top = DH_TOPBAR_H + 8;
    for (int i = 0; i < MENU_COUNT; i++) {
        int y = top + i * DH_SIDEBAR_ITEM_H;
        int on = (i == ui->menu_item);
        if (on) {
            dh_fill(ui->r, x, y, DH_SIDEBAR_W, DH_SIDEBAR_ITEM_H, th->row_on);
            dh_round_rect(ui->r, x + 12, y + 18, 6, DH_SIDEBAR_ITEM_H - 36, 3, th->accent);
        }
        int fh = 0;
        int tw = dh_text_size(font, labels[i], &fh);
        int tx = x + 36;
        int ty = y + (DH_SIDEBAR_ITEM_H - fh) / 2;
        int max_w = DH_SIDEBAR_W - 52;
        SDL_Rect clip = {tx, y, max_w, DH_SIDEBAR_ITEM_H};
        SDL_RenderSetClipRect(ui->r, &clip);
        if (on && tw > max_w) {
            int gap = 48;
            int cycle = tw + gap;
            float m = ui->marquee;
            while (m > (float)cycle) {
                m -= (float)cycle;
            }
            int ox = tx - (int)m;
            dh_text(ui->r, font, labels[i], ox, ty, th->white);
            dh_text(ui->r, font, labels[i], ox + cycle, ty, th->white);
        } else {
            dh_text(ui->r, font, labels[i], tx, ty, on ? th->white : th->text);
        }
        SDL_RenderSetClipRect(ui->r, NULL);
    }
}

static void draw_nav_icon(SDL_Renderer *r, int kind, int x, int y, SDL_Color c)
{
    if (kind == SETCAT_LIBRARY) {
        dh_fill(r, x, y, 10, 10, c);
        dh_fill(r, x + 14, y, 10, 10, c);
        dh_fill(r, x, y + 14, 10, 10, c);
        dh_fill(r, x + 14, y + 14, 10, 10, c);
        return;
    }
    if (kind == SETCAT_COVERS) {
        dh_round_rect(r, x + 2, y + 2, 20, 20, 4, c);
        dh_round_outline(r, x + 6, y + 6, 12, 12, 3, c, 2);
        return;
    }
    if (kind == SETCAT_AUDIO) {
        dh_fill(r, x + 2, y + 8, 8, 8, c);
        dh_fill(r, x + 8, y + 4, 4, 16, c);
        dh_round_outline(r, x + 12, y + 6, 10, 12, 6, c, 2);
        return;
    }
#ifdef DH_ULAUNCH
    if (kind == SETCAT_DISPLAY) {
        dh_round_outline(r, x + 2, y + 4, 20, 16, 3, c, 2);
        dh_fill(r, x + 8, y + 22, 8, 2, c);
        return;
    }
    if (kind == SETCAT_POWER) {
        dh_round_outline(r, x + 2, y + 4, 20, 16, 8, c, 2);
        dh_fill(r, x + 11, y, 2, 10, c);
        return;
    }
    if (kind == SETCAT_INTERNET) {
        dh_round_outline(r, x + 2, y + 10, 20, 10, 10, c, 2);
        dh_round_outline(r, x + 6, y + 14, 12, 6, 6, c, 2);
        dh_fill(r, x + 10, y + 20, 4, 4, c);
        return;
    }
    if (kind == SETCAT_BLUETOOTH) {
        dh_fill(r, x + 11, y, 2, 24, c);
        dh_fill(r, x + 6, y + 4, 12, 2, c);
        dh_fill(r, x + 6, y + 18, 12, 2, c);
        return;
    }
#endif
    dh_round_rect(r, x + 4, y + 4, 16, 16, 8, c);
    dh_fill(r, x + 10, y, 4, 24, c);
    dh_fill(r, x, y + 10, 24, 4, c);
}

static void draw_seg_pill(DhUi *ui, int x, int y, int w, int h, const char **labs, int n, int sel, int enabled)
{
    const DhTheme *th = ui->th;
    dh_round_rect(ui->r, x, y, w, h, h / 2, enabled ? th->pill : th->header);
    if (n < 1 || w < n) {
        return;
    }
    int inner = h - 6;
    TTF_Font *f = ui->fonts->medium;
    for (int i = 0; i < n; i++) {
        int x0 = x + (i * w) / n;
        int x1 = x + ((i + 1) * w) / n;
        int sw = x1 - x0;
        if (enabled && i == sel) {
            dh_round_rect(ui->r, x0 + 3, y + 3, sw - 6, inner, inner / 2, th->white);
        }
        int tht = 0;
        int tw = dh_text_size(f, labs[i], &tht);
        SDL_Color c = th->muted;
        if (enabled) {
            c = (i == sel) ? th->header : th->text;
        }
        dh_text(ui->r, f, labs[i], x0 + (sw - tw) / 2, y + (h - tht) / 2, c);
    }
}

static void draw_choice_row(DhUi *ui, int x, int y, int w, const char *title, const char **labs, int n, int sel, int on, int enabled)
{
    const DhTheme *th = ui->th;
    SDL_Color bg = on ? th->row_on : th->row;
    dh_round_rect(ui->r, x, y, w, DH_SET_CHOICE_H, DH_SET_RADIUS, bg);
    int tht = 0;
    dh_text_size(ui->fonts->medium, title, &tht);
    dh_text(ui->r, ui->fonts->medium, title, x + 24, y + (DH_SET_CHOICE_H - tht) / 2, enabled ? th->white : th->muted);
    int px, py, pw, ph;
    seg_pill_rect(x, y, w, &px, &py, &pw, &ph);
    draw_seg_pill(ui, px, py, pw, ph, labs, n, sel, enabled);
}

static void draw_set_row(DhUi *ui, int x, int y, int w, const char *title, const char *sub, const char *value, int on)
{
    const DhTheme *th = ui->th;
    dh_round_rect(ui->r, x, y, w, DH_SET_ROW_H, DH_SET_RADIUS, on ? th->row_on : th->row);
    dh_text(ui->r, ui->fonts->medium, title, x + 24, y + (sub ? 14 : 28), on ? th->white : th->white);
    if (sub && sub[0]) {
        dh_text(ui->r, ui->fonts->small, sub, x + 24, y + 46, th->muted);
    }
    if (value) {
        dh_text_right(ui->r, ui->fonts->small, value, x + w - 24, y + 30, th->muted);
    }
}

static void draw_switch(DhUi *ui, int x, int y, int on, int focus)
{
    const DhTheme *th = ui->th;
    int tw = focus ? 56 : DH_SWITCH_W;
    int thh = focus ? 32 : DH_SWITCH_H;
    int tx = x - tw;
    int ty = y + (DH_SET_ROW_H - thh) / 2;
    dh_round_rect(ui->r, tx, ty, tw, thh, thh / 2, on ? th->accent : th->pill);
    int kd = focus ? 26 : 22;
    int kx = on ? (tx + tw - kd - 3) : (tx + 3);
    dh_circle(ui->r, kx, ty + (thh - kd) / 2, kd, th->white);
}

static void draw_options(DhUi *ui)
{
    if (!ui->options) {
        return;
    }
    const DhTheme *th = ui->th;
    dh_fill(ui->r, 0, DH_OPT_TOP_H, DH_SCREEN_W, DH_FOOTER_Y - DH_OPT_TOP_H, th->bg);

#ifdef DH_ULAUNCH
    const char *nav[] = {
        dh_lang("cat_library"), dh_lang("cat_covers"), dh_lang("cat_audio"), dh_lang("cat_display"),
        dh_lang("cat_power"), dh_lang("cat_internet"), dh_lang("cat_bluetooth"), dh_lang("cat_system")
    };
#else
    const char *nav[] = { dh_lang("cat_library"), dh_lang("cat_covers"), dh_lang("cat_audio"), dh_lang("cat_system") };
#endif
    int nav_y = options_nav_y();
    for (int i = 0; i < SETCAT_COUNT; i++) {
        int y = nav_y + i * DH_SET_NAV_H;
        int on = (i == ui->opt_cat);
        if (on) {
            dh_round_rect(ui->r, 16, y, DH_SET_NAV_W - 32, DH_SET_NAV_H - 4, 8, th->row_on);
            dh_round_rect(ui->r, 16, y, 6, DH_SET_NAV_H - 4, 3, th->accent);
        }
        SDL_Color ic = on ? th->white : th->muted;
        draw_nav_icon(ui->r, i, 36, y + 14, ic);
        TTF_Font *nav_f = ui->fonts->sub ? ui->fonts->sub : ui->fonts->medium;
        dh_text(ui->r, nav_f, nav[i], 76, y + 14, on ? th->white : th->text);
        if (on && ui->opt_nav) {
            dh_round_rect(ui->r, 16, y, 6, DH_SET_NAV_H - 4, 3, th->white);
        }
    }

    int cx = DH_SET_NAV_W + 24;
    int cw = DH_SCREEN_W - cx - 48;
    const char *cat_title = nav[ui->opt_cat];
    dh_text(ui->r, ui->fonts->large, cat_title, cx, DH_OPT_TOP_H + 8, th->white);

    static const char *shape_labs[] = {"Square", "Cover"};
    static const char *size_labs[] = {"Large", "Medium", "Small"};
    static const char *scale_labs[] = {"Fill", "Fit", "Stretch"};
    static const char *border_labs[] = {"Thin", "Medium", "Thick"};
    static const char *titles[] = {"Tile shape", "Tile size", "Cover scale"};
    static const char *edge_labs[] = {"Wrap", "Next row", "Next tab"};
    static const char *diag_labs[] = {"Cardinal", "Diagonal"};
    static const char *sort_labs[] = {"Recents", "A-Z", "Z-A", "Custom"};

    if (ui->opt_cat == SETCAT_LIBRARY) {
        static const char *heads[] = {"Games", "Homebrew"};
        int head_y[2], row_y[10], vis[10];
        lib_pack_ys(ui, head_y, row_y, vis);
        {
            int on = !ui->opt_nav && ui->opt_row == 0;
            draw_choice_row(ui, cx, row_y[0], cw, "Focus border", border_labs, 3, ui->cfg.border, on, 1);
        }
        for (int block = 0; block < 2; block++) {
            dh_text(ui->r, ui->fonts->medium, heads[block], cx, head_y[block] + 2, th->white);
            for (int i = 0; i < 3; i++) {
                int row = 1 + block * 3 + i;
                if (!vis[row]) {
                    continue;
                }
                int on = !ui->opt_nav && ui->opt_row == row;
                const char **labs = (i == 0) ? shape_labs : (i == 1) ? size_labs : scale_labs;
                draw_choice_row(ui, cx, row_y[row], cw, titles[i], labs, lib_seg_count(row), lib_value(ui, row), on, 1);
            }
        }
        draw_choice_row(ui, cx, row_y[7], cw, "Row end", edge_labs, 3, ui->cfg.edge,
            !ui->opt_nav && ui->opt_row == 7, 1);
        draw_choice_row(ui, cx, row_y[8], cw, "Stick", diag_labs, 2, ui->cfg.stick_diag ? 1 : 0,
            !ui->opt_nav && ui->opt_row == 8, 1);
        draw_choice_row(ui, cx, row_y[9], cw, "Sort", sort_labs, 4, ui->cfg.sort,
            !ui->opt_nav && ui->opt_row == 9, 1);
    } else if (ui->opt_cat == SETCAT_COVERS) {
        int cy = options_body_y();
        TTF_Font *small_f = ui->fonts->small ? ui->fonts->small : ui->fonts->medium;
        static const char *src_labs[] = { "uDeck", "SteamGrid" };
        if (covers_udeck(ui)) {
            dh_text(ui->r, small_f, "uDeck downloads the matching box art. No picker.",
                cx, cy, th->muted);
            cy += covers_hint_h(ui);
        } else if (!sgdb_key_ready(ui)) {
            dh_text(ui->r, small_f,
                "Get a free key at steamgriddb.com. Enter it here, paste it, or use a 4-character EasyAPI code.",
                cx, cy, th->muted);
            cy += covers_hint_h(ui);
        }
        for (int row = 0; row < COVERS_ROW_N; row++) {
            if (!covers_row_enabled(ui, row)) {
                continue;
            }
            int on = ui->opt_row == row && !ui->opt_nav;
            if (row == COVERS_ROW_SRC) {
                draw_choice_row(ui, cx, cy, cw, "Source", src_labs, 2, ui->cfg.cover_src, on, 1);
            } else if (row == COVERS_ROW_EASY) {
                draw_set_row(ui, cx, cy, cw, "EasyAPI", "A: 4-character website code, or paste the full key", "Code", on);
            } else if (row == COVERS_ROW_KEY) {
                char keyv[24];
                snprintf(keyv, sizeof(keyv), "%s", ui->cfg.sgdb_key[0] ? "Set" : "Missing");
                draw_set_row(ui, cx, cy, cw, "API key", "A: type or paste the 32-character key", keyv, on);
            } else if (row == COVERS_ROW_FILE) {
                draw_set_row(ui, cx, cy, cw, "Load from SD", "A: read switch/uDeckLaunch/sgdb.key", "File", on);
            } else if (row == COVERS_ROW_TEST) {
                DhSgdbStatus sg;
                dh_sgdb_status(&sg);
                char val[24];
                if (ui->cfg.sgdb_key_ok) {
                    snprintf(val, sizeof(val), "Verified");
                } else if (sg.phase[0] && strstr(sg.phase, "Key works")) {
                    snprintf(val, sizeof(val), "OK");
                } else if (sg.error[0]) {
                    snprintf(val, sizeof(val), "Error");
                } else {
                    snprintf(val, sizeof(val), "Optional");
                }
                draw_set_row(ui, cx, cy, cw, "Test API key", "A: optional check against SteamGridDB", val, on);
            } else if (row == COVERS_ROW_SETUP) {
                draw_set_row(ui, cx, cy, cw, "Setup All Covers",
                    covers_udeck(ui) ? "A: download matching covers" : "A: pick covers per game", "Start", on);
            } else if (row == COVERS_ROW_RESET) {
                draw_set_row(ui, cx, cy, cw, "Reset all covers", "A: remove every custom cover", "Clear", on);
            } else if (row == COVERS_ROW_PRIVACY) {
                draw_set_row(ui, cx, cy, cw, dh_lang("privacy_covers"), dh_lang("privacy_covers_sub"), NULL, on);
                draw_switch(ui, cx + cw - 20, cy, ui->cfg.privacy_covers, on);
            }
            cy += covers_row_h(row) + DH_SET_ROW_GAP;
        }
    } else if (set_uses_table(ui)) {
        set_build_rows(ui);
        opt_ensure_visible(ui);
        int body_y = options_body_y();
        int view_h = DH_FOOTER_Y - 8 - body_y;
        SDL_Rect clip = { cx, body_y, cw, view_h };
        SDL_RenderSetClipRect(ui->r, &clip);
        int cy = body_y - ui->opt_scroll;
        static const char *album_labs[] = { "SD", "NAND" };
        for (int i = 0; i < g_set_n; i++) {
            const DhSetRow *row = &g_set_rows[i];
            int on = !ui->opt_nav && ui->opt_row == i;
            int rh = set_kind_h(row->kind);
            if (row->kind == RK_HEAD) {
                dh_text(ui->r, ui->fonts->medium, row->title, cx, cy + 4, th->white);
            } else if (row->kind == RK_SEG) {
                draw_choice_row(ui, cx, cy, cw, row->title, album_labs, 2, row->on, on, 1);
            } else if (row->kind == RK_SWITCH) {
                draw_set_row(ui, cx, cy, cw, row->title, row->sub, NULL, on);
                draw_switch(ui, cx + cw - 20, cy, row->on, on);
            } else if (row->kind == RK_INFO) {
                draw_set_row(ui, cx, cy, cw, row->title, row->sub, row->value[0] ? row->value : NULL, on);
            } else if (row->kind == RK_CYCLE) {
                draw_set_row(ui, cx, cy, cw, row->title, row->sub, row->value[0] ? row->value : NULL, on);
            } else {
                draw_set_row(ui, cx, cy, cw, row->title, row->sub, row->value[0] ? row->value : NULL, on);
            }
            cy += rh + DH_SET_ROW_GAP;
        }
        SDL_RenderSetClipRect(ui->r, NULL);
    }
}

static void draw_modal_btn(DhUi *ui, int x, int y, int w, int h, const char *lab, int on, int danger)
{
    const DhTheme *th = ui->th;
    SDL_Color bg;
    SDL_Color fg;
    if (on) {
        bg = danger ? th->danger : th->white;
        fg = danger ? th->white : th->header;
    } else {
        bg = th->row;
        fg = th->text;
    }
    dh_round_rect(ui->r, x, y, w, h, h / 2, bg);
    int tht = 0;
    int tw = dh_text_size(ui->fonts->medium, lab, &tht);
    dh_text(ui->r, ui->fonts->medium, lab, x + (w - tw) / 2, y + (h - tht) / 2, fg);
}

static void draw_scrim(DhUi *ui)
{
    SDL_Color dim = {8, 10, 14, 210};
    dh_fill(ui->r, 0, 0, DH_SCREEN_W, DH_SCREEN_H, dim);
}

static void draw_sgdb_spinner(DhUi *ui, int cx, int cy)
{
    const DhTheme *th = ui->th;
    static const int ox[8] = {18, 13, 0, -13, -18, -13, 0, 13};
    static const int oy[8] = {0, 13, 18, 13, 0, -13, -18, -13};
    u32 t = SDL_GetTicks();
    int on_i = (int)((t / 90) % 8);
    for (int i = 0; i < 8; i++) {
        int on = (i == on_i);
        dh_circle(ui->r, cx + ox[i] - 5, cy + oy[i] - 5, on ? 12 : 8, on ? th->accent : th->pill);
    }
}

static void draw_sgdb_progress(DhUi *ui)
{
    const DhTheme *th = ui->th;
    DhSgdbStatus st;
    dh_sgdb_status(&st);
    TTF_Font *title_f = ui->fonts->large ? ui->fonts->large : ui->fonts->medium;
    TTF_Font *body_f = ui->fonts->medium;
    TTF_Font *small_f = ui->fonts->small ? ui->fonts->small : body_f;
    int card_w = 760;
    int card_h = 420;
    int x = (DH_SCREEN_W - card_w) / 2;
    int y = (DH_SCREEN_H - card_h) / 2 - 24;
    dh_round_rect(ui->r, x, y, card_w, card_h, 18, th->panel);
    dh_round_rect(ui->r, x, y + 24, 6, 56, 3, th->accent);
    dh_text(ui->r, title_f, "SteamGridDB", x + 36, y + 32, th->white);
    const char *phase = st.phase[0] ? st.phase : (st.busy ? "Working…" : "Done");
    dh_text(ui->r, body_f, phase, x + 36, y + 108, th->text);
    if (st.current[0]) {
        dh_text(ui->r, small_f, st.current, x + 36, y + 156, th->muted);
    }
    int bar_x = x + 36;
    int bar_y = y + 214;
    int bar_w = card_w - 72;
    dh_round_rect(ui->r, bar_x, bar_y, bar_w, 12, 6, th->row);
    int tot = st.total > 0 ? st.total : (st.busy ? 1 : 0);
    if (tot > 0) {
        int fill = (bar_w * st.done) / tot;
        if (st.busy && fill < 24) {
            fill = 24;
        }
        if (fill > bar_w) {
            fill = bar_w;
        }
        if (fill > 0) {
            dh_round_rect(ui->r, bar_x, bar_y, fill, 12, 6, th->accent);
        }
    }
    char nums[64];
    snprintf(nums, sizeof(nums), "%d of %d", st.done, st.total);
    if (st.fail > 0) {
        snprintf(nums, sizeof(nums), "%d of %d  ·  %d missed", st.done, st.total, st.fail);
    }
    dh_text(ui->r, small_f, nums, x + 36, y + 240, th->muted);
    if (st.busy) {
        draw_sgdb_spinner(ui, x + card_w - 72, y + 60);
    }
    if (strstr(st.phase, "Key works")) {
        dh_text(ui->r, body_f, "This key can reach SteamGridDB.", x + 36, y + 300, th->green);
    } else if (st.error[0]) {
        dh_text(ui->r, body_f, st.error, x + 36, y + 300, th->danger);
    } else if (!st.busy && st.total > 0) {
        dh_text(ui->r, body_f, "Finished. B to close.", x + 36, y + 300, th->muted);
    }
}

static void draw_pick_side(DhUi *ui, int x, int y, int w, int h, SDL_Texture *tex, int tw, int ih,
    Uint8 cr, Uint8 cg, Uint8 cb, const char *title, const char *sub, int marked)
{
    const DhTheme *th = ui->th;
    TTF_Font *body_f = ui->fonts->medium;
    TTF_Font *small_f = ui->fonts->small ? ui->fonts->small : body_f;
    int cap_h = 0;
    dh_text_size(small_f, title, &cap_h);
    int cap_y = y - cap_h - 14;
    if (cap_y < 16) {
        cap_y = 16;
    }
    dh_text(ui->r, small_f, title, x, cap_y, marked ? th->accent : th->muted);
    dh_round_rect(ui->r, x - 10, y - 10, w + 20, h + 20, 16, th->panel);
    dh_round_rect(ui->r, x, y, w, h, 10, th->row);
    if (tex && tw > 0 && ih > 0) {
        blit_icon(ui, tex, tw, ih, x, y, w, h);
    } else {
        dh_fill(ui->r, x, y, w, h, (SDL_Color){cr, cg, cb, 255});
    }
    if (marked) {
        dh_round_outline(ui->r, x, y, w, h, 10, th->accent, 3);
        dh_glow_rect(ui->r, x, y, w, h, 10, th->accent);
    } else {
        dh_round_outline(ui->r, x, y, w, h, 10, th->muted, 2);
    }
    if (sub && sub[0]) {
        int sw = dh_text_width(small_f, sub);
        dh_text(ui->r, small_f, sub, x + (w - sw) / 2, y + h + 16, marked ? th->white : th->muted);
    }
}

static DhTitle *focus_game(DhUi *ui)
{
    if (ui->setup_on) {
        return setup_title(ui);
    }
    if (ui->tab != TAB_GAMES) {
        return NULL;
    }
    int idx = vis_real(ui, ui->tab, ui->item[ui->tab]);
    return dh_titles_get(idx);
}

static void draw_sgdb_pick(DhUi *ui)
{
    const DhTheme *th = ui->th;
    DhSgdbStatus st;
    dh_sgdb_status(&st);
    TTF_Font *title_f = ui->fonts->large ? ui->fonts->large : ui->fonts->medium;
    TTF_Font *body_f = ui->fonts->medium;
    TTF_Font *small_f = ui->fonts->small ? ui->fonts->small : body_f;
    PickGeom g;
    pick_geom(ui, &g);
    int x = g.card_x;
    int y = g.card_y;
    dh_round_rect(ui->r, x, y, PICK_CARD_W, PICK_CARD_H, 18, th->panel);
    dh_round_rect(ui->r, x, y + 24, 6, 56, 3, th->accent);
    dh_text(ui->r, title_f, "Choose cover", x + 36, y + 32, th->white);
    DhTitle *t = focus_game(ui);
    const char *nm = ui->cfg.privacy_covers ? privacy_label() : (t ? t->name : focus_name(ui));
    if (nm) {
        dh_text(ui->r, body_f, nm, x + 36, y + 100, th->text);
    }
    if (ui->setup_on && ui->setup_n > 0) {
        char prog[32];
        snprintf(prog, sizeof(prog), "%d / %d", ui->setup_k, ui->setup_n);
        int pw = dh_text_width(small_f, prog);
        dh_text(ui->r, small_f, prog, x + PICK_CARD_W - 36 - pw, y + 40, th->muted);
    }
    SDL_Texture *cur = NULL;
    int cw = 0, ch = 0;
    Uint8 cr = 27, cg = 40, cb = 56;
    if (t) {
        cr = t->cr;
        cg = t->cg;
        cb = t->cb;
        cur = dh_sgdb_icon(t->application_id, &cw, &ch);
        if (!cur || cw < 1 || ch < 1) {
            cur = t->icon;
            cw = t->icon_w;
            ch = t->icon_h;
        }
    }
    draw_pick_side(ui, g.lx, g.py, g.pw, g.ph, cur, cw, ch, cr, cg, cb, "Current", NULL, 0);

    SDL_Texture *sel = NULL;
    int sw = 0, sh = 0;
    const char *sel_sub = NULL;
    if (st.pick_ready && st.pick_n > 0) {
        if (ui->overlay_item < 0 || ui->overlay_item >= st.pick_n) {
            ui->overlay_item = 0;
        }
        sel = dh_sgdb_pick_icon(ui->overlay_item, &sw, &sh);
        sel_sub = dh_sgdb_pick_label(ui->overlay_item);
    }
    draw_pick_side(ui, g.rx, g.py, g.pw, g.ph, sel, sw, sh, cr, cg, cb, "Selected", sel_sub, 1);

    if (!st.pick_ready) {
        const char *phase = st.phase[0] ? st.phase : "Searching…";
        dh_text(ui->r, body_f, phase, x + 36, y + 250, th->muted);
        if (st.current[0]) {
            dh_text(ui->r, small_f, st.current, x + 36, y + 298, th->muted);
        }
        draw_sgdb_spinner(ui, x + PICK_CARD_W / 2, y + 400);
        if (st.error[0]) {
            dh_text(ui->r, body_f, st.error, x + 36, y + 460, th->danger);
        }
        return;
    }
    if (st.pick_n < 1) {
        const char *err = st.error[0] ? st.error : "No covers for this format";
        dh_text(ui->r, body_f, err, x + 36, y + 250, th->danger);
        if (ui->setup_on) {
            dh_text(ui->r, small_f, "Y to skip this title.", x + 36, y + PICK_CARD_H - 48, th->muted);
        }
        return;
    }
    if (!ui->setup_on) {
        char count[32];
        snprintf(count, sizeof(count), "%d covers", st.pick_n);
        int count_w = dh_text_width(small_f, count);
        dh_text(ui->r, small_f, count, x + PICK_CARD_W - 36 - count_w, y + 40, th->muted);
    }
    int scroll = pick_scroll_row(ui->overlay_item, st.pick_n);
    int vis0 = scroll * PICK_COLS;
    int vis1 = vis0 + PICK_COLS * PICK_VIS_ROWS;
    if (vis1 > st.pick_n) {
        vis1 = st.pick_n;
    }
    for (int i = vis0; i < vis1; i++) {
        int local = i - vis0;
        int col = local % PICK_COLS;
        int row = local / PICK_COLS;
        int px = g.sx + col * (PICK_SLOT + PICK_GAP);
        int py = g.sy + row * (PICK_SLOT + PICK_GAP);
        int on = (i == ui->overlay_item);
        dh_round_rect(ui->r, px, py, PICK_SLOT, PICK_SLOT, 14, on ? th->row_on : th->row);
        int bw = 0, bh = 0;
        SDL_Texture *tex = dh_sgdb_pick_icon(i, &bw, &bh);
        if (tex && bw > 0 && bh > 0) {
            dh_contain(ui->r, tex, bw, bh, px + 12, py + 12, PICK_SLOT - 24, PICK_SLOT - 40);
        }
        const char *lab = dh_sgdb_pick_label(i);
        if (lab && lab[0]) {
            int tw = dh_text_width(small_f, lab);
            dh_text(ui->r, small_f, lab, px + (PICK_SLOT - tw) / 2, py + PICK_SLOT - 26,
                on ? th->white : th->muted);
        }
        if (on) {
            dh_round_outline(ui->r, px, py, PICK_SLOT, PICK_SLOT, 14, th->accent, 3);
        }
    }
    dh_text(ui->r, small_f, ui->setup_on
        ? "A to apply · Y to skip · B to stop."
        : "Left: current tile · Right: marked cover. A to apply.",
        x + 36, y + PICK_CARD_H - 48, th->muted);
}

static void draw_overlays(DhUi *ui)
{
    if (ui->overlay == OV_NONE) {
        return;
    }
    const DhTheme *th = ui->th;
    draw_scrim(ui);
    TTF_Font *title_f = ui->fonts->large ? ui->fonts->large : ui->fonts->medium;
    TTF_Font *body_f = ui->fonts->medium;

    if (ui->overlay == OV_SGDB) {
        draw_sgdb_progress(ui);
        return;
    }
    if (ui->overlay == OV_SGDB_PICK) {
        draw_sgdb_pick(ui);
        return;
    }
    if (ui->overlay == OV_SGDB_FMT || ui->overlay == OV_SGDB_SKIP) {
        int x, y, ax, cx, by;
        confirm_geom(&x, &y, &ax, &cx, &by);
        dh_round_rect(ui->r, x, y, CONFIRM_W, CONFIRM_H, 18, th->panel);
        if (ui->overlay == OV_SGDB_FMT) {
            dh_text(ui->r, title_f, "Setup All Covers", x + 40, y + 36, th->white);
            dh_text(ui->r, body_f, "Which tile format?", x + 40, y + 100, th->text);
            dh_text(ui->r, ui->fonts->small ? ui->fonts->small : body_f,
                "Square: 1:1 icons · Cover: 600×900 grids", x + 40, y + 156, th->muted);
            draw_modal_btn(ui, ax, by, CONFIRM_BTN_W, CONFIRM_BTN_H, "Square", ui->overlay_btn == 0, 0);
            draw_modal_btn(ui, cx, by, CONFIRM_BTN_W, CONFIRM_BTN_H, "Cover", ui->overlay_btn == 1, 0);
        } else {
            dh_text(ui->r, title_f, "Skip replaced?", x + 40, y + 36, th->white);
            dh_text(ui->r, body_f, "Skip already replaced ones?", x + 40, y + 100, th->text);
            dh_text(ui->r, ui->fonts->small ? ui->fonts->small : body_f,
                "Yes keeps titles that already have this format.", x + 40, y + 156, th->muted);
            draw_modal_btn(ui, ax, by, CONFIRM_BTN_W, CONFIRM_BTN_H, "Yes", ui->overlay_btn == 0, 0);
            draw_modal_btn(ui, cx, by, CONFIRM_BTN_W, CONFIRM_BTN_H, "No", ui->overlay_btn == 1, 0);
        }
        return;
    }

    if (ui->overlay == OV_ITEM) {
        int card_w = 560;
        int n = item_menu_n(ui);
        int card_h = 72 * n + 48;
        int x = (DH_SCREEN_W - card_w) / 2;
        int y = (DH_SCREEN_H - card_h) / 2;
        dh_round_rect(ui->r, x, y, card_w, card_h, 16, th->panel);
        for (int i = 0; i < n; i++) {
            int ry = y + 24 + i * 72;
            int on = (i == ui->overlay_item);
            int id = item_id_at(ui, i);
            dh_round_rect(ui->r, x + 20, ry, card_w - 40, 64, 12, on ? th->row_on : th->row);
            if (on) {
                dh_round_rect(ui->r, x + 28, ry + 16, 6, 32, 3, th->accent);
            }
            int tht = 0;
            const char *lab = item_lab(id);
            dh_text_size(body_f, lab, &tht);
            SDL_Color c = (id == ITEM_UNINSTALL) ? th->danger : th->white;
            dh_text(ui->r, body_f, lab, x + 48, ry + (64 - tht) / 2, c);
        }
        return;
    }

    if (ui->overlay == OV_BT) {
#ifdef DH_ULAUNCH
        int n = ui->bt_discover ? 1 : 3;
        static const char *labs3[] = { "Connect", "Disconnect", "Unpair" };
        static const char *labs1[] = { "Connect" };
        const char **labs = ui->bt_discover ? labs1 : labs3;
        int card_w = 560;
        int card_h = 72 * n + 96;
        int x = (DH_SCREEN_W - card_w) / 2;
        int y = (DH_SCREEN_H - card_h) / 2;
        dh_round_rect(ui->r, x, y, card_w, card_h, 16, th->panel);
        dh_text(ui->r, title_f, ui->confirm_name[0] ? ui->confirm_name : "Device", x + 36, y + 24, th->white);
        for (int i = 0; i < n; i++) {
            int ry = y + 72 + i * 72;
            int on = (i == ui->overlay_item);
            dh_round_rect(ui->r, x + 20, ry, card_w - 40, 64, 12, on ? th->row_on : th->row);
            if (on) {
                dh_round_rect(ui->r, x + 28, ry + 16, 6, 32, 3, th->accent);
            }
            int tht = 0;
            dh_text_size(body_f, labs[i], &tht);
            dh_text(ui->r, body_f, labs[i], x + 48, ry + (64 - tht) / 2, th->white);
        }
#endif
        return;
    }

    if (ui->overlay == OV_INFO) {
        int card_w = 820;
        int card_h = 560;
        int x = (DH_SCREEN_W - card_w) / 2;
        int y = (DH_SCREEN_H - card_h) / 2 - 20;
        dh_round_rect(ui->r, x, y, card_w, card_h, 16, th->panel);
        dh_text(ui->r, title_f, "Info", x + 36, y + 28, th->white);
        int ly = y + 88;
        const char *p = ui->info_body;
        while (*p && ly < y + card_h - 40) {
            char line[160];
            int n = 0;
            while (*p && *p != '\n' && n < (int)sizeof(line) - 1) {
                line[n++] = *p++;
            }
            line[n] = 0;
            if (*p == '\n') {
                p++;
            }
            dh_text(ui->r, body_f, line[0] ? line : " ", x + 36, ly, th->text);
            ly += 36;
        }
        return;
    }

#ifndef DH_ULAUNCH
    if (ui->overlay == OV_MTP) {
        int card_w = 720;
        int card_h = 320;
        int x = (DH_SCREEN_W - card_w) / 2;
        int y = (DH_SCREEN_H - card_h) / 2 - 24;
        dh_round_rect(ui->r, x, y, card_w, card_h, 18, th->panel);
        dh_text(ui->r, title_f, "USB MTP", x + 40, y + 36, th->white);
        dh_text(ui->r, body_f, "Connect a USB-C cable to this Switch.", x + 40, y + 108, th->text);
        dh_text(ui->r, ui->fonts->small ? ui->fonts->small : body_f,
            "Windows: device Switch, storage 1: SD Card.", x + 40, y + 156, th->muted);
        char st[160];
        st[0] = 0;
        dh_mtp_status(st, sizeof(st));
        if (st[0]) {
            dh_text(ui->r, body_f, st, x + 40, y + 214, th->white);
        }
        return;
    }
#endif

    int x, y, ax, cx, by;
    confirm_geom(&x, &y, &ax, &cx, &by);
    dh_round_rect(ui->r, x, y, CONFIRM_W, CONFIRM_H, 18, th->panel);
    const char *title = "Close software?";
    const char *sub = "Unsaved progress will be lost.";
    const char *yes = "Close";
    int danger = 0;
    if (ui->overlay == OV_CLOSE) {
        title = "Switch software?";
        sub = ui->pending_hb[0]
            ? "Close the running title and start homebrew."
            : "Close the running title and start the selected game.";
        yes = "Switch";
    } else if (ui->overlay == OV_UN1) {
        title = "Uninstall?";
        sub = "This cannot be undone.";
        yes = "Continue";
        danger = 1;
    } else if (ui->overlay == OV_UN2) {
        title = "Really uninstall?";
        sub = "All data for this title will be deleted.";
        yes = "Uninstall";
        danger = 1;
    }
    dh_text(ui->r, title_f, title, x + 40, y + 36, th->white);
    dh_text(ui->r, body_f, ui->confirm_name, x + 40, y + 100, th->text);
    dh_text(ui->r, ui->fonts->small ? ui->fonts->small : body_f, sub, x + 40, y + 156, th->muted);
    draw_modal_btn(ui, ax, by, CONFIRM_BTN_W, CONFIRM_BTN_H, yes, ui->overlay_btn == 0, danger);
    draw_modal_btn(ui, cx, by, CONFIRM_BTN_W, CONFIRM_BTN_H, "Cancel", ui->overlay_btn == 1, 0);
}

void dh_ui_draw(DhUi *ui, SDL_Renderer *r)
{
    (void)r;
    const DhTheme *th = ui->th;
    SDL_SetRenderDrawColor(ui->r, th->bg.r, th->bg.g, th->bg.b, 255);
    SDL_RenderClear(ui->r);

    if (!ui->options) {
        draw_grid(ui);
        draw_topbar(ui);
        draw_tabs(ui);
    } else {
        draw_topbar(ui);
        draw_options(ui);
    }
    draw_footer(ui);
    draw_sidebar(ui);
    draw_overlays(ui);

    if (ui->toast_left > 0) {
        int tw = dh_text_width(ui->fonts->medium, ui->toast) + 48;
        int tx = (DH_SCREEN_W - tw) / 2;
        dh_round_rect(ui->r, tx, 900, tw, 44, 12, th->card);
        dh_text(ui->r, ui->fonts->medium, ui->toast, tx + 24, 908, th->text);
    }
}
