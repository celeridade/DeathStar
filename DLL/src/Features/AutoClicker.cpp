#include "AutoClicker.h"
#include "../game/jni/GameInstance.h"
#include "../game/jni/JniUtil.h"
#include "../game/classes/Minecraft.h"
#include "../game/mapping/Mapper.h"
#include <random>
#include <chrono>
#include <thread>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace omega::dll::features {

    static void SleepUntilSteadyClock(HANDLE timer, std::chrono::steady_clock::time_point deadline)
    {
        if (!timer)
        {
            std::this_thread::sleep_until(deadline);
            return;
        }

        while (true)
        {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero())
                return;

            const auto remainingNs = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count();
            LARGE_INTEGER dueTime{};
            dueTime.QuadPart = -static_cast<LONGLONG>((remainingNs + 99) / 100);
            if (!SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, FALSE))
            {
                std::this_thread::sleep_until(deadline);
                return;
            }

            if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
            {
                std::this_thread::sleep_until(deadline);
                return;
            }
        }
    }

    AutoClicker::AutoClicker() = default;

    AutoClicker::~AutoClicker()
    {
        Stop();
    }

    void AutoClicker::Start()
    {
        if (m_running.load())
            return;

        m_running.store(true);
        m_thread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
    }

    void AutoClicker::Stop()
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

    void AutoClicker::SetEnabled(bool enabled) { m_enabled.store(enabled); }
    bool AutoClicker::IsEnabled() const { return m_enabled.load(); }
    void AutoClicker::SetMinCPS(int cps) { if (cps < 1) cps = 1; if (cps > 25) cps = 25; m_minCps.store(cps); }
    int AutoClicker::GetMinCPS() const { return m_minCps.load(); }
    void AutoClicker::SetMaxCPS(int cps) { if (cps < 1) cps = 1; if (cps > 25) cps = 25; m_maxCps.store(cps); }
    int AutoClicker::GetMaxCPS() const { return m_maxCps.load(); }
    void AutoClicker::SetJitter(bool enabled) { m_jitter.store(enabled); }
    bool AutoClicker::GetJitter() const { return m_jitter.load(); }
    void AutoClicker::SetOnlyWhileHolding(bool enabled) { m_onlyWhileHolding.store(enabled); }
    bool AutoClicker::GetOnlyWhileHolding() const { return m_onlyWhileHolding.load(); }
    void AutoClicker::SetDontClickInMenus(bool enabled) { m_dontClickInMenus.store(enabled); }
    bool AutoClicker::GetDontClickInMenus() const { return m_dontClickInMenus.load(); }
    void AutoClicker::SetBreakBlocks(bool enabled) { m_breakBlocks.store(enabled); }
    bool AutoClicker::GetBreakBlocks() const { return m_breakBlocks.load(); }

    DWORD WINAPI AutoClicker::ThreadProc(LPVOID lpParam)
    {
        auto* self = static_cast<AutoClicker*>(lpParam);
        self->Run();
        return 0;
    }

    static bool IsMopTargetingBlock(jobject mop, JNIEnv* env)
    {
        if (!mop || !env)
            return false;

        std::string typeFieldName = Mapper::Get("typeOfHit", FIELD_MAPPING);
        if (typeFieldName.empty())
            return false;

        jclass mopClazz = env->GetObjectClass(mop);
        if (!mopClazz)
        {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }

        // Reflection so the enum's obfuscated type signature never matters.
        jfieldID typeField = jniutil::FieldByName(env, mopClazz, typeFieldName.c_str(), jniutil::FieldKind::Object);
        env->DeleteLocalRef(mopClazz);

        if (!typeField)
            return false;

        jobject type = env->GetObjectField(mop, typeField);
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return false;
        }
        if (!type)
            return false;

        jclass enumClazz = env->GetObjectClass(type);
        jmethodID ordinalMethod = env->GetMethodID(enumClazz, "ordinal", "()I");
        if (!ordinalMethod)
        {
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(enumClazz);
            env->DeleteLocalRef(type);
            return false;
        }

        jint ord = env->CallIntMethod(type, ordinalMethod);
        env->DeleteLocalRef(enumClazz);
        env->DeleteLocalRef(type);

        return ord == 1;
    }

    void AutoClicker::UpdateFromGame(JNIEnv* env)
    {
        if (!env)
            return;

        if (!m_enabled.load())
        {
            m_guiOpen.store(false);
            m_lookingAtBlock.store(false);
            return;
        }

        jobject screen = Minecraft::GetCurrentScreen(env);
        m_guiOpen.store(screen != nullptr);
        if (screen)
            env->DeleteLocalRef(screen);

        if (m_breakBlocks.load())
        {
            bool isBlock = false;
            jobject mop = Minecraft::GetObjectMouseOver(env);
            if (mop)
            {
                isBlock = IsMopTargetingBlock(mop, env);
                env->DeleteLocalRef(mop);
            }
            m_lookingAtBlock.store(isBlock);
        }
        else
        {
            m_lookingAtBlock.store(false);
        }

        Minecraft::SetLeftClickCounter(0, env);
    }

    void AutoClicker::Run()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        HANDLE highResolutionTimer = CreateWaitableTimerExW(nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

        bool previousLmb = false;
        bool previousBind = false;
        bool holdingMine = false;   // LMB held down (not spammed) to break a block
        int fixedRateCps = 0;
        auto nextFixedClick = std::chrono::steady_clock::time_point{};
        HWND mineWindow = nullptr;

        auto releaseMine = [&]() {
            if (holdingMine && mineWindow)
                PostMessageA(mineWindow, WM_LBUTTONUP, 0, 0);
            holdingMine = false;
            mineWindow = nullptr;
        };

        // No JNI in this loop on purpose. This thread is not the one attached to
        // the JVM, so it reads the snapshots UpdateFromGame leaves in the atomics.
        while (m_running.load())
        {
            // Counted even while disabled so the watermark shows real clicks too.
            const bool lmbNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (lmbNow && !previousLmb)
                m_clickCount.fetch_add(1);
            previousLmb = lmbNow;

            HWND minecraftWindow = FindWindowA("LWJGL", nullptr);
            const bool inGame = minecraftWindow && GetForegroundWindow() == minecraftWindow;

            // The keybind toggles the clicker on and off by flipping the enabled
            // flag; the owner picks up ConsumeToggle and updates shared memory so
            // the menu checkbox tracks it. Only while the game is focused, so a
            // side-button click elsewhere does not flip it. Hitting is always LMB.
            const int keybind = m_keybind.load();
            const bool bindNow = keybind != 0 && inGame && (GetAsyncKeyState(keybind) & 0x8000) != 0;
            if (bindNow && !previousBind)
                m_togglePending.store(true);
            previousBind = bindNow;

            const bool m1Held = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

            const bool gate =
                m_enabled.load() && inGame &&
                !(m_dontClickInMenus.load() && m_guiOpen.load()) &&
                !(m_onlyWhileHolding.load() && !m1Held);

            if (!gate)
            {
                releaseMine();
                fixedRateCps = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (m_lookingAtBlock.load())
            {
                if (m_breakBlocks.load())
                {
                    // Hold left click down so the block actually breaks; spamming
                    // clicks would keep resetting the mining progress.
                    if (!holdingMine)
                    {
                        PostMessageA(minecraftWindow, WM_LBUTTONDOWN, MK_LBUTTON, 0);
                        holdingMine = true;
                        mineWindow = minecraftWindow;
                    }
                    fixedRateCps = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                // Break Blocks off: leave blocks alone so builds survive.
                releaseMine();
                fixedRateCps = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Back to normal combat clicking; drop any mining hold first.
            releaseMine();

            int minCps = m_minCps.load();
            int maxCps = m_maxCps.load();
            if (minCps > maxCps)
                std::swap(minCps, maxCps);

            const bool burst = m_burst.load();
            const int targetCps = burst ? maxCps : minCps;
            const bool fixedRate = burst || minCps == maxCps;
            if (fixedRate)
            {
                if (fixedRateCps != targetCps)
                {
                    fixedRateCps = targetCps;
                    nextFixedClick = std::chrono::steady_clock::now();
                }
            }
            else
            {
                fixedRateCps = 0;
            }

            PostMessageA(minecraftWindow, WM_LBUTTONDOWN, MK_LBUTTON, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            PostMessageA(minecraftWindow, WM_LBUTTONUP, 0, 0);

            m_clickCount.fetch_add(1);

            if (fixedRate)
            {
                const auto interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(1.0 / targetCps));
                nextFixedClick += interval;
                const auto now = std::chrono::steady_clock::now();
                if (nextFixedClick <= now)
                    nextFixedClick = now + interval;
                SleepUntilSteadyClock(highResolutionTimer, nextFixedClick);
            }
            else
            {
                int minDelay = 1000 / maxCps;
                int maxDelay = 1000 / minCps;
                if (minDelay < 1) minDelay = 1;
                if (maxDelay < minDelay) maxDelay = minDelay;

                std::uniform_int_distribution<> delayDist(minDelay, maxDelay);
                std::this_thread::sleep_for(std::chrono::milliseconds(delayDist(gen)));
            }

            if (m_jitter.load())
            {
                std::uniform_int_distribution<> jitterDist(-1, 1);
                int dx = jitterDist(gen);
                int dy = jitterDist(gen);
                if (dx != 0 || dy != 0)
                {
                    INPUT input = {};
                    input.type = INPUT_MOUSE;
                    input.mi.dx = dx;
                    input.mi.dy = dy;
                    input.mi.dwFlags = MOUSEEVENTF_MOVE;
                    SendInput(1, &input, sizeof(INPUT));
                }
            }
        }

        releaseMine();
        if (highResolutionTimer)
            CloseHandle(highResolutionTimer);
    }

}
