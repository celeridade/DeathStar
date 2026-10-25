#pragma once
#include <windows.h>
#include <cstdint>

#ifdef OMC_BUILD_DLL
#define OMC_DLL __declspec(dllexport)
#else
#define OMC_DLL __declspec(dllimport)
#endif

namespace omega::shared {
    inline const wchar_t* PROCESS_NAMES[] = { L"minecraft.exe", L"javaw.exe", L"java.exe" };
    inline constexpr int PROCESS_NAME_COUNT = 3;
    inline constexpr const wchar_t* NAME = L"DeathStar";
    inline constexpr const wchar_t* DLL_NAME = L"DeathStar.dll";
    inline constexpr const wchar_t* WINDOW_CLASS = L"DeathStarLoader";
    inline constexpr int WINDOW_WIDTH = 900;
    inline constexpr int WINDOW_HEIGHT = 515;
    inline constexpr const wchar_t* SHARED_MEM_NAME = L"DeathStar::SharedConfig";
    inline constexpr const wchar_t* MUTEX_NAME = L"DeathStar::Loader";

    struct SharedConfig {
        bool autoClickerEnabled = false;
        int autoClickerMinCps = 8;
        int autoClickerMaxCps = 12;
        bool autoClickerJitter = false;
        bool autoClickerOnlyWhileHolding = true;
        bool autoClickerDontClickInMenus = true;
        bool autoClickerBreakBlocks = false;
        bool autoClickerBurst = false;   // pin straight to max CPS, no random pacing
        // Virtual-key code that toggles the clicker on/off. 0 = unbound (always
        // on). 0x05/0x06 are the side mouse buttons (M4/M5). Hitting is always M1.
        int autoClickerKeybind = 0;
        char altManagerUsername[17]{};
        std::uint32_t altManagerRequest = 0;
        std::uint32_t altManagerAcknowledged = 0;
        int altManagerResult = 0;
        bool noHitDelayEnabled = false;
        bool noJumpDelayEnabled = false;

        // Watermark drawn inside the game, not in the loader window.
        bool watermarkEnabled = true;
        int watermarkCorner = 0;        // 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right
        bool watermarkName = true;
        bool watermarkUser = true;
        bool watermarkFps = true;
        bool watermarkCps = false;
        bool watermarkTime = true;
        float watermarkColor[3] = { 222 / 255.f, 51 / 255.f, 61 / 255.f };

        // Written by the DLL, read by the Loader.
    bool dllAttached = false;
    // Written by the Loader, read by the DLL: asks it to unload itself.
    bool shutdownRequested = false;
    // Written by the Loader, read by the DLL: dll was manual-mapped into the
    // target, so it must NOT call FreeLibraryAndExitThread on shutdown (there
    // is no module list entry). The loader frees the region itself.
    bool manualMapped = false;
    };
}
