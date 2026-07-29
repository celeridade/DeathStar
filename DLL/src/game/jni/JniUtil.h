#pragma once
#include <jni.h>

namespace jniutil
{
    // GetFieldID needs the exact type signature, which nobody can spell on an
    // obfuscated client. Reflection only needs the name, so it is what the game
    // classes use to reach their fields.
    jfieldID FieldByName(JNIEnv* env, jclass clazz, const char* name);

    // Finds the static field a class holds of its own type and returns its value.
    // That is how Minecraft keeps its singleton, and it needs no field mapping.
    jobject StaticSelfInstance(JNIEnv* env, jclass clazz);
}
