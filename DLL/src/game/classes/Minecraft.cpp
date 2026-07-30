#include "Minecraft.h"
#include "../jni/GameInstance.h"
#include "../jni/JniUtil.h"
#include "../mapping/Mapper.h"
#include <string>
#include <unordered_map>

namespace
{
    // jfieldID stays valid while the class is loaded, and reflection to find one
    // is not cheap, so each is resolved once and kept. Only the game/main thread
    // touches these, so no locking is needed.
    jfieldID CachedField(JNIEnv* env, jobject owner, const char* mapping, jniutil::FieldKind kind)
    {
        static std::unordered_map<std::string, jfieldID> cache;

        const std::string name = Mapper::Get(mapping, FIELD_MAPPING);
        if (name.empty())
            return nullptr;

        auto it = cache.find(mapping);
        if (it != cache.end())
            return it->second;

        jclass clazz = env->GetObjectClass(owner);
        if (!clazz)
        {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return nullptr;
        }

        jfieldID field = jniutil::FieldByName(env, clazz, name.c_str(), kind);
        env->DeleteLocalRef(clazz);

        if (field)
            cache[mapping] = field;

        return field;
    }

    // Reads an object field off an instance, going through the instance's real
    // class so the field is found whatever the obfuscation did to the hierarchy.
    jobject ObjectField(JNIEnv* env, jobject owner, const char* mapping)
    {
        if (!env || !owner)
            return nullptr;

        jfieldID field = CachedField(env, owner, mapping, jniutil::FieldKind::Object);
        if (!field)
            return nullptr;

        jobject value = env->GetObjectField(owner, field);
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return nullptr;
        }

        return value;
    }

    void SetIntField(JNIEnv* env, jobject owner, const char* mapping, int value)
    {
        if (!env || !owner)
            return;

        jfieldID field = CachedField(env, owner, mapping, jniutil::FieldKind::Int);
        if (!field)
            return;

        env->SetIntField(owner, field, value);
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    std::string ReadString(JNIEnv* env, jstring value)
    {
        if (!value)
            return {};

        std::string result;
        if (const char* utf = env->GetStringUTFChars(value, nullptr))
        {
            result = utf;
            env->ReleaseStringUTFChars(value, utf);
        }
        env->DeleteLocalRef(value);
        return result;
    }

    // player.getGameProfile().getName(). GameProfile is com.mojang.authlib, a
    // library class the obfuscator leaves alone, so its getName is dependable.
    std::string NameFromProfile(JNIEnv* env, jobject player, jclass playerClass)
    {
        const std::string method = Mapper::Get("getGameProfile", METHOD_MAPPING);
        if (method.empty())
            return {};

        jmethodID getProfile = env->GetMethodID(playerClass, method.c_str(), "()Lcom/mojang/authlib/GameProfile;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!getProfile)
            return {};

        jobject profile = env->CallObjectMethod(player, getProfile);
        if (env->ExceptionCheck()) { env->ExceptionClear(); profile = nullptr; }
        if (!profile)
            return {};

        std::string result;
        jclass profileClass = env->GetObjectClass(profile);
        if (profileClass)
        {
            jmethodID getName = env->GetMethodID(profileClass, "getName", "()Ljava/lang/String;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (getName)
                result = ReadString(env, (jstring)env->CallObjectMethod(profile, getName));
            env->DeleteLocalRef(profileClass);
        }

        env->DeleteLocalRef(profile);
        return result;
    }
}

jobject Minecraft::GetTheMinecraft(JNIEnv* env)
{
    if (!env) return nullptr;

    const std::string mcClass = Mapper::Get("Minecraft", CLASS_MAPPING);
    if (mcClass.empty())
        return nullptr;

    jclass clazz = GameInstance::ResolveClass(env, mcClass);
    if (!clazz)
        return nullptr;

    // Named static field first, then the "static field of my own type" scan,
    // which needs no mapping at all.
    jobject instance = nullptr;

    const std::string fieldName = Mapper::Get("theMinecraft", FIELD_MAPPING);
    if (!fieldName.empty())
    {
        if (jfieldID field = jniutil::FieldByName(env, clazz, fieldName.c_str(), jniutil::FieldKind::Object))
        {
            instance = env->GetStaticObjectField(clazz, field);
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
                instance = nullptr;
            }
        }
    }

    if (!instance)
        instance = jniutil::StaticSelfInstance(env, clazz);

    env->DeleteLocalRef(clazz);
    return instance;
}

jobject Minecraft::GetThePlayer(JNIEnv* env)
{
    if (!env) return nullptr;

    jobject mc = GetTheMinecraft(env);
    if (!mc) return nullptr;

    jobject player = ObjectField(env, mc, "thePlayer");
    env->DeleteLocalRef(mc);
    return player;
}

jobject Minecraft::GetTheWorld(JNIEnv* env)
{
    if (!env) return nullptr;

    jobject mc = GetTheMinecraft(env);
    if (!mc) return nullptr;

    jobject world = ObjectField(env, mc, "theWorld");
    env->DeleteLocalRef(mc);
    return world;
}

jobject Minecraft::GetCurrentScreen(JNIEnv* env)
{
    if (!env) return nullptr;

    jobject mc = GetTheMinecraft(env);
    if (!mc) return nullptr;

    jobject screen = ObjectField(env, mc, "currentScreen");
    env->DeleteLocalRef(mc);
    return screen;
}

jobject Minecraft::GetPlayerController(JNIEnv* env)
{
    if (!env) return nullptr;

    jobject mc = GetTheMinecraft(env);
    if (!mc) return nullptr;

    jobject controller = ObjectField(env, mc, "playerController");
    env->DeleteLocalRef(mc);
    return controller;
}

jobject Minecraft::GetObjectMouseOver(JNIEnv* env)
{
    if (!env) return nullptr;

    jobject mc = GetTheMinecraft(env);
    if (!mc) return nullptr;

    jobject mouseOver = ObjectField(env, mc, "objectMouseOver");
    env->DeleteLocalRef(mc);
    return mouseOver;
}

void Minecraft::SetLeftClickCounter(int value, JNIEnv* env)
{
    if (!env) return;

    jobject mc = GetTheMinecraft(env);
    if (!mc) return;

    SetIntField(env, mc, "leftClickCounter", value);
    env->DeleteLocalRef(mc);
}

void Minecraft::SetRightClickCounter(int value, JNIEnv* env)
{
    if (!env) return;

    jobject mc = GetTheMinecraft(env);
    if (!mc) return;

    SetIntField(env, mc, "rightClickCounter", value);
    env->DeleteLocalRef(mc);
}

std::string Minecraft::GetPlayerName(JNIEnv* env)
{
    if (!env) return {};

    jobject player = GetThePlayer(env);
    if (!player) return {};

    std::string result;

    jclass clazz = env->GetObjectClass(player);
    if (clazz)
    {
        // GameProfile first, since the entity's own getName is the one that
        // returns "game.player.swim" and friends on this client.
        result = NameFromProfile(env, player, clazz);

        if (result.empty())
        {
            const std::string methodName = Mapper::Get("getName", METHOD_MAPPING);
            if (!methodName.empty())
            {
                jmethodID method = env->GetMethodID(clazz, methodName.c_str(), "()Ljava/lang/String;");
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (method)
                    result = ReadString(env, (jstring)env->CallObjectMethod(player, method));
            }
        }

        env->DeleteLocalRef(clazz);
    }

    env->DeleteLocalRef(player);
    return result;
}
