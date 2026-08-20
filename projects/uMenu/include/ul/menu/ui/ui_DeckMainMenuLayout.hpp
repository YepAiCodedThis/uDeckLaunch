#pragma once
#include <ul/menu/ui/ui_IMenuLayout.hpp>

namespace ul::menu::ui {

    class DeckMainMenuLayout : public IMenuLayout {
        private:
            bool next_reload_user_changed;
            bool deck_ready;

            void EnsureDeck();
            void DestroyDeck();

        public:
            DeckMainMenuLayout();
            ~DeckMainMenuLayout();
            PU_SMART_CTOR(DeckMainMenuLayout)

            void OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override;
            void OnMenuUpdate() override;

            bool OnHomeButtonPress() override;
            void LoadSfx() override;
            void DisposeSfx() override;

            void Initialize();
            void Reload();

            inline void NotifyNextReloadUserChanged() {
                this->next_reload_user_changed = true;
            }

            inline void UpdateApplicationVerifyProgress(const u64 app_id, const float progress) {
                (void)app_id;
                (void)progress;
            }
    };

}
