#include <ul/menu/deck/deck_smi.h>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <ul/menu/smi/smi_Commands.hpp>
#include <ul/ul_Result.hpp>
#include <ul/os/os_Applications.hpp>
#include <cstring>
#include <cstdio>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace {

    int FinishLaunch(const Result rc) {
        if(R_SUCCEEDED(rc)) {
            g_MenuApplication->Finalize();
            return 0;
        }
        g_MenuApplication->FadeIn();
        g_MenuApplication->ResetFade();
        return static_cast<int>(rc);
    }

}

extern "C" {

int deck_smi_launch_app(u64 app_id) {
    g_MenuApplication->FadeOutToNonLibraryApplet();
    return FinishLaunch(ul::menu::smi::LaunchApplication(app_id));
}

int deck_smi_resume_app(void) {
    return static_cast<int>(ul::menu::smi::ResumeApplication());
}

int deck_smi_terminate_app(void) {
    const auto rc = ul::menu::smi::TerminateApplication();
    if(R_SUCCEEDED(rc)) {
        g_GlobalSettings.ResetSuspendedApplication();
    }
    return static_cast<int>(rc);
}

u64 deck_smi_get_hb_takeover(void) {
    return g_GlobalSettings.cache_hb_takeover_app_id;
}

int deck_smi_ensure_hb_takeover(u64 app_id) {
    if(g_GlobalSettings.cache_hb_takeover_app_id != ul::os::InvalidApplicationId) {
        return 0;
    }
    if(app_id == ul::os::InvalidApplicationId) {
        return -1;
    }
    g_GlobalSettings.SetHomebrewTakeoverApplicationId(app_id);
    return 0;
}

int deck_smi_launch_hb_app(const char *path) {
    if((path == nullptr) || (path[0] == '\0')) {
        return -1;
    }
    if(g_GlobalSettings.cache_hb_takeover_app_id == ul::os::InvalidApplicationId) {
        return static_cast<int>(ul::ResultNoHomebrewTakeoverApplication);
    }
    g_MenuApplication->FadeOutToNonLibraryApplet();
    return FinishLaunch(ul::menu::smi::LaunchHomebrewApplication(path, path));
}

int deck_smi_launch_hb_applet(const char *path) {
    if((path == nullptr) || (path[0] == '\0')) {
        return -1;
    }
    g_MenuApplication->FadeOutToNonLibraryApplet();
    return FinishLaunch(ul::menu::smi::LaunchHomebrewLibraryApplet(path, path));
}

int deck_smi_is_suspended(u64 *out_app_id, char *out_hb, size_t hb_len) {
    if(out_app_id != nullptr) {
        *out_app_id = 0;
    }
    if((out_hb != nullptr) && (hb_len > 0)) {
        out_hb[0] = '\0';
    }
    if(g_GlobalSettings.IsTitleSuspended()) {
        if(out_app_id != nullptr) {
            *out_app_id = g_GlobalSettings.system_status.suspended_app_id;
        }
        return 1;
    }
    if(g_GlobalSettings.IsHomebrewSuspended()) {
        if((out_hb != nullptr) && (hb_len > 0)) {
            std::snprintf(out_hb, hb_len, "%s", g_GlobalSettings.system_status.suspended_hb_target_ipt.nro_path);
        }
        if(out_app_id != nullptr) {
            *out_app_id = 1;
        }
        return 1;
    }
    return 0;
}

int deck_smi_sleep(void) {
    ul::menu::ui::SleepSystem();
    return 0;
}

int deck_smi_reboot(void) {
    ul::menu::ui::RebootSystem();
    return 0;
}

int deck_smi_power_off(void) {
    ul::menu::ui::ShutdownSystem();
    return 0;
}

int deck_smi_open_album(void) {
    ul::menu::ui::ShowAlbum();
    return 0;
}

void deck_smi_request_settings(void) {
    ul::menu::ui::ShowSettingsMenu();
}

}
