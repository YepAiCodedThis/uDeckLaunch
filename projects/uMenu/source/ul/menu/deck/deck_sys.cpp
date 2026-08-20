#include <ul/menu/deck/deck_sys.h>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <ul/menu/bt/bt_Manager.hpp>
#include <ul/net/net_Service.hpp>
#include <ul/os/os_System.hpp>
#include <ul/os/os_Applications.hpp>
#include <ul/util/util_String.hpp>
#include <cstring>
#include <cstdio>
#include <vector>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace {

    constexpr auto SleepFlag_SleepsWhilePlayingMedia = BIT(0);
    constexpr auto SleepFlag_WakesAtPowerStateChange = BIT(1);

    int g_want_overlay = 0;
    BtmAudioDevice g_bt_pick = {};
    std::vector<BtmAudioDevice> g_bt_cache;

    const char *RegionName(const SetRegion region) {
        switch(region) {
            case SetRegion_JPN: return "Japan";
            case SetRegion_USA: return "Americas";
            case SetRegion_EUR: return "Europe";
            case SetRegion_AUS: return "Australia";
            case SetRegion_HTK: return "Hong Kong / Taiwan / Korea";
            case SetRegion_CHN: return "China";
            default: return "Unknown";
        }
    }

    const char *HandheldSleepName(int index) {
        switch(index) {
            case 0: return "Never";
            case 1: return "1 min";
            case 2: return "3 min";
            case 3: return "5 min";
            case 4: return "10 min";
            case 5: return "30 min";
            default: return "Unknown";
        }
    }

    const char *ConsoleSleepName(int index) {
        switch(index) {
            case 0: return "Never";
            case 1: return "1 hour";
            case 2: return "2 hours";
            case 3: return "3 hours";
            case 4: return "6 hours";
            case 5: return "12 hours";
            default: return "Unknown";
        }
    }

    int HandheldIndex(SetSysHandheldSleepPlan plan) {
        switch(plan) {
            case SetSysHandheldSleepPlan_Never: return 0;
            case SetSysHandheldSleepPlan_1Min: return 1;
            case SetSysHandheldSleepPlan_3Min: return 2;
            case SetSysHandheldSleepPlan_5Min: return 3;
            case SetSysHandheldSleepPlan_10Min: return 4;
            case SetSysHandheldSleepPlan_30Min: return 5;
            default: return 0;
        }
    }

    SetSysHandheldSleepPlan HandheldPlan(int index) {
        switch(index) {
            case 1: return SetSysHandheldSleepPlan_1Min;
            case 2: return SetSysHandheldSleepPlan_3Min;
            case 3: return SetSysHandheldSleepPlan_5Min;
            case 4: return SetSysHandheldSleepPlan_10Min;
            case 5: return SetSysHandheldSleepPlan_30Min;
            default: return SetSysHandheldSleepPlan_Never;
        }
    }

    int ConsoleIndex(SetSysConsoleSleepPlan plan) {
        switch(plan) {
            case SetSysConsoleSleepPlan_Never: return 0;
            case SetSysConsoleSleepPlan_1Hour: return 1;
            case SetSysConsoleSleepPlan_2Hour: return 2;
            case SetSysConsoleSleepPlan_3Hour: return 3;
            case SetSysConsoleSleepPlan_6Hour: return 4;
            case SetSysConsoleSleepPlan_12Hour: return 5;
            default: return 0;
        }
    }

    SetSysConsoleSleepPlan ConsolePlan(int index) {
        switch(index) {
            case 1: return SetSysConsoleSleepPlan_1Hour;
            case 2: return SetSysConsoleSleepPlan_2Hour;
            case 3: return SetSysConsoleSleepPlan_3Hour;
            case 4: return SetSysConsoleSleepPlan_6Hour;
            case 5: return SetSysConsoleSleepPlan_12Hour;
            default: return SetSysConsoleSleepPlan_Never;
        }
    }

    bool GetCfgBool(ul::cfg::ConfigEntryId id) {
        bool v = false;
        if(!g_GlobalSettings.config.GetEntry(id, v)) {
            return false;
        }
        return v;
    }

    int SetCfgBool(ul::cfg::ConfigEntryId id, bool v) {
        if(!g_GlobalSettings.config.SetEntry(id, v)) {
            return 0;
        }
        g_GlobalSettings.SaveConfig();
        return 1;
    }

}

extern "C" {

void deck_sys_request_overlay(void) {
    g_want_overlay = 1;
}

int deck_sys_take_overlay(void) {
    const int v = g_want_overlay;
    g_want_overlay = 0;
    return v;
}

int deck_sys_bool(int id) {
    switch(id) {
        case DH_SYS_LOCKSCREEN:
            return GetCfgBool(ul::cfg::ConfigEntryId::LockscreenEnabled) ? 1 : 0;
        case DH_SYS_WLAN:
            return g_GlobalSettings.wireless_lan_enabled ? 1 : 0;
        case DH_SYS_BT:
            return g_GlobalSettings.bluetooth_enabled ? 1 : 0;
        case DH_SYS_NFC:
            return g_GlobalSettings.nfc_enabled ? 1 : 0;
        case DH_SYS_USB30:
            return g_GlobalSettings.usb30_enabled ? 1 : 0;
        case DH_SYS_CAPTURE:
            return GetCfgBool(ul::cfg::ConfigEntryId::UsbScreenCaptureEnabled) ? 1 : 0;
        case DH_SYS_HB_APP:
            return GetCfgBool(ul::cfg::ConfigEntryId::LaunchHomebrewApplicationByDefault) ? 1 : 0;
        case DH_SYS_AUTO_UPD:
            return g_GlobalSettings.auto_update_enabled ? 1 : 0;
        case DH_SYS_AUTO_DL:
            return g_GlobalSettings.auto_app_download_enabled ? 1 : 0;
        case DH_SYS_INFO_UP:
            return g_GlobalSettings.console_info_upload_enabled ? 1 : 0;
        case DH_SYS_SLEEP_MEDIA:
            return (g_GlobalSettings.sleep_settings.flags & SleepFlag_SleepsWhilePlayingMedia) ? 1 : 0;
        case DH_SYS_SLEEP_WAKE:
            return (g_GlobalSettings.sleep_settings.flags & SleepFlag_WakesAtPowerStateChange) ? 1 : 0;
        default:
            return 0;
    }
}

int deck_sys_bool_set(int id, int on) {
    const bool v = on != 0;
    switch(id) {
        case DH_SYS_LOCKSCREEN:
            return SetCfgBool(ul::cfg::ConfigEntryId::LockscreenEnabled, v);
        case DH_SYS_WLAN:
            g_GlobalSettings.wireless_lan_enabled = v;
            setsysSetWirelessLanEnableFlag(v);
            return 1;
        case DH_SYS_BT:
            g_GlobalSettings.bluetooth_enabled = v;
            setsysSetBluetoothEnableFlag(v);
            return 1;
        case DH_SYS_NFC:
            g_GlobalSettings.nfc_enabled = v;
            setsysSetNfcEnableFlag(v);
            return 1;
        case DH_SYS_USB30:
            g_GlobalSettings.usb30_enabled = v;
            setsysSetUsb30EnableFlag(v);
            return 1;
        case DH_SYS_CAPTURE: {
            if(SetCfgBool(ul::cfg::ConfigEntryId::UsbScreenCaptureEnabled, v) == 0) {
                return 0;
            }
            return 2;
        }
        case DH_SYS_HB_APP:
            return SetCfgBool(ul::cfg::ConfigEntryId::LaunchHomebrewApplicationByDefault, v);
        case DH_SYS_AUTO_UPD:
            g_GlobalSettings.auto_update_enabled = v;
            setsysSetAutoUpdateEnableFlag(v);
            return 1;
        case DH_SYS_AUTO_DL:
            g_GlobalSettings.auto_app_download_enabled = v;
            setsysSetAutomaticApplicationDownloadFlag(v);
            return 1;
        case DH_SYS_INFO_UP:
            g_GlobalSettings.console_info_upload_enabled = v;
            setsysSetConsoleInformationUploadFlag(v);
            return 1;
        case DH_SYS_SLEEP_MEDIA:
            if(v) {
                g_GlobalSettings.sleep_settings.flags |= SleepFlag_SleepsWhilePlayingMedia;
            }
            else {
                g_GlobalSettings.sleep_settings.flags &= ~SleepFlag_SleepsWhilePlayingMedia;
            }
            g_GlobalSettings.UpdateSleepSettings();
            return 1;
        case DH_SYS_SLEEP_WAKE:
            if(v) {
                g_GlobalSettings.sleep_settings.flags |= SleepFlag_WakesAtPowerStateChange;
            }
            else {
                g_GlobalSettings.sleep_settings.flags &= ~SleepFlag_WakesAtPowerStateChange;
            }
            g_GlobalSettings.UpdateSleepSettings();
            return 1;
        default:
            return 0;
    }
}

int deck_sys_bool_toggle(int id) {
    return deck_sys_bool_set(id, !deck_sys_bool(id));
}

int deck_sys_copy(int id, char *buf, size_t n) {
    if((buf == nullptr) || (n == 0)) {
        return -1;
    }
    buf[0] = '\0';
    switch(id) {
        case DH_SYS_STR_SSID: {
            NifmNetworkProfileData prof = {};
            if(R_SUCCEEDED(nifmGetCurrentNetworkProfile(&prof)) && prof.wireless_setting_data.ssid[0]) {
                std::snprintf(buf, n, "%s", prof.wireless_setting_data.ssid);
            }
            else {
                std::snprintf(buf, n, "Not connected");
            }
            return 0;
        }
        case DH_SYS_STR_IP: {
            const auto ip = ul::net::GetConsoleIpAddress();
            std::snprintf(buf, n, "%s", ip.empty() ? "—" : ip.c_str());
            return 0;
        }
        case DH_SYS_STR_MAC: {
            ul::net::WlanMacAddress mac = {};
            if(R_SUCCEEDED(ul::net::GetMacAddress(mac))) {
                const auto s = ul::net::FormatMacAddress(mac);
                std::snprintf(buf, n, "%s", s.c_str());
            }
            else {
                std::snprintf(buf, n, "—");
            }
            return 0;
        }
        case DH_SYS_STR_NICK:
            std::snprintf(buf, n, "%s", g_GlobalSettings.nickname.nickname);
            return 0;
        case DH_SYS_STR_LANG: {
            const auto i = static_cast<u32>(g_GlobalSettings.language);
            if(i < ul::os::LanguageNameCount) {
                std::snprintf(buf, n, "%s", ul::os::LanguageNameList[i]);
            }
            return 0;
        }
        case DH_SYS_STR_FW:
            std::snprintf(buf, n, "%s", g_GlobalSettings.fw_version.display_version);
            return 0;
        case DH_SYS_STR_AMS: {
            const auto s = g_GlobalSettings.ams_version.Format();
            std::snprintf(buf, n, "%s", s.c_str());
            return 0;
        }
        case DH_SYS_STR_EMUMMC:
            std::snprintf(buf, n, "%s", g_GlobalSettings.ams_is_emummc ? "Yes" : "No");
            return 0;
        case DH_SYS_STR_REGION:
            std::snprintf(buf, n, "%s", RegionName(g_GlobalSettings.region));
            return 0;
        case DH_SYS_STR_TZ:
            std::snprintf(buf, n, "%s", g_GlobalSettings.timezone.name);
            return 0;
        case DH_SYS_STR_SERIAL:
            std::snprintf(buf, n, "%s", g_GlobalSettings.serial_no.number);
            return 0;
        case DH_SYS_STR_BATTERY:
            std::snprintf(buf, n, "%s", g_GlobalSettings.battery_lot.lot);
            return 0;
        case DH_SYS_STR_AUDSVC:
            if(serviceIsActive(audrenGetServiceSession_AudioRenderer())) {
                std::snprintf(buf, n, "audren");
            }
            else if(serviceIsActive(audoutGetServiceSession_AudioOut())) {
                std::snprintf(buf, n, "audout");
            }
            else {
                std::snprintf(buf, n, "—");
            }
            return 0;
        case DH_SYS_STR_THEME:
            std::snprintf(buf, n, "%s", g_GlobalSettings.active_theme.name.empty() ? "Default" : g_GlobalSettings.active_theme.name.c_str());
            return 0;
        case DH_SYS_STR_TAKEOVER: {
            const auto app_id = g_GlobalSettings.cache_hb_takeover_app_id;
            if(app_id == ul::os::InvalidApplicationId) {
                std::snprintf(buf, n, "None");
            }
            else {
                const auto hex = ul::util::FormatProgramId(app_id);
                std::snprintf(buf, n, "%s", hex.c_str());
            }
            return 0;
        }
        case DH_SYS_STR_SLEEP_HH:
            std::snprintf(buf, n, "%s", HandheldSleepName(HandheldIndex(static_cast<SetSysHandheldSleepPlan>(g_GlobalSettings.sleep_settings.handheld_sleep_plan))));
            return 0;
        case DH_SYS_STR_SLEEP_DOCK:
            std::snprintf(buf, n, "%s", ConsoleSleepName(ConsoleIndex(static_cast<SetSysConsoleSleepPlan>(g_GlobalSettings.sleep_settings.console_sleep_plan))));
            return 0;
        case DH_SYS_STR_VERSION:
            std::snprintf(buf, n, "%s", UL_VERSION);
            return 0;
        default:
            return -1;
    }
}

int deck_sys_album_get(void) {
    return (g_GlobalSettings.album_storage == SetSysPrimaryAlbumStorage_Nand) ? 1 : 0;
}

void deck_sys_album_set(int sd_nand) {
    g_GlobalSettings.album_storage = (sd_nand != 0) ? SetSysPrimaryAlbumStorage_Nand : SetSysPrimaryAlbumStorage_SdCard;
    setsysSetPrimaryAlbumStorage(g_GlobalSettings.album_storage);
}

int deck_sys_sleep_get(int dock) {
    if(dock) {
        return ConsoleIndex(static_cast<SetSysConsoleSleepPlan>(g_GlobalSettings.sleep_settings.console_sleep_plan));
    }
    return HandheldIndex(static_cast<SetSysHandheldSleepPlan>(g_GlobalSettings.sleep_settings.handheld_sleep_plan));
}

int deck_sys_sleep_set(int dock, int index) {
    if(index < 0) {
        index = 0;
    }
    if(index > 5) {
        index = 5;
    }
    if(dock) {
        g_GlobalSettings.sleep_settings.console_sleep_plan = ConsolePlan(index);
    }
    else {
        g_GlobalSettings.sleep_settings.handheld_sleep_plan = HandheldPlan(index);
    }
    g_GlobalSettings.UpdateSleepSettings();
    return index;
}

void deck_sys_sleep_copy(int dock, char *buf, size_t n) {
    deck_sys_copy(dock ? DH_SYS_STR_SLEEP_DOCK : DH_SYS_STR_SLEEP_HH, buf, n);
}

void deck_sys_open_wifi(void) {
    ul::menu::ui::ShowNetConnect();
}

void deck_sys_open_themes(void) {
    ul::menu::ui::ShowThemesMenu();
}

int deck_sys_edit_nick(void) {
    SwkbdConfig swkbd;
    if(R_FAILED(swkbdCreate(&swkbd, 0))) {
        return -1;
    }
    swkbdConfigMakePresetDefault(&swkbd);
    swkbdConfigSetType(&swkbd, SwkbdType_All);
    swkbdConfigSetGuideText(&swkbd, ul::menu::ui::GetLanguageString("swkbd_console_nick_guide").c_str());
    swkbdConfigSetInitialText(&swkbd, g_GlobalSettings.nickname.nickname);
    swkbdConfigSetStringLenMax(&swkbd, 32);
    SetSysDeviceNickName new_name = {};
    const auto rc = ul::menu::ui::ShowSwkbd(&swkbd, new_name.nickname, sizeof(new_name.nickname));
    swkbdClose(&swkbd);
    if(R_FAILED(rc) || (new_name.nickname[0] == '\0')) {
        return 0;
    }
    g_GlobalSettings.nickname = new_name;
    setsysSetDeviceNickname(&g_GlobalSettings.nickname);
    return 1;
}

int deck_sys_pick_lang(void) {
    std::vector<std::string> opts;
    for(u32 i = 0; i < ul::os::LanguageNameCount; i++) {
        opts.push_back(ul::os::LanguageNameList[i]);
    }
    opts.push_back(ul::menu::ui::GetLanguageString("cancel"));
    const auto opt = g_MenuApplication->DisplayDialog(
        ul::menu::ui::GetLanguageString("set_lang_select"),
        ul::menu::ui::GetLanguageString("set_lang_conf"),
        opts, true);
    if((opt < 0) || (opt >= static_cast<int>(ul::os::LanguageNameCount))) {
        return 0;
    }
    if(ul::os::GetSystemLanguage() == opt) {
        g_MenuApplication->ShowNotification(ul::menu::ui::GetLanguageString("set_lang_active"));
        return 0;
    }
    const auto lang_code = g_GlobalSettings.available_language_codes[opt];
    const auto rc = setsysSetLanguageCode(lang_code);
    g_MenuApplication->DisplayDialog(
        ul::menu::ui::GetLanguageString("set_lang"),
        R_SUCCEEDED(rc) ? ul::menu::ui::GetLanguageString("set_lang_select_ok") : (ul::menu::ui::GetLanguageString("set_lang_select_error") + ": " + ul::util::FormatResultDisplay(rc)),
        { ul::menu::ui::GetLanguageString("ok") }, true);
    if(R_SUCCEEDED(rc)) {
        ul::menu::ui::RebootSystem();
        return 1;
    }
    return 0;
}

int deck_sys_reset_takeover(void) {
    if(g_GlobalSettings.cache_hb_takeover_app_id == ul::os::InvalidApplicationId) {
        return 0;
    }
    g_GlobalSettings.ResetHomebrewTakeoverApplicationId();
    return 1;
}

int deck_sys_set_takeover(u64 app_id) {
    if(app_id == ul::os::InvalidApplicationId) {
        return -1;
    }
    g_GlobalSettings.SetHomebrewTakeoverApplicationId(app_id);
    return 1;
}

u64 deck_sys_takeover_id(void) {
    return g_GlobalSettings.cache_hb_takeover_app_id;
}

void deck_sys_bt_discover_start(void) {
    ul::menu::bt::HasDiscoveredAudioDeviceChanges();
    ul::menu::bt::StartAudioDeviceDiscovery();
}

void deck_sys_bt_discover_stop(void) {
    ul::menu::bt::StopAudioDeviceDiscovery();
}

int deck_sys_bt_list(int discover, DhSysBtDev *out, int max) {
    if((out == nullptr) || (max < 1)) {
        return 0;
    }
    g_bt_cache = discover ? ul::menu::bt::ListDiscoveredAudioDevices() : ul::menu::bt::ListPairedAudioDevices();
    const auto connected = ul::menu::bt::GetConnectedAudioDevice();
    int n = static_cast<int>(g_bt_cache.size());
    if(n > max) {
        n = max;
    }
    for(int i = 0; i < n; i++) {
        std::snprintf(out[i].name, sizeof(out[i].name), "%s", g_bt_cache[static_cast<size_t>(i)].name);
        if(out[i].name[0] == '\0') {
            std::snprintf(out[i].name, sizeof(out[i].name), "Audio device");
        }
        out[i].connected = ul::menu::bt::AudioDevicesEqual(g_bt_cache[static_cast<size_t>(i)], connected) ? 1 : 0;
    }
    return n;
}

int deck_sys_bt_pick(int discover, int index) {
    const auto list = discover ? ul::menu::bt::ListDiscoveredAudioDevices() : ul::menu::bt::ListPairedAudioDevices();
    if((index < 0) || (index >= static_cast<int>(list.size()))) {
        return -1;
    }
    g_bt_pick = list[static_cast<size_t>(index)];
    return 0;
}

int deck_sys_bt_act(int act) {
    Result rc = 0;
    if(act == DH_SYS_BT_CONNECT) {
        rc = ul::menu::bt::ConnectAudioDevice(g_bt_pick);
    }
    else if(act == DH_SYS_BT_DISCONNECT) {
        rc = ul::menu::bt::DisconnectAudioDevice(g_bt_pick);
    }
    else if(act == DH_SYS_BT_UNPAIR) {
        rc = ul::menu::bt::UnpairAudioDevice(g_bt_pick);
    }
    else {
        return -1;
    }
    return R_SUCCEEDED(rc) ? 0 : static_cast<int>(rc);
}

int deck_sys_bt_poll(void) {
    const int a = ul::menu::bt::HasPairedAudioDeviceChanges() ? 1 : 0;
    const int b = ul::menu::bt::HasConnectedAudioDeviceChanges() ? 1 : 0;
    const int c = ul::menu::bt::HasDiscoveredAudioDeviceChanges() ? 1 : 0;
    return a || b || c;
}

}
