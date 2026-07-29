#include "GameInstance.h"
#include <windows.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>

GameInstance* g_Game = nullptr;

namespace {

    // Walks every live thread's context ClassLoader and asks it to load the class.
    // The launcher's loader is the one that has Minecraft, and it is always the
    // context loader of at least the client thread.
    jclass LoadViaThreadClassLoaders(JNIEnv* env, const std::string& dottedName)
    {
        jclass threadCls = env->FindClass("java/lang/Thread");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        if (!threadCls) return nullptr;

        jclass loaderCls = env->FindClass("java/lang/ClassLoader");
        if (env->ExceptionCheck()) { env->ExceptionClear(); }

        jmethodID getAllStackTraces = env->GetStaticMethodID(threadCls, "getAllStackTraces", "()Ljava/util/Map;");
        jmethodID getContextLoader = env->GetMethodID(threadCls, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
        jmethodID loadClass = loaderCls ? env->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;") : nullptr;
        jmethodID getSystemLoader = loaderCls ? env->GetStaticMethodID(loaderCls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;") : nullptr;

        if (env->ExceptionCheck()) { env->ExceptionClear(); }

        if (!loadClass)
        {
            env->DeleteLocalRef(threadCls);
            if (loaderCls) env->DeleteLocalRef(loaderCls);
            return nullptr;
        }

        jstring name = env->NewStringUTF(dottedName.c_str());
        if (!name)
        {
            env->DeleteLocalRef(threadCls);
            if (loaderCls) env->DeleteLocalRef(loaderCls);
            return nullptr;
        }

        auto tryLoader = [&](jobject loader) -> jclass {
            if (!loader) return nullptr;
            jclass found = (jclass)env->CallObjectMethod(loader, loadClass, name);
            if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
            return found;
        };

        jclass result = nullptr;

        if (getSystemLoader)
        {
            jobject system = env->CallStaticObjectMethod(loaderCls, getSystemLoader);
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (system)
            {
                result = tryLoader(system);
                env->DeleteLocalRef(system);
            }
        }

        if (!result && getAllStackTraces)
        {
            jobject map = env->CallStaticObjectMethod(threadCls, getAllStackTraces);
            if (env->ExceptionCheck()) { env->ExceptionClear(); map = nullptr; }

            if (map)
            {
                jclass mapCls = env->GetObjectClass(map);
                jmethodID keySet = env->GetMethodID(mapCls, "keySet", "()Ljava/util/Set;");
                jobject keys = keySet ? env->CallObjectMethod(map, keySet) : nullptr;
                if (env->ExceptionCheck()) { env->ExceptionClear(); keys = nullptr; }

                if (keys)
                {
                    jclass setCls = env->GetObjectClass(keys);
                    jmethodID iterator = env->GetMethodID(setCls, "iterator", "()Ljava/util/Iterator;");
                    jobject it = iterator ? env->CallObjectMethod(keys, iterator) : nullptr;
                    if (env->ExceptionCheck()) { env->ExceptionClear(); it = nullptr; }

                    if (it)
                    {
                        jclass itCls = env->GetObjectClass(it);
                        jmethodID hasNext = env->GetMethodID(itCls, "hasNext", "()Z");
                        jmethodID next = env->GetMethodID(itCls, "next", "()Ljava/lang/Object;");

                        while (!result && hasNext && next && env->CallBooleanMethod(it, hasNext))
                        {
                            if (env->ExceptionCheck()) { env->ExceptionClear(); break; }

                            jobject thread = env->CallObjectMethod(it, next);
                            if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
                            if (!thread) continue;

                            jobject loader = env->CallObjectMethod(thread, getContextLoader);
                            if (env->ExceptionCheck()) env->ExceptionClear();

                            if (loader)
                            {
                                result = tryLoader(loader);
                                env->DeleteLocalRef(loader);
                            }

                            env->DeleteLocalRef(thread);
                        }

                        env->DeleteLocalRef(itCls);
                        env->DeleteLocalRef(it);
                    }

                    env->DeleteLocalRef(setCls);
                    env->DeleteLocalRef(keys);
                }

                env->DeleteLocalRef(mapCls);
                env->DeleteLocalRef(map);
            }
        }

        env->DeleteLocalRef(name);
        env->DeleteLocalRef(threadCls);
        if (loaderCls) env->DeleteLocalRef(loaderCls);

        return result;
    }
}

jclass GameInstance::ResolveClass(JNIEnv* env, const std::string& className)
{
    if (!env || className.empty())
        return nullptr;

    // The classloader walk allocates a map of every live thread, so doing it per
    // frame would be brutal. Each resolved class is kept as a global ref and the
    // caller gets a fresh local ref off it, cheap after the first time.
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, jclass> cache;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(className);
        if (it != cache.end())
            return (jclass)env->NewLocalRef(it->second);
    }

    std::string slashName = className;
    std::replace(slashName.begin(), slashName.end(), '.', '/');

    jclass local = env->FindClass(slashName.c_str());
    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        local = nullptr;
    }

    if (!local)
    {
        std::string dottedName = className;
        std::replace(dottedName.begin(), dottedName.end(), '/', '.');
        local = LoadViaThreadClassLoaders(env, dottedName);
    }

    if (!local)
        return nullptr;

    jclass global = (jclass)env->NewGlobalRef(local);

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        // Another thread may have cached it first; keep the winner.
        auto [it, inserted] = cache.emplace(className, global);
        if (!inserted)
            env->DeleteGlobalRef(global);
    }

    return local;
}

GameInstance::GameInstance()
{
    m_Env = nullptr;
    m_Jvm = nullptr;

    jsize count;
    if (JNI_GetCreatedJavaVMs(&m_Jvm, 1, &count) == JNI_OK && count > 0)
    {
        if (m_Jvm->GetEnv(reinterpret_cast<void**>(&m_Env), JNI_VERSION_1_8) == JNI_OK)
        {
            if (m_Env)
            {
                m_GameVersion = DetectGameVersion();
            }
        }
    }
}

JNIEnv* GameInstance::GetCurrentEnv() const
{
    JNIEnv* env = nullptr;
    if (m_Jvm)
    {
        jint result = m_Jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
        if (result == JNI_OK)
            return env;
    }
    return nullptr;
}

bool GameInstance::Attach()
{
    if (!m_Jvm)
        return false;

    JNIEnv* env = nullptr;
    jint result = m_Jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (result == JNI_EDETACHED)
    {
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_8;
        args.name = nullptr;   // no custom thread name = no fingerprint left behind
        args.group = nullptr;

        if (m_Jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), &args) == JNI_OK)
        {
            m_Env = env;
            return true;
        }
        return false;
    }
    else if (result == JNI_OK)
    {
        m_Env = env;
        return true;
    }

    return false;
}

void GameInstance::Detach()
{
    if (m_Jvm)
    {
        m_Jvm->DetachCurrentThread();
        m_Env = nullptr;
    }
}

JNIEnv* GameInstance::AttachCurrentThreadEnv(bool* outAttached)
{
    if (outAttached)
        *outAttached = false;

    if (!m_Jvm)
        return nullptr;

    JNIEnv* env = nullptr;
    jint result = m_Jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);

    if (result == JNI_OK)
        return env;

    if (result != JNI_EDETACHED)
        return nullptr;

    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_8;
    args.name = nullptr;   // no custom thread name = no fingerprint left behind
    args.group = nullptr;

    // As a daemon so a thread of ours can never hold the JVM open on shutdown.
    if (m_Jvm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), &args) != JNI_OK)
        return nullptr;

    if (outAttached)
        *outAttached = true;

    return env;
}

void GameInstance::DetachCurrentThreadEnv()
{
    if (m_Jvm)
        m_Jvm->DetachCurrentThread();
}

bool GameInstance::InitializeGame()
{
    if (m_Initialized)
        return true;

    if (!m_Env)
    {
        if (!Attach())
            return false;
    }

    if (m_GameVersion == UNKNOWN)
    {
        m_GameVersion = DetectGameVersion();
        if (m_GameVersion == UNKNOWN)
            return false;
    }

    Mapper::Initialize(m_GameVersion);

    m_Initialized = true;
    return true;
}

Class* GameInstance::FindClass(const std::string& className)
{
    if (!m_Env)
        return nullptr;

    auto it = m_CachedClasses.find(className);
    if (it != m_CachedClasses.end())
        return it->second;

    std::string runtimeName = Mapper::Get(className.c_str(), CLASS_MAPPING);
    std::string jniName = runtimeName;
    for (auto& c : jniName)
        if (c == '/') c = '.';

    jclass clazz = m_Env->FindClass(jniName.c_str());
    if (!clazz)
    {
        if (m_Env->ExceptionCheck())
            m_Env->ExceptionClear();
        return nullptr;
    }

    Class* cls = new Class();
    m_CachedClasses[className] = cls;
    return cls;
}

GameVersions GameInstance::DetectGameVersion()
{
    if (!m_Env)
        return UNKNOWN;

    jclass testClass = nullptr;

    // CheatBreaker 1.8.9 ships its own Allatori pass, so its Minecraft class has
    // a name no other client uses. Checked first because the generic lookups
    // below would otherwise resolve through the same loader and misreport.
    testClass = ResolveClass(m_Env, "IllIIIlIlllllIllllIIIIIIl");
    if (testClass)
    {
        m_Env->DeleteLocalRef(testClass);
        return CHEATBREAKER;
    }

    testClass = ResolveClass(m_Env, "ave");
    if (testClass)
    {
        m_Env->DeleteLocalRef(testClass);
        return BADLION;
    }

    testClass = ResolveClass(m_Env, "net.minecraft.client.Minecraft");
    if (testClass)
    {
        m_Env->DeleteLocalRef(testClass);
        jclass lunarClass = ResolveClass(m_Env, "com.lunarclient.bukkitapi.nethandler.LCPacket");
        if (lunarClass)
        {
            m_Env->DeleteLocalRef(lunarClass);
            return LUNAR;
        }

        jclass featherClass = ResolveClass(m_Env, "club.feather.Feather");
        if (featherClass)
        {
            m_Env->DeleteLocalRef(featherClass);
            return FEATHER_1_8;
        }

        jclass forgeClass = ResolveClass(m_Env, "net.minecraftforge.common.MinecraftForge");
        if (forgeClass)
        {
            m_Env->DeleteLocalRef(forgeClass);
            return FORGE_1_8;
        }

        return LUNAR;
    }

    testClass = ResolveClass(m_Env, "net/minecraft/client/Minecraft");
    if (testClass)
    {
        m_Env->DeleteLocalRef(testClass);

        jclass forgeClass = ResolveClass(m_Env, "net/minecraftforge/common/MinecraftForge");
        if (forgeClass)
        {
            m_Env->DeleteLocalRef(forgeClass);
            return FORGE_1_8;
        }

        return UNKNOWN;
    }

    if (m_Env->ExceptionCheck())
        m_Env->ExceptionClear();

    return UNKNOWN;
}
