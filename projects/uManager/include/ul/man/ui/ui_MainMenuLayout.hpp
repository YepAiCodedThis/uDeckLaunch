
#pragma once
#include <pu/Plutonium>

namespace ul::man::ui {

    class MainMenuLayout : public pu::ui::Layout {
        public:
            static constexpr pu::ui::Color BackgroundColor = { 0x0E, 0x14, 0x1B, 0xFF };
            static constexpr pu::ui::Color TitleColor = { 0xDC, 0xDE, 0xDF, 0xFF };
            static constexpr pu::ui::Color InfoTextColor = { 0x8B, 0x92, 0x9A, 0xFF };
            static constexpr pu::ui::Color MenuColor = { 0x16, 0x1B, 0x22, 0xFF };
            static constexpr pu::ui::Color MenuFocusColor = { 0x32, 0x38, 0x42, 0xFF };
            static constexpr pu::ui::Color MenuItemColor = { 0xDC, 0xDE, 0xDF, 0xFF };

            static constexpr u32 LogoY = 36;
            static constexpr u32 LogoH = 96;
            static constexpr u32 TitleY = 148;
            static constexpr u32 InfoTextY = 210;

            static constexpr u32 UpdateDownloadBarY = 280;
            static constexpr u32 UpdateDownloadBarHorizontalMargin = 360;
            static constexpr u32 UpdateDownloadBarWidth = pu::ui::render::ScreenWidth - 2 * UpdateDownloadBarHorizontalMargin;
            static constexpr u32 UpdateDownloadBarHeight = 28;

            static constexpr u32 MenuX = 280;
            static constexpr u32 MenuY = 280;
            static constexpr u32 MenuW = pu::ui::render::ScreenWidth - 2 * MenuX;
            static constexpr u32 MenuItemCount = 3;
            static constexpr u32 MenuItemSize = 92;

        private:
            pu::ui::elm::Image::Ref logo;
            pu::ui::elm::TextBlock::Ref title_text;
            pu::ui::elm::TextBlock::Ref info_text;
            pu::ui::elm::ProgressBar::Ref update_download_bar;
            pu::ui::elm::Menu::Ref options_menu;
            pu::ui::elm::MenuItem::Ref activate_menu_item;
            pu::ui::elm::MenuItem::Ref reset_menu_menu_item;
            pu::ui::elm::MenuItem::Ref reset_cache_menu_item;
            // pu::ui::elm::MenuItem::Ref update_menu_item;

            void ResetInfoText();
            bool RunEmbeddedInstall();

            inline void ReloadMenu() {
                this->options_menu->ClearItems();
                this->options_menu->AddItem(this->activate_menu_item);
                this->options_menu->AddItem(this->reset_menu_menu_item);
                this->options_menu->AddItem(this->reset_cache_menu_item);
                // this->options_menu->AddItem(this->update_menu_item);
            }

        public:
            MainMenuLayout();
            PU_SMART_CTOR(MainMenuLayout)

            void activate_DefaultKey();
            void resetMenu_DefaultKey();
            void resetCache_DefaultKey();
            // void update_DefaultKey();
    };

}
