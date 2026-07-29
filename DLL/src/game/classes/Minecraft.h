#pragma once
#include <jni.h>
#include <string>

class Minecraft {
public:
    static jobject GetTheMinecraft(JNIEnv* env);
    static jobject GetThePlayer(JNIEnv* env);
    static jobject GetTheWorld(JNIEnv* env);
    static jobject GetCurrentScreen(JNIEnv* env);
    static jobject GetPlayerController(JNIEnv* env);
    static jobject GetObjectMouseOver(JNIEnv* env);
    static void SetLeftClickCounter(int value, JNIEnv* env);
    static void SetRightClickCounter(int value, JNIEnv* env);

    // Empty when the game has not reached a world yet.
    static std::string GetPlayerName(JNIEnv* env);
};
