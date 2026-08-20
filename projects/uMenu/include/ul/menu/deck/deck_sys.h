#pragma once

#include <switch.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DH_SYS_LOCKSCREEN = 0,
    DH_SYS_WLAN,
    DH_SYS_BT,
    DH_SYS_NFC,
    DH_SYS_USB30,
    DH_SYS_CAPTURE,
    DH_SYS_HB_APP,
    DH_SYS_AUTO_UPD,
    DH_SYS_AUTO_DL,
    DH_SYS_INFO_UP,
    DH_SYS_SLEEP_MEDIA,
    DH_SYS_SLEEP_WAKE
};

enum {
    DH_SYS_STR_SSID = 0,
    DH_SYS_STR_IP,
    DH_SYS_STR_MAC,
    DH_SYS_STR_NICK,
    DH_SYS_STR_LANG,
    DH_SYS_STR_FW,
    DH_SYS_STR_AMS,
    DH_SYS_STR_EMUMMC,
    DH_SYS_STR_REGION,
    DH_SYS_STR_TZ,
    DH_SYS_STR_SERIAL,
    DH_SYS_STR_BATTERY,
    DH_SYS_STR_AUDSVC,
    DH_SYS_STR_THEME,
    DH_SYS_STR_TAKEOVER,
    DH_SYS_STR_SLEEP_HH,
    DH_SYS_STR_SLEEP_DOCK,
    DH_SYS_STR_VERSION
};

enum {
    DH_SYS_BT_CONNECT = 0,
    DH_SYS_BT_DISCONNECT,
    DH_SYS_BT_UNPAIR
};

#define DH_SYS_BT_MAX 16

typedef struct {
    char name[64];
    int connected;
} DhSysBtDev;

void deck_sys_request_overlay(void);
int deck_sys_take_overlay(void);

int deck_sys_bool(int id);
int deck_sys_bool_set(int id, int on);
int deck_sys_bool_toggle(int id);

int deck_sys_copy(int id, char *buf, size_t n);

int deck_sys_album_get(void);
void deck_sys_album_set(int sd_nand);

int deck_sys_sleep_get(int dock);
int deck_sys_sleep_set(int dock, int index);
void deck_sys_sleep_copy(int dock, char *buf, size_t n);

void deck_sys_open_wifi(void);
void deck_sys_open_themes(void);
int deck_sys_edit_nick(void);
int deck_sys_pick_lang(void);
int deck_sys_reset_takeover(void);
int deck_sys_set_takeover(u64 app_id);
u64 deck_sys_takeover_id(void);

void deck_sys_bt_discover_start(void);
void deck_sys_bt_discover_stop(void);
int deck_sys_bt_list(int discover, DhSysBtDev *out, int max);
int deck_sys_bt_pick(int discover, int index);
int deck_sys_bt_act(int act);
int deck_sys_bt_poll(void);

#ifdef __cplusplus
}
#endif
