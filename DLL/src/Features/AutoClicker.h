#pragma once
#include <atomic>
#include <chrono>
#include <random>
#include <windows.h>
#include <jni.h>

namespace omega::dll::features {
    class AutoClicker {
    public:
        AutoClicker();
        ~AutoClicker();

        void Start();
        void Stop();

        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        void SetMinCPS(int cps);
        int GetMinCPS() const;
        void SetMaxCPS(int cps);
        int GetMaxCPS() const;
        void SetJitter(bool enabled);
        bool GetJitter() const;
        void SetOnlyWhileHolding(bool enabled);
        bool GetOnlyWhileHolding() const;
        void SetDontClickInMenus(bool enabled);
        bool GetDontClickInMenus() const;
        void SetBreakBlocks(bool enabled);
        bool GetBreakBlocks() const;
        void SetKeybind(int vk) { m_keybind.store(vk); }
        int GetKeybind() const { return m_keybind.load(); }

        // True once if the keybind was pressed since the last call. The owner
        // uses it to flip the enabled flag in shared memory so the menu's
        // checkbox tracks the in-game toggle.
        bool ConsumeToggle() { return m_togglePending.exchange(false); }

        // Reads the game state the clicker needs and writes it into the atomics
        // below. Must only be called from the thread that is attached to the JVM,
        // the clicker thread never touches JNI itself.
        void UpdateFromGame(JNIEnv* env);

        // Every click that reached the game, whether posted by us or pressed by
        // hand. The watermark turns the delta into clicks per second.
        unsigned int GetClickCount() const { return m_clickCount.load(); }

    private:
        void Run();
        static DWORD WINAPI ThreadProc(LPVOID lpParam);

        std::atomic<bool> m_enabled{ false };
        std::atomic<bool> m_running{ false };
        std::atomic<int> m_minCps{ 10 };
        std::atomic<int> m_maxCps{ 14 };
        std::atomic<bool> m_jitter{ false };
        std::atomic<bool> m_onlyWhileHolding{ true };
        std::atomic<bool> m_dontClickInMenus{ true };
        std::atomic<bool> m_breakBlocks{ false };
        std::atomic<int> m_keybind{ 0 };            // toggle key; 0 = unbound
        std::atomic<bool> m_togglePending{ false }; // set on a keybind press

        // Snapshots of the game, refreshed by UpdateFromGame.
        std::atomic<bool> m_guiOpen{ false };
        std::atomic<bool> m_lookingAtBlock{ false };
        std::atomic<unsigned int> m_clickCount{ 0 };

        HANDLE m_thread = nullptr;
    };
}
