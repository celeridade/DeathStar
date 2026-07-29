#pragma once
#include "Class.h"
#include "../mapping/Mapper.h"
#include <jni.h>
#include <unordered_map>
#include <string>

class GameInstance {
public:
    GameInstance();
    JNIEnv* GetENV() const { return m_Env; }
    JNIEnv* GetCurrentEnv() const;
    bool Attach();
    void Detach();

    // A JNIEnv belongs to one thread and one thread only. Any native thread that
    // wants to touch the game has to attach itself and use the env it gets back,
    // never the one cached in m_Env.
    JNIEnv* AttachCurrentThreadEnv(bool* outAttached = nullptr);
    void DetachCurrentThreadEnv();
    bool InitializeGame();
    Class* FindClass(const std::string& className);

    // env->FindClass only sees the bootstrap and system loaders. Minecraft is
    // loaded by the launcher's own ClassLoader, so anything of the game has to
    // be resolved through a loader that can actually see it.
    static jclass ResolveClass(JNIEnv* env, const std::string& className);
    GameVersions GetGameVersion() const { return m_GameVersion; }
    bool IsInitialized() const { return m_Initialized; }

private:
    GameVersions DetectGameVersion();
    bool m_Initialized = false;
    GameVersions m_GameVersion = UNKNOWN;
    std::unordered_map<std::string, Class*> m_CachedClasses;
    JavaVM* m_Jvm = nullptr;
    JNIEnv* m_Env = nullptr;
};

extern GameInstance* g_Game;
