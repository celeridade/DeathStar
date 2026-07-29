#define IMGUI_DEFINE_MATH_OPERATORS
#include "Watermark.h"

#include "Shared.h"
#include "../../game/classes/Minecraft.h"

#include <imgui.h>
#include <chrono>
#include <ctime>
#include <mutex>
#include <vector>

namespace omega::dll::features::render
{
    namespace
    {
        shared::SharedConfig* g_config = nullptr;
        ImFont* g_bold = nullptr;
        ImFont* g_regular = nullptr;

        std::mutex g_nameMutex;
        std::string g_playerName;

        unsigned int g_clickCount = 0;
        unsigned int g_lastClickCount = 0;
        int g_cps = 0;
        std::chrono::steady_clock::time_point g_lastCpsSample = std::chrono::steady_clock::now();

        std::string PlayerName()
        {
            std::lock_guard<std::mutex> lock(g_nameMutex);
            return g_playerName;
        }

        std::string ClockText()
        {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_s(&local, &now);

            char buffer[16]{};
            std::strftime(buffer, sizeof(buffer), "%H:%M", &local);
            return buffer;
        }

        int SampleCps()
        {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastCpsSample).count();

            if (elapsed >= 500)
            {
                const unsigned int current = g_clickCount;
                const unsigned int delta = current - g_lastClickCount;
                g_cps = (int)(delta * 1000.0 / (double)elapsed);
                g_lastClickCount = current;
                g_lastCpsSample = now;
            }

            return g_cps;
        }
    }

    void Watermark::SetConfig(shared::SharedConfig* config)
    {
        g_config = config;
    }

    void Watermark::SetFonts(ImFont* bold, ImFont* regular)
    {
        g_bold = bold;
        g_regular = regular;
    }

    void Watermark::SetClickCount(unsigned int count)
    {
        g_clickCount = count;
    }

    void Watermark::Shutdown()
    {
        std::lock_guard<std::mutex> lock(g_nameMutex);
        for (size_t i = 0; i < g_playerName.size(); i++)
            g_playerName[i] = '\0';
        g_playerName.clear();
        g_playerName.shrink_to_fit();
    }

    void Watermark::RefreshFromGame(JNIEnv* env)
    {
        if (!env)
            return;

        std::string name = Minecraft::GetPlayerName(env);
        if (name.empty())
            return;

        std::lock_guard<std::mutex> lock(g_nameMutex);
        g_playerName = std::move(name);
    }

    void Watermark::Draw(float screenWidth, float screenHeight, int fps)
    {
        if (!g_config || !g_config->watermarkEnabled)
            return;

        std::vector<std::string> items;

        if (g_config->watermarkName)
            items.push_back("DEATHSTAR");

        if (g_config->watermarkUser)
        {
            const std::string name = PlayerName();
            items.push_back(name.empty() ? "..." : name);
        }

        if (g_config->watermarkFps)
            items.push_back(std::to_string(fps) + "fps");

        if (g_config->watermarkCps)
            items.push_back(std::to_string(SampleCps()) + "cps");

        if (g_config->watermarkTime)
            items.push_back(ClockText());

        if (items.empty())
            return;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        if (!draw)
            return;

        ImFont* font = g_bold ? g_bold : ImGui::GetFont();
        const float fontSize = font->FontSize;

        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_config->watermarkColor[0], g_config->watermarkColor[1], g_config->watermarkColor[2], 1.f));
        const ImU32 text = IM_COL32(255, 255, 255, 235);
        const ImU32 dim = IM_COL32(120, 128, 140, 255);
        const ImU32 filling = IM_COL32(12, 12, 12, 215);
        const ImU32 stroke = IM_COL32(28, 26, 37, 255);

        const float padding = 12.f;
        const float gap = 8.f;
        const float separator = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, "|").x;

        float contentWidth = 0.f;
        for (size_t i = 0; i < items.size(); i++)
        {
            contentWidth += font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, items[i].c_str()).x;
            if (i + 1 < items.size())
                contentWidth += gap * 2.f + separator;
        }

        const float boxWidth = contentWidth + padding * 2.f;
        const float boxHeight = fontSize + 16.f;
        const float margin = 12.f;

        ImVec2 origin;
        switch (g_config->watermarkCorner)
        {
        case 1:  origin = ImVec2(screenWidth - boxWidth - margin, margin); break;
        case 2:  origin = ImVec2(margin, screenHeight - boxHeight - margin); break;
        case 3:  origin = ImVec2(screenWidth - boxWidth - margin, screenHeight - boxHeight - margin); break;
        default: origin = ImVec2(margin, margin); break;
        }

        const ImVec2 end = origin + ImVec2(boxWidth, boxHeight);

        draw->AddRectFilled(origin, end, filling, 5.f);
        draw->AddRect(origin, end, stroke, 5.f);
        // Accent underline, the same cue the menu uses.
        draw->AddRectFilled(ImVec2(origin.x, end.y - 2.f), end, accent, 5.f, ImDrawFlags_RoundCornersBottom);

        float x = origin.x + padding;
        const float y = origin.y + (boxHeight - fontSize) * 0.5f;

        for (size_t i = 0; i < items.size(); i++)
        {
            const ImU32 color = (i == 0 && g_config->watermarkName) ? accent : text;

            draw->AddText(font, fontSize, ImVec2(x, y), color, items[i].c_str());
            x += font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, items[i].c_str()).x;

            if (i + 1 < items.size())
            {
                x += gap;
                draw->AddText(font, fontSize, ImVec2(x, y), dim, "|");
                x += separator + gap;
            }
        }
    }
}
