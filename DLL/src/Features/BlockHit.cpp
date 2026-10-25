#include "BlockHit.h"
#include "../game/classes/Minecraft.h"
#include <chrono>
#include <thread>

namespace omega::dll::features {
    BlockHit::~BlockHit()
    {
        Stop();
    }

    void BlockHit::Start()
    {
        if (m_running.exchange(true))
            return;

        m_thread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
    }

    void BlockHit::Stop()
    {
        m_running.store(false);
        m_enabled.store(false);
        if (m_thread)
        {
            WaitForSingleObject(m_thread, 1000);
            CloseHandle(m_thread);
            m_thread = nullptr;
        }
    }

    void BlockHit::SetEnabled(bool enabled)
    {
        m_enabled.store(enabled);
    }

    void BlockHit::SetMode(int mode)
    {
        m_mode.store(mode == 1 ? 1 : 0);
    }

    void BlockHit::SetDelayMs(int delayMs)
    {
        if (delayMs < 0) delayMs = 0;
        if (delayMs > 150) delayMs = 150;
        m_delayMs.store(delayMs);
    }

    void BlockHit::UpdateFromGame(JNIEnv* env)
    {
        if (!env || !m_enabled.load())
        {
            m_guiOpen.store(false);
            return;
        }

        jobject screen = Minecraft::GetCurrentScreen(env);
        m_guiOpen.store(screen != nullptr);
        if (screen)
            env->DeleteLocalRef(screen);
    }

    DWORD WINAPI BlockHit::ThreadProc(LPVOID lpParam)
    {
        auto* self = static_cast<BlockHit*>(lpParam);
        self->Run();
        return 0;
    }

    void BlockHit::Run()
    {
        HWND targetWindow = nullptr;
        bool buttonHeld = false;
        auto nextClick = std::chrono::steady_clock::now();

        const auto release = [&]() {
            if (buttonHeld && targetWindow)
                PostMessageA(targetWindow, WM_RBUTTONUP, 0, 0);
            buttonHeld = false;
        };

        while (m_running.load())
        {
            HWND gameWindow = FindWindowA("LWJGL", nullptr);
            const bool focused = gameWindow && GetForegroundWindow() == gameWindow;
            const int mode = m_mode.load();
            const bool triggerHeld = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            const bool shouldBlock = m_enabled.load() && focused && triggerHeld && !m_guiOpen.load();

            if (!shouldBlock)
            {
                release();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            if (targetWindow != gameWindow)
            {
                release();
                targetWindow = gameWindow;
            }

            const auto now = std::chrono::steady_clock::now();
            if (!buttonHeld && now >= nextClick)
            {
                PostMessageA(gameWindow, WM_RBUTTONDOWN, MK_RBUTTON, 0);
                buttonHeld = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                release();
                const int delayMs = mode == 1 ? m_delayMs.load() : 75;
                nextClick = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        release();
    }
}
