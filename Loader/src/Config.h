#pragma once
#include <string>
#include "Shared.h"

namespace omega::loader {
    struct ModuleSettings {
        bool autoClickerEnabled = false;
        int autoClickerMinCps = 10;
        int autoClickerMaxCps = 14;
        bool autoClickerJitter = false;
        bool autoClickerOnlyWhileHolding = true;
        bool autoClickerDontClickInMenus = true;
        bool autoClickerBreakBlocks = false;
        bool autoClickerBurst = false;   // pin straight to max CPS, no random pacing
        int autoClickerKeybind = 0;   // VK toggle; 0 = unbound (always on), 0x05/0x06 = side buttons
        bool blockHitEnabled = false;
        int blockHitMode = 0;         // 0 = Manual, 1 = Automatic
        int blockHitDelayMs = 75;

        bool noHitDelayEnabled = false;
        bool noJumpDelayEnabled = false;
    };

    struct UiSettings {
        float accent[4] = { 222 / 255.f, 51 / 255.f, 61 / 255.f, 1.f };
    };

    // Drawn by the injected dll on top of the game, not in this window.
    struct WatermarkSettings {
        bool enabled = true;
        int corner = 0;          // 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right
        bool showName = true;
        bool showUser = true;
        bool showFps = true;
        bool showCps = false;
        bool showTime = true;
    };

    class Config {
    public:
        Config();
        void Load();
        void Save();
        void Reset();

        // Pushes the module settings into the memory the injected DLL reads.
        void ApplyTo(shared::SharedConfig& target) const;

        ModuleSettings settings;
        UiSettings ui;
        WatermarkSettings watermark;
    };
}
