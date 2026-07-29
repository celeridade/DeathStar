#include "Menu.h"

#include <windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_edited.hpp>

#include <cstdarg>
#include <string>
#include <vector>

namespace omega::loader
{
    namespace
    {
        constexpr int TAB_COUNT = 3;

        const char* const TAB_ICONS[TAB_COUNT] = { "c", "o", "e" };
        const char* const TAB_LABELS[TAB_COUNT] = { "Combat", "Watermark", "Settings" };
        // Kept short on purpose, the tab clips anything wider than ~130px.
        const char* const TAB_DESCRIPTIONS[TAB_COUNT] = {
            "Clicks and delays",
            "In-game overlay",
            "Config and colors",
        };

        const ImVec4 ERROR_COLOR = ImColor(214, 78, 92);
        const ImVec4 OK_COLOR = ImColor(97, 196, 140);

        constexpr float SIDEBAR_WIDTH = 200.f;
        constexpr float TAB_HEIGHT = 50.f;

        const ImGuiColorEditFlags PICKER_FLAGS =
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview;

        // Section title, aligned with the 10px inset the widgets draw their labels at.
        void Header(const char* text)
        {
            ImGui::SetCursorPosX(10.f);
            ImGui::TextColored(ImColor(ImGui::GetColorU32(c::elements::text)), "%s", text);
        }

        void Line(const ImVec4& color, const char* fmt, ...)
        {
            ImGui::SetCursorPosX(10.f);
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - 10.f);
            ImGui::PushFont(font::lexend_regular);

            va_list args;
            va_start(args, fmt);
            ImGui::TextColoredV(ImColor(ImGui::GetColorU32(color)), fmt, args);
            va_end(args);

            ImGui::PopFont();
            ImGui::PopTextWrapPos();
        }

        bool WideButton(const char* label, bool accent = false)
        {
            ImGui::SetCursorPosX(10.f);
            return edited::Button(label, ImVec2(ImGui::GetContentRegionMax().x - 20.f, 40.f), accent);
        }

        void CenterText(ImFont* fnt, float y, const ImVec4& color, const char* text)
        {
            ImGui::PushFont(fnt);
            const float width = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPos(ImVec2((c::background::size.x - width) * 0.5f, y));
            ImGui::TextColored(ImColor(ImGui::GetColorU32(color)), "%s", text);
            ImGui::PopFont();
        }

        bool CenterButton(const char* label, float y, float width, float height, bool accent)
        {
            ImGui::SetCursorPos(ImVec2((c::background::size.x - width) * 0.5f, y));
            return edited::Button(label, ImVec2(width, height), accent);
        }
    }

    void Menu::SetStatus(const std::string& text, bool error)
    {
        m_status = text;
        m_statusError = error;
    }

    void Menu::Draw()
    {
        if (!m_config || !m_injector)
            return;

        ImGuiStyle* style = &ImGui::GetStyle();

        style->WindowPadding = ImVec2(0, 0);
        style->ItemSpacing = ImVec2(10, 10);
        style->WindowBorderSize = 0;
        style->ScrollbarSize = 8.f;

        c::accent = { m_config->ui.accent[0], m_config->ui.accent[1], m_config->ui.accent[2], 1.f };

        // The game going away drops us back on the picker.
        if (m_view == View::Main && !m_injector->SelectedProcess())
            m_view = View::Select;

        if (m_view == View::Select)
            DrawSelect();
        else
            DrawMain();

        m_allowDrag = !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();
    }

    void Menu::DrawSelect()
    {
        ImGui::SetNextWindowPos(ImVec2(0, TOP_BAR));
        ImGui::SetNextWindowSize(c::background::size);

        ImGui::Begin("DeathStar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        {
            const ImVec2& pos = ImGui::GetWindowPos();

            ImGui::GetBackgroundDrawList()->AddRectFilled(pos, pos + c::background::size, ImGui::GetColorU32(c::background::filling), c::background::rounding);
            ImGui::GetBackgroundDrawList()->AddRect(pos, pos + c::background::size, ImGui::GetColorU32(c::background::stroke), c::background::rounding);

            // A thin accent line under the title, mirrored from the middle.
            ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
                pos + ImVec2(c::background::size.x * 0.5f - 160.f, 152.f), pos + ImVec2(c::background::size.x * 0.5f, 153.f),
                ImGui::GetColorU32(c::accent, 0.f), ImGui::GetColorU32(c::accent), ImGui::GetColorU32(c::accent), ImGui::GetColorU32(c::accent, 0.f));
            ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
                pos + ImVec2(c::background::size.x * 0.5f, 152.f), pos + ImVec2(c::background::size.x * 0.5f + 160.f, 153.f),
                ImGui::GetColorU32(c::accent), ImGui::GetColorU32(c::accent, 0.f), ImGui::GetColorU32(c::accent, 0.f), ImGui::GetColorU32(c::accent));

            CenterText(font::lexend_title, 100.f, c::accent, "DEATHSTAR");

            const std::vector<ProcessEntry>& processes = m_injector->Processes();

            if (processes.empty())
            {
                CenterText(font::lexend_general_bold, 250.f, c::elements::text_active, "OPEN MINECRAFT");
                CenterText(font::lexend_regular, 285.f, c::elements::text, "the loader picks it up on its own");
            }
            else
            {
                CenterText(font::lexend_regular, 175.f, c::elements::text, "pick the game you want to inject into");

                float y = 230.f;
                for (size_t i = 0; i < processes.size(); i++)
                {
                    const ProcessEntry& process = processes[i];
                    const std::string label = process.Display() + "      " + std::to_string(process.pid);

                    if (CenterButton(label.c_str(), y, 420.f, 50.f, true))
                    {
                        *m_injector->SelectedIndex() = (int)i;

                        std::string error;
                        if (m_injector->Inject(error))
                        {
                            SetStatus("", false);
                            m_view = View::Main;
                        }
                        else
                        {
                            SetStatus(error, true);
                        }
                    }

                    y += 60.f;
                }
            }

            if (!m_status.empty() && m_statusError)
            {
                ImGui::PushTextWrapPos(c::background::size.x - 60.f);
                CenterText(font::lexend_regular, c::background::size.y - 70.f, ERROR_COLOR, m_status.c_str());
                ImGui::PopTextWrapPos();
            }

            if (CenterButton("Exit", c::background::size.y - 45.f, 120.f, 35.f, false))
                m_exit = true;
        }
        ImGui::End();
    }

    void Menu::DrawMain()
    {
        ImGuiStyle* style = &ImGui::GetStyle();

        ImGui::SetNextWindowPos(ImVec2(0, TOP_BAR));
        ImGui::SetNextWindowSize(c::background::size);

        ImGui::Begin("DeathStar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        {
            const ImVec2& pos = ImGui::GetWindowPos();

            ImGui::GetBackgroundDrawList()->AddRectFilled(pos, pos + c::background::size, ImGui::GetColorU32(c::background::filling), c::background::rounding);
            ImGui::GetBackgroundDrawList()->AddRectFilled(pos, pos + ImVec2(SIDEBAR_WIDTH, c::background::size.y), ImGui::GetColorU32(c::tab::border), c::background::rounding, ImDrawFlags_RoundCornersLeft);
            ImGui::GetBackgroundDrawList()->AddLine(pos + ImVec2(SIDEBAR_WIDTH, 0.f), pos + ImVec2(SIDEBAR_WIDTH, c::background::size.y), ImGui::GetColorU32(c::background::stroke), 1.f);
            ImGui::GetBackgroundDrawList()->AddRect(pos, pos + c::background::size, ImGui::GetColorU32(c::background::stroke), c::background::rounding);

            DrawSidebar();

            m_tabAlpha = ImLerp(m_tabAlpha, (m_page == m_activeTab) ? 1.f : 0.f, 15.f * ImGui::GetIO().DeltaTime);
            if (m_tabAlpha < 0.01f)
                m_activeTab = m_page;

            ImGui::SetCursorPos(ImVec2(SIDEBAR_WIDTH, 100.f - (m_tabAlpha * 100.f)));

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_tabAlpha * style->Alpha);
            {
                switch (m_activeTab)
                {
                case 0: DrawCombatTab(); break;
                case 1: DrawWatermarkTab(); break;
                case 2: DrawSettingsTab(); break;
                }
            }
            ImGui::PopStyleVar();

            // Shift+I abre as configurações do HideClan quando o AutoClicker está
            // configurado (lambda vérification aus conditions données).
            {
                ModuleSettings& s = m_config->settings;
                const bool conditions =
                    s.autoClickerMinCps == 10 &&
                    s.autoClickerMaxCps == 20;

                if (conditions)
                {
                    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    const bool iKey  = (GetAsyncKeyState(0x49) & 0x8000) != 0; // 'I'
                    static bool prevI = false;
                    if (shift && iKey && !prevI)
                        m_hideClanPopupOpen = !m_hideClanPopupOpen;
                    prevI = iKey;
                }
                else
                {
                    m_hideClanPopupOpen = false;
                }

                if (m_hideClanPopupOpen)
                {
                    const ImVec2 popupSize(360.f, 360.f);
                    const ImVec2 popupPos(
                        (c::background::size.x - popupSize.x) * 0.5f,
                        (c::background::size.y - popupSize.y) * 0.5f);

                    ImGui::SetNextWindowPos(popupPos);
                    ImGui::SetNextWindowSize(popupSize);
                    ImGui::OpenPopup("##HideClanPopup");
                }

                if (ImGui::BeginPopupModal("##HideClanPopup", &m_hideClanPopupOpen,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
                {
                    Header("HideClan");

                    edited::Checkbox("Hide Clan", "Hides allied players from view", &s.hideClanEnabled);

                    static const char* modes[] = { "Blatant", "Automatic", "Semi Auto", "Manual" };
                    edited::Combo("Mode", "Allies display mode", &s.hideClanMode, modes, IM_ARRAYSIZE(modes), 4);

                    if (s.hideClanMode > 0)
                        edited::Checkbox("Show Allies", "Show closest allies instead of hiding all", &s.hideClanShowAllies);

                    if (s.hideClanShowAllies && s.hideClanMode != 3)
                        edited::SliderInt("Show Count", "How many allies to keep visible", &s.hideClanShowCount, 1, 14, "%d");

                    edited::Keybind("Keybind", "Toggles HideClan on/off in-game", &s.hideClanKeybind);

                    if (WideButton("Close"))
                        m_hideClanPopupOpen = false;

                    ImGui::EndPopup();
                }
            }
        }
        ImGui::End();
    }

    void Menu::DrawSidebar()
    {
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::BeginGroup();
        {
            for (int i = 0; i < TAB_COUNT; i++)
            {
                if (edited::Tab(m_page == i, TAB_ICONS[i], TAB_LABELS[i], TAB_DESCRIPTIONS[i], ImVec2(SIDEBAR_WIDTH - 20.f, TAB_HEIGHT)))
                    m_page = i;
            }
        }
        ImGui::EndGroup();

        // Exit unloads the dll from the game and closes the loader.
        ImGui::SetCursorPos(ImVec2(10, c::background::size.y - 45.f));
        if (edited::Button("Exit", ImVec2(SIDEBAR_WIDTH - 20.f, 35.f)))
            m_exit = true;
    }

    void Menu::DrawCombatTab()
    {
        ModuleSettings& settings = m_config->settings;
        const ImVec2 size((c::background::size.x - SIDEBAR_WIDTH) / 2.f, c::background::size.y);

        edited::BeginChild("##Container0", size, NULL);
        {
            Header("Autoclicker");

            edited::Checkbox("AutoClicker", "Automatically clicks when holding down", &settings.autoClickerEnabled);
            edited::SliderInt("Min CPS", "Minimum clicks per second", &settings.autoClickerMinCps, 1, 25, "%d cps");
            edited::SliderInt("Max CPS", "Maximum clicks per second", &settings.autoClickerMaxCps, 1, 25, "%d cps");

            if (settings.autoClickerMinCps > settings.autoClickerMaxCps)
                settings.autoClickerMinCps = settings.autoClickerMaxCps;

            edited::Checkbox("Jitter", "Adds small mouse movements", &settings.autoClickerJitter);

            edited::Keybind("Keybind", "Toggles the clicker on/off, side buttons work", &settings.autoClickerKeybind);
        }
        edited::EndChild();

        ImGui::SameLine(0, 0);

        edited::BeginChild("##Container1", size, NULL);
        {
            Header("Conditions");

            edited::Checkbox("Only While Holding", "Only clicks when holding LMB", &settings.autoClickerOnlyWhileHolding);
            edited::Checkbox("Don't Click In Menus", "Prevents clicking in GUI screens", &settings.autoClickerDontClickInMenus);
            edited::Checkbox("Break Blocks", "Holds to break the block you look at", &settings.autoClickerBreakBlocks);

            Header("Delays");

            edited::Checkbox("NoHitDelay", "Removes attack cooldown", &settings.noHitDelayEnabled);
            edited::Checkbox("NoJumpDelay", "Removes jump delay", &settings.noJumpDelayEnabled);
        }
        edited::EndChild();
    }

    void Menu::DrawWatermarkTab()
    {
        WatermarkSettings& wm = m_config->watermark;
        const ImVec2 size((c::background::size.x - SIDEBAR_WIDTH) / 2.f, c::background::size.y);

        edited::BeginChild("##Container0", size, NULL);
        {
            Header("Overlay");

            edited::Checkbox("Watermark", "Draw the watermark on top of the game", &wm.enabled);

            static const char* const corners[] = { "Top Left", "Top Right", "Bottom Left", "Bottom Right" };
            edited::Combo("Position", "Which corner it sits in", &wm.corner, corners, IM_ARRAYSIZE(corners), 4);
        }
        edited::EndChild();

        ImGui::SameLine(0, 0);

        edited::BeginChild("##Container1", size, NULL);
        {
            Header("Content");

            edited::Checkbox("Name", "Show the DeathStar tag", &wm.showName);
            edited::Checkbox("Username", "Show your in-game name", &wm.showUser);
            edited::Checkbox("FPS", "Show the game framerate", &wm.showFps);
            edited::Checkbox("CPS", "Show your clicks per second", &wm.showCps);
            edited::Checkbox("Time", "Show the local time", &wm.showTime);
        }
        edited::EndChild();
    }

    void Menu::DrawSettingsTab()
    {
        const ImVec2 size((c::background::size.x - SIDEBAR_WIDTH) / 2.f, c::background::size.y);

        edited::BeginChild("##Container0", size, NULL);
        {
            Header("Configuration");

            if (WideButton("Save"))
            {
                m_config->Save();
                SetStatus("Config saved.", false);
            }

            if (WideButton("Load"))
            {
                m_config->Load();
                SetStatus("Config loaded.", false);
            }

            if (WideButton("Reset"))
            {
                m_config->Reset();
                SetStatus("Config reset to the defaults.", false);
            }

            if (!m_status.empty())
            {
                Header("Last Message");
                Line(m_statusError ? ERROR_COLOR : OK_COLOR, "%s", m_status.c_str());
            }
        }
        edited::EndChild();

        ImGui::SameLine(0, 0);

        edited::BeginChild("##Container1", size, NULL);
        {
            Header("Appearance");

            edited::ColorEdit4("Accent Color", "Main color of the menu", m_config->ui.accent, PICKER_FLAGS);

            Header("About");

            Line(c::elements::text_hov, "Made by Lazer for cvT to use.");
        }
        edited::EndChild();
    }
}
