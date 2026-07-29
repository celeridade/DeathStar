#pragma once
#include <jni.h>

class Field {
public:
    jobject GetObjectField(JNIEnv* env, void* fieldOwner, bool staticField = false);
    int GetIntField(JNIEnv* env, void* fieldOwner, bool staticField = false);
    bool GetBooleanField(JNIEnv* env, void* fieldOwner, bool staticField = false);
    void SetIntField(JNIEnv* env, void* fieldOwner, int value, bool staticField = false);
    void SetObjectField(JNIEnv* env, void* fieldOwner, jobject value, bool staticField = false);

private:
    jfieldID m_FieldID = nullptr;
};
