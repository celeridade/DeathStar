#include "AltManager.h"
#include "../game/classes/Minecraft.h"
#include "../game/jni/GameInstance.h"
#include "../game/mapping/Mapper.h"
#include <regex>

namespace omega::dll::features {
    bool AltManager::Apply(JNIEnv* env, const std::string& username)
    {
        if (!env || !std::regex_match(username, std::regex("[A-Za-z0-9_]{3,16}")))
            return false;

        jobject minecraft = Minecraft::GetTheMinecraft(env);
        if (!minecraft)
            return false;

        jobject session = Minecraft::GetSession(env);
        if (!session)
        {
            env->DeleteLocalRef(minecraft);
            return false;
        }

        if (!m_originalSession)
            m_originalSession = env->NewGlobalRef(session);
        if (!m_originalSession)
        {
            env->DeleteLocalRef(session);
            env->DeleteLocalRef(minecraft);
            return false;
        }

        const std::string sessionClassName = Mapper::Get("Session", CLASS_MAPPING);
        jclass sessionClass = GameInstance::ResolveClass(env, sessionClassName);
        jmethodID constructor = sessionClass
            ? env->GetMethodID(sessionClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V")
            : nullptr;
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            constructor = nullptr;
        }

        jobject replacement = nullptr;
        if (constructor)
        {
            jstring name = env->NewStringUTF(username.c_str());
            jstring id = env->NewStringUTF("00000000-0000-0000-0000-000000000000");
            jstring token = env->NewStringUTF("");
            jstring type = env->NewStringUTF("legacy");
            replacement = env->NewObject(sessionClass, constructor, name, id, token, type);
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
                replacement = nullptr;
            }
            env->DeleteLocalRef(name);
            env->DeleteLocalRef(id);
            env->DeleteLocalRef(token);
            env->DeleteLocalRef(type);
        }

        const bool applied = replacement && Minecraft::SetSession(env, minecraft, replacement);
        if (replacement)
            env->DeleteLocalRef(replacement);
        if (sessionClass)
            env->DeleteLocalRef(sessionClass);
        env->DeleteLocalRef(session);
        env->DeleteLocalRef(minecraft);
        return applied;
    }

    bool AltManager::Reset(JNIEnv* env)
    {
        if (!env || !m_originalSession)
            return false;

        jobject minecraft = Minecraft::GetTheMinecraft(env);
        if (!minecraft)
            return false;

        const bool restored = Minecraft::SetSession(env, minecraft, m_originalSession);
        env->DeleteLocalRef(minecraft);
        if (restored)
        {
            env->DeleteGlobalRef(m_originalSession);
            m_originalSession = nullptr;
        }
        return restored;
    }

    void AltManager::Release(JNIEnv* env)
    {
        if (m_originalSession)
        {
            if (env)
                env->DeleteGlobalRef(m_originalSession);
            m_originalSession = nullptr;
        }
    }
}
