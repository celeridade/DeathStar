#pragma once
#include <jni.h>
#include <string>

namespace omega::dll::features {
    class AltManager {
    public:
        bool Apply(JNIEnv* env, const std::string& username);
        bool Reset(JNIEnv* env);
        void Release(JNIEnv* env);

    private:
        jobject m_originalSession = nullptr;
    };
}
