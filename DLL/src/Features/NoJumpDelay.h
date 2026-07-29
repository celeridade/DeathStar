#pragma once
#include <atomic>
#include <jni.h>

namespace omega::dll::features {
    class NoJumpDelay {
    public:
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        void Tick(JNIEnv* env);
    private:
        std::atomic<bool> m_enabled{ false };
    };
}
