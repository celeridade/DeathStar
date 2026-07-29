#pragma once
#include <jni.h>

class Method {
public:
    jobject CallObjectMethod(JNIEnv* env, void* owner, bool staticMethod = false, ...);
    void CallVoidMethod(JNIEnv* env, void* owner, bool staticMethod = false, ...);
    int CallIntMethod(JNIEnv* env, void* owner, bool staticMethod = false, ...);
    bool CallBooleanMethod(JNIEnv* env, void* owner, bool staticMethod = false, ...);

private:
    jmethodID m_MethodID = nullptr;
};
