#pragma once
#include <jni.h>

namespace jniutil
{
    // What the caller intends to do with the field. Resolving by name alone is
    // not enough: an obfuscated mapping can point at a real field of the wrong
    // type, and writing an int into an object slot corrupts the JVM heap.
    enum class FieldKind
    {
        Any,
        Int,
        Float,
        Boolean,
        Object,
    };

    // GetFieldID needs the exact type signature, which nobody can spell on an
    // obfuscated client. Reflection only needs the name, so it is what the game
    // classes use to reach their fields. The kind is verified before the id is
    // handed back, so a stale mapping fails safely instead of corrupting memory.
    jfieldID FieldByName(JNIEnv* env, jclass clazz, const char* name, FieldKind kind = FieldKind::Any);

    // Finds the static field a class holds of its own type and returns its value.
    // That is how Minecraft keeps its singleton, and it needs no field mapping.
    jobject StaticSelfInstance(JNIEnv* env, jclass clazz);
}
