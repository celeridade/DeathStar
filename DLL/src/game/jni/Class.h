#pragma once
#include <jni.h>
#include <string>

class Field;
class Method;

class Class {
public:
    std::string GetName(JNIEnv* env);
    Field* GetField(JNIEnv* env, const char* name, const char* sig, bool staticField = false);
    Method* GetMethod(JNIEnv* env, const char* name, const char* sig, bool staticMethod = false);

private:
    jclass m_Class = nullptr;
};
