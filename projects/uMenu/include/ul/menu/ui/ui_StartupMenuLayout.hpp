#pragma once
#include <ul/menu/ui/ui_IMenuLayout.hpp>

namespace ul::menu::ui {

    class StartupMenuLayout : public IMenuLayout {
        private:
            bool load_menu;
            bool userpick_ready;
            AccountUid pending_uid;
            pu::audio::Sfx user_create_sfx;
            pu::audio::Sfx user_select_sfx;

            void EnsureUserPick();
            void DestroyUserPick();
            void user_DefaultKey(const AccountUid uid);
            void create_DefaultKey();

        public:
            StartupMenuLayout();
            ~StartupMenuLayout();
            PU_SMART_CTOR(StartupMenuLayout)

            void OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override;
            void OnMenuUpdate() override;
            bool OnHomeButtonPress() override;
            void LoadSfx() override;
            void DisposeSfx() override;

            void ReloadMenu();
    };

}
