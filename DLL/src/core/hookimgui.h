#pragma once
#include <atomic>
#include <jni.h>

namespace omega::dll::core {

class HideClan {
public:
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled; }

    void SetKeybind(int vk) { m_keybind = vk; }
    int GetKeybind() const { return m_keybind; }

    void SetShowAllies(bool show) { m_showAllies = show; }
    void SetMode(int mode) { m_mode = mode; }
    void SetShowCount(int count) { m_showCount = count; }

    bool ConsumeToggle() { return m_togglePending.exchange(false); }

    void Tick(JNIEnv* env);
    void Shutdown(JNIEnv* env);

private:
    void HidePlayer(JNIEnv* env, jobject player);
    void RestorePlayer(JNIEnv* env, jobject player);

    bool m_enabled = false;
    int m_keybind = 0;
    bool m_showAllies = false;
    int m_mode = 0;
    int m_showCount = 5;

    std::atomic<bool> m_togglePending{ false };
};

} // namespace omega::dll::core
