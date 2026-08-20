#include <ul/menu/ui/ui_DeckMainMenuLayout.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <ul/menu/smi/smi_Commands.hpp>

extern "C" {
#include <ui.h>
#include <titles.h>
#include <theme.h>
#include <deck_fonts.h>
}

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {

    namespace {

        DhUi *g_DeckUi = nullptr;
        DhFonts g_DeckFonts = {};
        DhTheme g_DeckTheme = {};
        bool g_TitlesInited = false;

        class DeckGridElement : public pu::ui::elm::Element {
            public:
                PU_SMART_CTOR(DeckGridElement)

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
                    if((g_DeckUi == nullptr) || (ren == nullptr)) {
                        return;
                    }
                    dh_ui_tick(g_DeckUi, ren);
                    dh_ui_draw(g_DeckUi, ren);
                }

                void OnInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override {
                    (void)keys_down;
                    (void)keys_up;
                    (void)keys_held;
                    (void)touch_pos;
                }
        };

    }

    DeckMainMenuLayout::DeckMainMenuLayout() : IMenuLayout(), next_reload_user_changed(false), deck_ready(false) {
        this->SetBackgroundColor({ 0x0E, 0x14, 0x1B, 0xFF });
        this->Add(DeckGridElement::New());
    }

    DeckMainMenuLayout::~DeckMainMenuLayout() {
        this->DestroyDeck();
    }

    void DeckMainMenuLayout::EnsureDeck() {
        if(this->deck_ready) {
            return;
        }
        auto *ren = pu::ui::render::GetMainRenderer();
        if(ren == nullptr) {
            return;
        }

        dh_theme_init(&g_DeckTheme);
        if(!g_TitlesInited) {
            if(dh_titles_init() != 0) {
                UL_LOG_WARN("Deck titles init failed");
            }
            g_TitlesInited = dh_titles_ok();
        }

        if(deck_fonts_load(&g_DeckFonts) != 0) {
            UL_LOG_WARN("Deck fonts missing — grid will not start");
            return;
        }

        g_DeckUi = dh_ui_create(ren, &g_DeckFonts, &g_DeckTheme);
        if(g_DeckUi == nullptr) {
            deck_fonts_close(&g_DeckFonts);
            return;
        }
        this->deck_ready = true;
        UL_LOG_INFO("Deck grid ready titles=%d", dh_titles_count());
    }

    void DeckMainMenuLayout::DestroyDeck() {
        if(g_DeckUi != nullptr) {
            dh_ui_destroy(g_DeckUi);
            g_DeckUi = nullptr;
        }
        deck_fonts_close(&g_DeckFonts);
        if(g_TitlesInited) {
            dh_titles_exit();
            g_TitlesInited = false;
        }
        this->deck_ready = false;
    }

    void DeckMainMenuLayout::OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        (void)keys_up;
        (void)touch_pos;
        this->EnsureDeck();
        if(g_DeckUi == nullptr) {
            return;
        }
        dh_ui_set_held(g_DeckUi, keys_held);
        dh_ui_handle_input(g_DeckUi, keys_down, keys_held);

        if(g_MenuApplication->GetConsumeApplicationRecordReloadNeeded() || g_MenuApplication->GetConsumeApplicationEntryReloadNeeded()) {
            dh_titles_refresh();
        }
        if(g_MenuApplication->HasVerifyFinishedPending()) {
            (void)g_MenuApplication->GetConsumeVerifyFinishedApplicationId();
            (void)g_MenuApplication->GetConsumeVerifyResult();
            (void)g_MenuApplication->GetConsumeVerifyDetailResult();
            dh_titles_refresh();
        }
    }

    void DeckMainMenuLayout::OnMenuUpdate() {
        this->EnsureDeck();
    }

    bool DeckMainMenuLayout::OnHomeButtonPress() {
        if(g_GlobalSettings.IsSuspended()) {
            ul::menu::smi::ResumeApplication();
        }
        return true;
    }

    void DeckMainMenuLayout::LoadSfx() {}

    void DeckMainMenuLayout::DisposeSfx() {}

    void DeckMainMenuLayout::Initialize() {
        this->EnsureDeck();
    }

    void DeckMainMenuLayout::Reload() {
        this->EnsureDeck();
        if(this->next_reload_user_changed && (g_DeckUi != nullptr)) {
            dh_ui_reload_user(g_DeckUi);
        }
        this->next_reload_user_changed = false;
    }

}
