#pragma once
#include <string>
#include "Config.h"
#include "Injector.h"

namespace omega::loader
{
    class Menu
    {
    public:
        // The old info bar became the in-game watermark, so the panel is the
        // whole window now.
        static constexpr float TOP_BAR = 0.f;

        void SetConfig(Config* config) { m_config = config; }
        void SetInjector(Injector* injector) { m_injector = injector; }
        void SetShared(shared::SharedConfig* shared) { m_shared = shared; }

        void Draw();

        bool WantsExit() const { return m_exit; }
        // False while the cursor is over a widget, so dragging never fights a click.
        bool AllowDrag() const { return m_allowDrag; }

    private:
        enum class View { Select, Main };

        void DrawSelect();
        void DrawMain();
        void DrawSidebar();
        void DrawCombatTab();
        void DrawWatermarkTab();
        void DrawSettingsTab();
        void DrawAltManagerTab();

        void SetStatus(const std::string& text, bool error);

        Config* m_config = nullptr;
        Injector* m_injector = nullptr;
        shared::SharedConfig* m_shared = nullptr;

        View m_view = View::Select;
        int m_page = 0;
        int m_activeTab = 0;
        float m_tabAlpha = 0.f;
        bool m_exit = false;
        bool m_allowDrag = true;
        char m_altUsername[17]{};
        std::uint32_t m_altRequest = 0;

        std::string m_status;
        bool m_statusError = false;
    };
}
