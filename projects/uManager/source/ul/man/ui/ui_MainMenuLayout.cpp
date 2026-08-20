#include <ul/man/ui/ui_MainApplication.hpp>
#include <ul/menu/menu_Cache.hpp>
#include <ul/menu/menu_Entries.hpp>
#include <ul/acc/acc_Accounts.hpp>
#include <ul/util/util_Json.hpp>
#include <ul/man/man_Manager.hpp>
#include <ul/man/man_Install.hpp>
#include <ul/cfg/cfg_Config.hpp>
#include <ul/os/os_Applications.hpp>
#include <ul/os/os_System.hpp>

extern ul::man::ui::MainApplication::Ref g_MainApplication;
extern ul::util::JSON g_DefaultLanguage;
extern ul::util::JSON g_MainLanguage;

namespace ul::man::ui {

    namespace {

        inline std::string GetLanguageString(const std::string &name) {
            return cfg::GetLanguageString(g_MainLanguage, g_DefaultLanguage, name);
        }

        inline std::string GetStatus() {
            std::string status = GetLanguageString("status") + ": ";
            if(IsBasePresent()) {
                if(IsSystemActive()) {
                    status += GetLanguageString("status_active");
                }
                else {
                    status += GetLanguageString("status_not_active");
                }
            }
            else {
                status += GetLanguageString("status_not_present");
            }

            return status;
        }

        s32 DeckDialog(const std::string &title, const std::string &content, const std::vector<std::string> &opts, const bool cancel_last) {
            return g_MainApplication->CreateShowDialog(title, content, opts, cancel_last, {}, [](pu::ui::Dialog::Ref &d) {
                d->SetTitleColor({ 0xDC, 0xDE, 0xDF, 0xFF });
                d->SetContentColor({ 0x8B, 0x92, 0x9A, 0xFF });
                d->SetOptionColor({ 0xDC, 0xDE, 0xDF, 0xFF });
                d->SetDialogColor({ 0x16, 0x1B, 0x22, 0xFF });
                d->SetOverColor({ 0x1A, 0x9F, 0xFF, 0xFF });
            });
        }

        inline void RebootSystem() {
            UL_RC_ASSERT(spsmInitialize());
            spsmShutdown(true);
        }

        inline void ShowInstallError() {
            DeckDialog(GetLanguageString("install_title"), GetLanguageString("install_error"), { GetLanguageString("ok") }, true);
        }

    }

    void MainMenuLayout::ResetInfoText() {
        std::string info = "uDeckLaunch v" UL_VERSION;
        if(g_MainApplication->IsAvailable()) {
            const auto ver = g_MainApplication->GetVersion();
            info += "  ·  HOME " + std::to_string((u32)ver.major) + "." + std::to_string((u32)ver.minor) + "." + std::to_string((u32)ver.micro);
            if(!g_MainApplication->IsVersionMatch()) {
                info += " (unexpected version)";
            }
        }
        this->info_text->SetText(info);
    }

    MainMenuLayout::MainMenuLayout() : pu::ui::Layout() {
        this->SetBackgroundColor(BackgroundColor);

        auto logo_tex = pu::sdl2::TextureHandle::New(pu::ui::render::LoadImageFromFile("romfs:/Logo.png"));
        this->logo = pu::ui::elm::Image::New(0, (s32)LogoY, logo_tex);
        if(this->logo->IsImageValid()) {
            const auto nw = this->logo->GetWidth();
            const auto nh = this->logo->GetHeight();
            this->logo->SetHeight(LogoH);
            if(nh > 0) {
                this->logo->SetWidth((nw * (s32)LogoH) / nh);
            }
            this->logo->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
            this->Add(this->logo);
        }

        this->title_text = pu::ui::elm::TextBlock::New(0, TitleY, "uDeckLaunch");
        this->title_text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
        this->title_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->title_text->SetColor(TitleColor);
        this->Add(this->title_text);

        this->info_text = pu::ui::elm::TextBlock::New(0, InfoTextY, "...");
        this->ResetInfoText();
        this->info_text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
        this->info_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->info_text->SetColor(InfoTextColor);
        this->Add(this->info_text);

        this->update_download_bar = pu::ui::elm::ProgressBar::New(0, UpdateDownloadBarY, UpdateDownloadBarWidth, UpdateDownloadBarHeight, 1.0f);
        this->update_download_bar->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->update_download_bar->SetVisible(false);
        this->Add(this->update_download_bar);

        this->options_menu = pu::ui::elm::Menu::New(MenuX, MenuY, MenuW, MenuColor, MenuFocusColor, MenuItemSize, MenuItemCount);

        this->activate_menu_item = pu::ui::elm::MenuItem::New(GetStatus());
        this->activate_menu_item->SetColor(MenuItemColor);
        this->activate_menu_item->AddOnKey(std::bind(&MainMenuLayout::activate_DefaultKey, this));
        this->options_menu->AddItem(this->activate_menu_item);

        this->reset_menu_menu_item = pu::ui::elm::MenuItem::New(GetLanguageString("reset_menu_item"));
        this->reset_menu_menu_item->SetColor(MenuItemColor);
        this->reset_menu_menu_item->AddOnKey(std::bind(&MainMenuLayout::resetMenu_DefaultKey, this));
        this->options_menu->AddItem(this->reset_menu_menu_item);

        this->reset_cache_menu_item = pu::ui::elm::MenuItem::New(GetLanguageString("reset_cache_item"));
        this->reset_cache_menu_item->SetColor(MenuItemColor);
        this->reset_cache_menu_item->AddOnKey(std::bind(&MainMenuLayout::resetCache_DefaultKey, this));
        this->options_menu->AddItem(this->reset_cache_menu_item);

        // No public update channel yet.
        // this->update_menu_item = pu::ui::elm::MenuItem::New(GetLanguageString("update_item"));
        // this->update_menu_item->SetColor(MenuItemColor);
        // this->update_menu_item->AddOnKey(std::bind(&MainMenuLayout::update_DefaultKey, this));
        // this->options_menu->AddItem(this->update_menu_item);

        this->Add(this->options_menu);
    }

    bool MainMenuLayout::RunEmbeddedInstall() {
        this->options_menu->SetVisible(false);
        this->update_download_bar->SetVisible(true);
        this->update_download_bar->SetProgress(0);
        this->info_text->SetText(GetLanguageString("install_progress"));

        const auto ok = man::InstallAndActivate([&](const double done, const double total) {
            this->update_download_bar->SetMaxProgress(total);
            this->update_download_bar->SetProgress(done);
            g_MainApplication->CallForRender();
        });

        this->update_download_bar->SetVisible(false);
        this->options_menu->SetVisible(true);
        this->activate_menu_item->SetName(GetStatus());
        this->ReloadMenu();
        this->ResetInfoText();

        return ok;
    }

    void MainMenuLayout::activate_DefaultKey() {
        if(!IsBasePresent()) {
            if(man::HasEmbeddedPayload()) {
                if(this->RunEmbeddedInstall()) {
                    DeckDialog(GetLanguageString("install_title"), GetLanguageString("install_success"), { GetLanguageString("reboot") }, true);
                    g_MainApplication->FadeOut();
                    RebootSystem();
                }
                else {
                    ShowInstallError();
                }
            }
            else {
                DeckDialog(GetLanguageString("activate_changes_title"), GetLanguageString("activate_not_present"), { GetLanguageString("ok") }, true);
            }
            return;
        }

        if(IsSystemActive()) {
            DeactivateSystem();
        }
        else {
            ActivateSystem();
        }

        this->activate_menu_item->SetName(GetStatus());
        this->ReloadMenu();

        DeckDialog(GetLanguageString("activate_changes_title"), GetLanguageString("activate_changes"), { GetLanguageString("reboot") }, true);
        g_MainApplication->FadeOut();
        RebootSystem();
    }

    void MainMenuLayout::resetMenu_DefaultKey() {
        const auto option = DeckDialog(GetLanguageString("reset_menu_title"), GetLanguageString("reset_menu_conf"), { GetLanguageString("yes"), GetLanguageString("cancel") }, true);
        if(option == 0) {
            const auto is_emummc = os::IsEmuMMC();

            std::vector<AccountUid> uids;
            UL_RC_ASSERT(acc::ListAccounts(uids));

            for(const auto &uid : uids) {
                const auto menu_path = menu::MakeMenuPath(is_emummc, uid);
                fs::DeleteDirectory(menu_path);
            }

            // When returning to uMenu it will automatically regenerate the menu entries

            g_MainApplication->ShowNotification(GetLanguageString("reset_menu_success"));
        }
    }

    void MainMenuLayout::resetCache_DefaultKey() {
        const auto option = DeckDialog(GetLanguageString("reset_cache_title"), GetLanguageString("reset_cache_conf"), { GetLanguageString("yes"), GetLanguageString("cancel") }, true);
        if(option == 0) {
            // Regenerate cache
            const auto cur_app_recs = os::ListApplicationRecords();
            menu::CacheApplications(cur_app_recs);
            menu::CacheHomebrew();

            cfg::RemoveActiveThemeCache();

            g_MainApplication->ShowNotification(GetLanguageString("reset_cache_success"));
        }
    }

    /* No public update channel yet. */

}
