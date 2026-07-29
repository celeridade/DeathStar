#include <windows.h>
#include <thread>
#include <chrono>
#include "Shared.h"
#include "game/jni/GameInstance.h"
#include "Features/AutoClicker.h"
#include "Features/NoHitDelay.h"
#include "Features/NoJumpDelay.h"
#include "features/render/Watermark.h"
#include "game/mapping/Mapper.h"
#include "core/RenderHook.h"
#include "core/hookimgui.h"

#include "MinHook.h"

using namespace omega::dll::features;
using omega::dll::features::render::Watermark;

static AutoClicker* g_autoClicker = nullptr;
static NoHitDelay* g_noHitDelay = nullptr;
static NoJumpDelay* g_noJumpDelay = nullptr;
static omega::dll::core::HideClan* g_hideClan = nullptr;
static omega::shared::SharedConfig* g_sharedConfig = nullptr;
static HANDLE g_hMapFile = nullptr;
static HMODULE g_hModule = nullptr;
static bool g_running = false;
static bool g_minHookReady = false;

static void SyncSettings() {
    if (!g_sharedConfig) return;

    // A keybind press in-game flips the enabled flag, which flows back to the
    // loader so the checkbox tracks it.
    if (g_autoClicker && g_autoClicker->ConsumeToggle())
        g_sharedConfig->autoClickerEnabled = !g_sharedConfig->autoClickerEnabled;

    if (g_autoClicker) {
        g_autoClicker->SetEnabled(g_sharedConfig->autoClickerEnabled);
        g_autoClicker->SetMinCPS(g_sharedConfig->autoClickerMinCps);
        g_autoClicker->SetMaxCPS(g_sharedConfig->autoClickerMaxCps);
        g_autoClicker->SetJitter(g_sharedConfig->autoClickerJitter);
        g_autoClicker->SetOnlyWhileHolding(g_sharedConfig->autoClickerOnlyWhileHolding);
        g_autoClicker->SetDontClickInMenus(g_sharedConfig->autoClickerDontClickInMenus);
        g_autoClicker->SetBreakBlocks(g_sharedConfig->autoClickerBreakBlocks);
        g_autoClicker->SetKeybind(g_sharedConfig->autoClickerKeybind);

        Watermark::SetClickCount(g_autoClicker->GetClickCount());
    }

    if (g_noHitDelay)
        g_noHitDelay->SetEnabled(g_sharedConfig->noHitDelayEnabled);

    if (g_noJumpDelay)
        g_noJumpDelay->SetEnabled(g_sharedConfig->noJumpDelayEnabled);

    if (g_hideClan) {
        g_hideClan->SetEnabled(g_sharedConfig->hideClanEnabled);
        g_hideClan->SetKeybind(g_sharedConfig->hideClanKeybind);
        g_hideClan->SetShowAllies(g_sharedConfig->hideClanShowAllies);
        g_hideClan->SetMode(g_sharedConfig->hideClanMode);
        g_hideClan->SetShowCount(g_sharedConfig->hideClanShowCount);
    }
}

// Everything that touches the game runs here, on the thread that attached to
// the JVM, using that thread's own env. That is JNI-legal; the crash was a
// worker thread borrowing this env, which no longer happens.
static void TickGame(JNIEnv* env) {
    if (!env) return;

    // Injection often lands on the main menu, before the client's classes are
    // loaded, so the version can read UNKNOWN at first. Keep retrying until the
    // mappings actually load, otherwise every lookup runs against vanilla names.
    if (g_Game->GetGameVersion() == UNKNOWN)
        g_Game->InitializeGame();

    if (g_autoClicker)
        g_autoClicker->UpdateFromGame(env);

    if (g_noHitDelay)
        g_noHitDelay->Tick(env);

    if (g_noJumpDelay)
        g_noJumpDelay->Tick(env);

    if (g_hideClan)
        g_hideClan->Tick(env);

    static auto lastName = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastName).count() >= 1000) {
        lastName = now;
        Watermark::RefreshFromGame(env);
    }
}

static void MainThread() {
    // Open shared memory created by Loader
    g_hMapFile = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, omega::shared::SHARED_MEM_NAME);
    if (!g_hMapFile)
        return;

    g_sharedConfig = (omega::shared::SharedConfig*)MapViewOfFile(
        g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(omega::shared::SharedConfig));

    if (!g_sharedConfig) {
        CloseHandle(g_hMapFile);
        g_hMapFile = nullptr;
        return;
    }

    g_Game = new GameInstance();
    if (!g_Game->Attach()) {
        UnmapViewOfFile(g_sharedConfig);
        g_sharedConfig = nullptr;
        CloseHandle(g_hMapFile);
        g_hMapFile = nullptr;
        delete g_Game;
        g_Game = nullptr;
        return;
    }

    // Detects the client and loads its mappings. Retried in the loop if the game
    // is not far enough along yet.
    g_Game->InitializeGame();

    g_autoClicker = new AutoClicker();
    g_noHitDelay = new NoHitDelay();
    g_noJumpDelay = new NoJumpDelay();
    g_hideClan = new omega::dll::core::HideClan();

    Watermark::SetConfig(g_sharedConfig);

    SyncSettings();
    g_autoClicker->Start();

    // HideClan toggle via keybind syncs back to shared memory
    static bool lastHideClan = false;

    // Only the render hook needs MinHook now; it draws the watermark on the
    // game's own frame. The features run on this thread, no hook required.
    if (MH_Initialize() == MH_OK) {
        g_minHookReady = true;
        omega::dll::core::InstallRenderHook();
    }

    g_running = true;
    g_sharedConfig->dllAttached = true;

    auto lastSync = std::chrono::steady_clock::now();
    auto lastTick = std::chrono::steady_clock::now();

    while (g_running) {
        if (g_sharedConfig->shutdownRequested)
            break;

        // Keybind toggle for hideclan flips the shared flag
        if (g_hideClan && g_hideClan->ConsumeToggle()) {
            g_sharedConfig->hideClanEnabled = !g_sharedConfig->hideClanEnabled;
            lastHideClan = g_sharedConfig->hideClanEnabled;
        }

        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSync).count() >= 100) {
            SyncSettings();
            lastSync = now;
        }

        // ~60 Hz is plenty for a 20 TPS game and keeps the JNI cost low.
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() >= 16) {
            lastTick = now;
            TickGame(g_Game->GetCurrentEnv());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    omega::dll::core::RemoveRenderHook();

    if (g_minHookReady) {
        MH_Uninitialize();
        g_minHookReady = false;
    }

    g_autoClicker->Stop();

    delete g_autoClicker;
    g_autoClicker = nullptr;
    delete g_noHitDelay;
    g_noHitDelay = nullptr;
    delete g_noJumpDelay;
    g_noJumpDelay = nullptr;

    if (g_hideClan) {
        g_hideClan->Shutdown(g_Game->GetCurrentEnv());
        delete g_hideClan;
        g_hideClan = nullptr;
    }

    // Wipe the strings we brought in before the module leaves, so nothing of
    // ours is left sitting in the game's memory.
    Watermark::SetConfig(nullptr);
    Watermark::Shutdown();
    Mapper::Clear();

    g_Game->Detach();
    delete g_Game;
    g_Game = nullptr;

    if (g_sharedConfig) {
        g_sharedConfig->dllAttached = false;
        g_sharedConfig->shutdownRequested = false;
        UnmapViewOfFile(g_sharedConfig);
        g_sharedConfig = nullptr;
    }
    if (g_hMapFile) {
        CloseHandle(g_hMapFile);
        g_hMapFile = nullptr;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;

        HANDLE hThread = CreateThread(
            nullptr, 0,
            [](LPVOID) -> DWORD {
                MainThread();

                // Still running means the Loader asked us to unload: free ourselves.
                if (g_running) {
                    g_running = false;
                    FreeLibraryAndExitThread(g_hModule, 0);
                }

                return 0;
            },
            nullptr, 0, nullptr);

        if (hThread)
            CloseHandle(hThread);

        break;
    }
    case DLL_PROCESS_DETACH: {
        g_running = false;
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}
