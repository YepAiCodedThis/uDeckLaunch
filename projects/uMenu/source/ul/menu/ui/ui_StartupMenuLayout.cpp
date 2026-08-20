#include <ul/menu/ui/ui_StartupMenuLayout.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <ul/menu/smi/smi_Commands.hpp>

extern "C" {
#include <userpick.h>
#include <titles.h>
#include <theme.h>
#include <deck_fonts.h>
#include <sfx.h>
#include <settings.h>
#include <deck_lang.h>
}

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {

    namespace {

        DhUserpick *g_Userpick = nullptr;
        DhFonts g_UserpickFonts = {};
        DhTheme g_UserpickTheme = {};

        class UserPickElement : public pu::ui::elm::Element {
            public:
                PU_SMART_CTOR(UserPickElement)

                s32 GetX() override {
                    return 0;
                }

                s32 GetY() override {
                    return 0;
                }

                s32 GetWidth() override {
                    return 1920;
                }

                s32 GetHeight() override {
                    return 1080;
                }

                void OnRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) override {
                    (void)drawer;
                    (void)x;
                    (void)y;
                    auto *ren = pu::ui::render::GetMainRenderer();
                    if((g_Userpick == nullptr) || (ren == nullptr)) {
                        return;
                    }
                    dh_userpick_tick(g_Userpick, ren);
                    dh_userpick_draw(g_Userpick, ren);
                }

                void OnInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override {
                    (void)keys_down;
                    (void)keys_up;
                    (void)keys_held;
                    (void)touch_pos;
                }
        };

    }

    void StartupMenuLayout::user_DefaultKey(const AccountUid uid) {
        pu::audio::PlaySfx(this->user_select_sfx);
        dh_sfx_startup();
        g_GlobalSettings.SetSelectedUser(uid);

        auto &main_menu_lyt = g_MenuApplication->GetMainMenuLayout();
        if(main_menu_lyt != nullptr) {
            main_menu_lyt->NotifyNextReloadUserChanged();
        }

        /* Same Deck black on both screens — skip Plutonium fade (it hitchs).
           Load the launcher first so draw/fonts stay refcounted, then drop user-pick. */
        g_MenuApplication->LoadMenu(MenuType::Main, false);
        if(g_Userpick != nullptr) {
            dh_userpick_detach_titles(g_Userpick);
        }
        this->DestroyUserPick();
    }

    void StartupMenuLayout::create_DefaultKey() {
        pu::audio::PlaySfx(this->user_create_sfx);

        g_MenuApplication->FadeOutToLibraryApplet(AppletId_LibraryAppletMyPage);
        UL_RC_ASSERT(smi::OpenAddUser());
        g_MenuApplication->Finalize();
    }

    StartupMenuLayout::StartupMenuLayout() : IMenuLayout(), load_menu(false), userpick_ready(false), pending_uid{} {
        this->user_create_sfx = nullptr;
        this->user_select_sfx = nullptr;
        this->SetBackgroundColor({ 0x0E, 0x14, 0x1B, 0xFF });
        this->Add(UserPickElement::New());
    }

    StartupMenuLayout::~StartupMenuLayout() {
        this->DestroyUserPick();
    }

    void StartupMenuLayout::EnsureUserPick() {
        if(this->userpick_ready) {
            return;
        }
        auto *ren = pu::ui::render::GetMainRenderer();
        if(ren == nullptr) {
            return;
        }

        dh_theme_init(&g_UserpickTheme);
        if(deck_fonts_load(&g_UserpickFonts) != 0) {
            UL_LOG_WARN("Userpick fonts missing");
            return;
        }

        g_Userpick = dh_userpick_create(ren, &g_UserpickFonts, &g_UserpickTheme);
        if(g_Userpick == nullptr) {
            deck_fonts_close(&g_UserpickFonts);
            return;
        }

        dh_userpick_set_title(g_Userpick, dh_lang("who_playing"));
        auto logo = GetLogoTexture();
        if(logo != nullptr) {
            dh_userpick_set_logo(g_Userpick, logo->Get());
        }
        this->userpick_ready = true;
    }

    void StartupMenuLayout::DestroyUserPick() {
        if(g_Userpick != nullptr) {
            dh_userpick_destroy(g_Userpick);
            g_Userpick = nullptr;
        }
        deck_fonts_close(&g_UserpickFonts);
        this->userpick_ready = false;
    }

    void StartupMenuLayout::LoadSfx() {
        this->user_create_sfx = pu::audio::LoadSfx(TryGetActiveThemeResource("sound/Startup/UserCreate.wav"));
        this->user_select_sfx = pu::audio::LoadSfx(TryGetActiveThemeResource("sound/Startup/UserSelect.wav"));
    }

    void StartupMenuLayout::DisposeSfx() {
        pu::audio::DestroySfx(this->user_create_sfx);
        pu::audio::DestroySfx(this->user_select_sfx);
    }

    void StartupMenuLayout::OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        (void)keys_up;
        (void)keys_held;
        this->EnsureUserPick();
        if(this->load_menu) {
            this->load_menu = false;
            this->user_DefaultKey(this->pending_uid);
            return;
        }
        if(g_Userpick == nullptr) {
            return;
        }

        const int tx = touch_pos.IsEmpty() ? -1 : touch_pos.x;
        const int ty = touch_pos.IsEmpty() ? -1 : touch_pos.y;
        const auto action = dh_userpick_handle_input(g_Userpick, keys_down, tx, ty);
        if(action == DH_USERPICK_SELECT) {
            AccountUid uid = {};
            if(dh_userpick_selected_uid(g_Userpick, &uid)) {
                this->pending_uid = uid;
                this->load_menu = true;
            }
        }
        else if(action == DH_USERPICK_ADD) {
            this->create_DefaultKey();
        }
    }

    void StartupMenuLayout::OnMenuUpdate() {
        this->EnsureUserPick();
        if(this->userpick_ready) {
            g_MenuApplication->PrewarmMainMenu();
        }
    }

    bool StartupMenuLayout::OnHomeButtonPress() {
        return true;
    }

    void StartupMenuLayout::ReloadMenu() {
        this->EnsureUserPick();
        this->load_menu = false;
        if(g_Userpick != nullptr) {
            dh_userpick_set_title(g_Userpick, dh_lang("who_playing"));
            dh_userpick_reload(g_Userpick);
        }
    }

}
