#pragma once
#include <atomic>
#include <jni.h>

namespace omega::dll::features {
    class NoHitDelay {
    public:
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        void Tick(JNIEnv* env);
    private:
        std::atomic<bool> m_enabled{ false };
    };
}
