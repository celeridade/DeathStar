#include "Config.h"
#include <windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include "json.hpp"

using json = nlohmann::json;

namespace omega::loader
{
    Config::Config()
    {
        Load();
    }

    static std::wstring GetConfigPath()
    {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(nullptr, path, MAX_PATH))
        {
            std::wstring directory(path);
            const size_t slash = directory.find_last_of(L'\\');
            if (slash != std::wstring::npos)
                directory.resize(slash + 1);

            return directory + L"DeathStar.json";
        }
        return L"DeathStar.json";
    }

    void Config::Load()
    {
        std::wstring wpath = GetConfigPath();
        std::ifstream file(wpath);
        if (!file.is_open())
        {
            Reset();
            return;
        }

        std::stringstream ss;
        ss << file.rdbuf();

        try
        {
            json j = json::parse(ss.str());

            if (j["autoClickerEnabled"].is_boolean())
                settings.autoClickerEnabled = j["autoClickerEnabled"].get<bool>();
            if (j["autoClickerMinCps"].is_number())
                settings.autoClickerMinCps = j["autoClickerMinCps"].get<int>();
            if (j["autoClickerMaxCps"].is_number())
                settings.autoClickerMaxCps = j["autoClickerMaxCps"].get<int>();
            if (j["autoClickerJitter"].is_boolean())
                settings.autoClickerJitter = j["autoClickerJitter"].get<bool>();
            if (j["autoClickerOnlyWhileHolding"].is_boolean())
                settings.autoClickerOnlyWhileHolding = j["autoClickerOnlyWhileHolding"].get<bool>();
            if (j["autoClickerDontClickInMenus"].is_boolean())
                settings.autoClickerDontClickInMenus = j["autoClickerDontClickInMenus"].get<bool>();
            if (j["autoClickerBreakBlocks"].is_boolean())
                settings.autoClickerBreakBlocks = j["autoClickerBreakBlocks"].get<bool>();
            if (j["autoClickerKeybind"].is_number())
                settings.autoClickerKeybind = j["autoClickerKeybind"].get<int>();
            if (j["noHitDelayEnabled"].is_boolean())
                settings.noHitDelayEnabled = j["noHitDelayEnabled"].get<bool>();
            if (j["noJumpDelayEnabled"].is_boolean())
                settings.noJumpDelayEnabled = j["noJumpDelayEnabled"].get<bool>();

            if (j["accentColor"].is_array() && j["accentColor"].size() == 4)
            {
                for (int i = 0; i < 4; i++)
                {
                    if (j["accentColor"][i].is_number())
                        ui.accent[i] = j["accentColor"][i].get<float>();
                }
            }

            if (j["watermarkEnabled"].is_boolean())
                watermark.enabled = j["watermarkEnabled"].get<bool>();
            if (j["watermarkCorner"].is_number())
                watermark.corner = j["watermarkCorner"].get<int>();
            if (j["watermarkName"].is_boolean())
                watermark.showName = j["watermarkName"].get<bool>();
            if (j["watermarkUser"].is_boolean())
                watermark.showUser = j["watermarkUser"].get<bool>();
            if (j["watermarkFps"].is_boolean())
                watermark.showFps = j["watermarkFps"].get<bool>();
            if (j["watermarkCps"].is_boolean())
                watermark.showCps = j["watermarkCps"].get<bool>();
            if (j["watermarkTime"].is_boolean())
                watermark.showTime = j["watermarkTime"].get<bool>();
        }
        catch (...)
        {
            Reset();
        }
    }

    void Config::Save()
    {
        json j = json::object();
        j["autoClickerEnabled"] = settings.autoClickerEnabled;
        j["autoClickerMinCps"] = settings.autoClickerMinCps;
        j["autoClickerMaxCps"] = settings.autoClickerMaxCps;
        j["autoClickerJitter"] = settings.autoClickerJitter;
        j["autoClickerOnlyWhileHolding"] = settings.autoClickerOnlyWhileHolding;
        j["autoClickerDontClickInMenus"] = settings.autoClickerDontClickInMenus;
        j["autoClickerBreakBlocks"] = settings.autoClickerBreakBlocks;
        j["autoClickerKeybind"] = settings.autoClickerKeybind;
        j["noHitDelayEnabled"] = settings.noHitDelayEnabled;
        j["noJumpDelayEnabled"] = settings.noJumpDelayEnabled;
        j["accentColor"] = { ui.accent[0], ui.accent[1], ui.accent[2], ui.accent[3] };
        j["watermarkEnabled"] = watermark.enabled;
        j["watermarkCorner"] = watermark.corner;
        j["watermarkName"] = watermark.showName;
        j["watermarkUser"] = watermark.showUser;
        j["watermarkFps"] = watermark.showFps;
        j["watermarkCps"] = watermark.showCps;
        j["watermarkTime"] = watermark.showTime;

        std::wstring wpath = GetConfigPath();
        std::ofstream file(wpath);
        if (file.is_open())
        {
            file << j.dump(4);
        }
    }

    void Config::Reset()
    {
        settings = ModuleSettings();
        ui = UiSettings();
        watermark = WatermarkSettings();
    }

    void Config::ApplyTo(shared::SharedConfig& target) const
    {
        target.autoClickerEnabled = settings.autoClickerEnabled;
        target.autoClickerMinCps = settings.autoClickerMinCps;
        target.autoClickerMaxCps = settings.autoClickerMaxCps;
        target.autoClickerJitter = settings.autoClickerJitter;
        target.autoClickerOnlyWhileHolding = settings.autoClickerOnlyWhileHolding;
        target.autoClickerDontClickInMenus = settings.autoClickerDontClickInMenus;
        target.autoClickerBreakBlocks = settings.autoClickerBreakBlocks;
        target.autoClickerKeybind = settings.autoClickerKeybind;
        target.noHitDelayEnabled = settings.noHitDelayEnabled;
        target.noJumpDelayEnabled = settings.noJumpDelayEnabled;

        target.watermarkEnabled = watermark.enabled;
        target.watermarkCorner = watermark.corner;
        target.watermarkName = watermark.showName;
        target.watermarkUser = watermark.showUser;
        target.watermarkFps = watermark.showFps;
        target.watermarkCps = watermark.showCps;
        target.watermarkTime = watermark.showTime;

        // One accent for the whole thing, menu and watermark alike.
        target.watermarkColor[0] = ui.accent[0];
        target.watermarkColor[1] = ui.accent[1];
        target.watermarkColor[2] = ui.accent[2];
    }
}
