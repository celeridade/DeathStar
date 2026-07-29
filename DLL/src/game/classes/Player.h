#pragma once
#include <jni.h>

class Player {
public:
    static void SetJumpTicks(jobject player, int ticks, JNIEnv* env);
};
