#pragma once
#include <atomic>
#include <windows.h>
#include <jni.h>

namespace omega::dll::features {
    class BlockHit {
    public:
        BlockHit() = default;
        ~BlockHit();

        void Start();
        void Stop();
        void SetEnabled(bool enabled);
        void SetMode(int mode);
        void SetDelayMs(int delayMs);
        void UpdateFromGame(JNIEnv* env);

    private:
        void Run();
        static DWORD WINAPI ThreadProc(LPVOID lpParam);

        std::atomic<bool> m_enabled{ false };
        std::atomic<int> m_mode{ 0 };
        std::atomic<int> m_delayMs{ 75 };
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_guiOpen{ false };
        HANDLE m_thread = nullptr;
    };
}
