#include "hookimgui.h"
#include "../game/jni/GameInstance.h"
#include "../game/jni/JniUtil.h"
#include "../game/mapping/Mapper.h"

#include <jni.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace omega::dll::core {

void HideClan::SetEnabled(bool enabled) {
    m_enabled = enabled;
}

static std::string ReadJString(JNIEnv* env, jstring str) {
    if (!str) return {};
    const char* utf = env->GetStringUTFChars(str, nullptr);
    if (!utf) return {};
    std::string result(utf);
    env->ReleaseStringUTFChars(str, utf);
    env->DeleteLocalRef(str);
    return result;
}

static std::string GetPlayerName(JNIEnv* env, jobject player) {
    if (!env || !player) return {};
    std::string methodName = Mapper::Get("getName", METHOD_MAPPING);
    if (methodName.empty()) return {};
    jclass clazz = env->GetObjectClass(player);
    if (!clazz) return {};
    jmethodID method = env->GetMethodID(clazz, methodName.c_str(), "()Ljava/lang/String;");
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(clazz);
    if (!method) return {};
    jstring name = (jstring)env->CallObjectMethod(player, method);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    return ReadJString(env, name);
}

static bool IsSameTeam(JNIEnv* env, jobject localPlayer, jobject other) {
    if (!env || !localPlayer || !other) return false;
    jclass playerClass = env->GetObjectClass(localPlayer);
    if (!playerClass) return false;
    std::string methodName = Mapper::Get("isOnSameTeam", METHOD_MAPPING);
    jmethodID method = env->GetMethodID(playerClass, methodName.c_str(), "(Lnet/minecraft/entity/Entity;)Z");
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(playerClass);
    if (!method) return false;
    jboolean result = env->CallBooleanMethod(localPlayer, method, other);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    return result != 0;
}

static void SetPlayerInvisible(JNIEnv* env, jobject player, bool invisible) {
    if (!env || !player) return;
    std::string fieldName = Mapper::Get("isInvisible", FIELD_MAPPING);
    if (fieldName.empty()) return;
    jclass clazz = env->GetObjectClass(player);
    if (!clazz) return;
    jfieldID field = jniutil::FieldByName(env, clazz, fieldName.c_str());
    env->DeleteLocalRef(clazz);
    if (!field) return;
    env->SetBooleanField(player, field, invisible ? JNI_TRUE : JNI_FALSE);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

static void ClearBoundingBox(JNIEnv* env, jobject player) {
    if (!env || !player) return;
    jclass clazz = env->GetObjectClass(player);
    if (!clazz) return;
    std::string widthField = Mapper::Get("width", FIELD_MAPPING);
    std::string heightField = Mapper::Get("height", FIELD_MAPPING);
    if (!widthField.empty()) {
        jfieldID wf = jniutil::FieldByName(env, clazz, widthField.c_str());
        if (wf) env->SetFloatField(player, wf, 0.f);
    }
    if (!heightField.empty()) {
        jfieldID hf = jniutil::FieldByName(env, clazz, heightField.c_str());
        if (hf) env->SetFloatField(player, hf, 0.f);
    }
    env->DeleteLocalRef(clazz);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void HideClan::HidePlayer(JNIEnv* env, jobject player) {
    SetPlayerInvisible(env, player, true);
    ClearBoundingBox(env, player);
}

void HideClan::RestorePlayer(JNIEnv* env, jobject player) {
    SetPlayerInvisible(env, player, false);
}

void HideClan::Tick(JNIEnv* env) {
    if (!env || !m_enabled) return;

    std::string mcClassName = Mapper::Get("Minecraft", CLASS_MAPPING);
    if (mcClassName.empty()) return;

    jclass mcClass = GameInstance::ResolveClass(env, mcClassName);
    if (!mcClass) return;

    std::string getMcMethod = Mapper::Get("getMinecraft", METHOD_MAPPING);
    std::string mcSig = "()L" + mcClassName + ";";
    jmethodID getMc = env->GetStaticMethodID(mcClass, getMcMethod.c_str(), mcSig.c_str());
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!getMc) { env->DeleteLocalRef(mcClass); return; }

    jobject mc = env->CallStaticObjectMethod(mcClass, getMc);
    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(mcClass); return; }
    if (!mc) { env->DeleteLocalRef(mcClass); return; }

    std::string playerField = Mapper::Get("thePlayer", FIELD_MAPPING);
    if (playerField.empty()) { env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }

    jfieldID playerFid = jniutil::FieldByName(env, mcClass, playerField.c_str());
    if (!playerFid) { env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }

    jobject localPlayer = env->GetObjectField(mc, playerFid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }
    if (!localPlayer) { env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }

    std::string worldField = Mapper::Get("theWorld", FIELD_MAPPING);
    if (worldField.empty()) { env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }

    jfieldID worldFid = jniutil::FieldByName(env, mcClass, worldField.c_str());
    if (!worldFid) { env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }

    jobject world = env->GetObjectField(mc, worldFid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); world = nullptr; }
    if (!world) { env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }

    jclass worldClass = env->GetObjectClass(world);
    if (!worldClass) {
        env->DeleteLocalRef(world); env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    std::string playersField = Mapper::Get("playerEntities", FIELD_MAPPING);
    if (playersField.empty()) {
        env->DeleteLocalRef(worldClass); env->DeleteLocalRef(world);
        env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jfieldID playersFid = jniutil::FieldByName(env, worldClass, playersField.c_str());
    if (!playersFid) {
        env->DeleteLocalRef(worldClass); env->DeleteLocalRef(world);
        env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jobject playerList = env->GetObjectField(world, playersFid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); playerList = nullptr; }
    if (!playerList) {
        env->DeleteLocalRef(worldClass); env->DeleteLocalRef(world);
        env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jclass listClass = env->GetObjectClass(playerList);
    if (!listClass) {
        env->DeleteLocalRef(playerList); env->DeleteLocalRef(worldClass);
        env->DeleteLocalRef(world); env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
    jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    jint size = 0;
    if (sizeMethod && getMethod) {
        size = env->CallIntMethod(playerList, sizeMethod);
        if (env->ExceptionCheck()) { env->ExceptionClear(); size = 0; }
    }

    std::string playerClassName = Mapper::Get("EntityPlayer", CLASS_MAPPING);
    jclass playerClass = !playerClassName.empty() ? GameInstance::ResolveClass(env, playerClassName) : nullptr;

    std::vector<jobject> allies;
    std::vector<jobject> enemies;

    for (jint i = 0; i < size; i++) {
        jobject entity = env->CallObjectMethod(playerList, getMethod, i);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (!entity) continue;
        if (env->IsSameObject(entity, localPlayer)) {
            env->DeleteLocalRef(entity);
            continue;
        }
        if (playerClass) {
            jboolean isPlayer = env->IsInstanceOf(entity, playerClass);
            if (!isPlayer) {
                env->DeleteLocalRef(entity);
                continue;
            }
        }
        bool sameTeam = IsSameTeam(env, localPlayer, entity);
        if (sameTeam)
            allies.push_back(entity);
        else
            enemies.push_back(entity);
    }

    if (playerClass) env->DeleteLocalRef(playerClass);

    for (size_t ei = 0; ei < enemies.size(); ei++) {
        RestorePlayer(env, enemies[ei]);
        env->DeleteLocalRef(enemies[ei]);
    }

    if (!m_showAllies) {
        for (size_t ai = 0; ai < allies.size(); ai++) {
            HidePlayer(env, allies[ai]);
            env->DeleteLocalRef(allies[ai]);
        }
    } else {
        int showCount = (std::max)(1, (std::min)(14, m_showCount));
        int shown = 0;
        for (size_t ai = 0; ai < allies.size(); ai++) {
            if (shown < showCount)
                RestorePlayer(env, allies[ai]);
            else
                HidePlayer(env, allies[ai]);
            shown++;
            env->DeleteLocalRef(allies[ai]);
        }
    }

    env->DeleteLocalRef(listClass);
    env->DeleteLocalRef(playerList);
    env->DeleteLocalRef(worldClass);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(localPlayer);
    env->DeleteLocalRef(mc);
    env->DeleteLocalRef(mcClass);
}

void HideClan::Shutdown(JNIEnv* env) {
    if (!env) return;

    std::string mcClassName = Mapper::Get("Minecraft", CLASS_MAPPING);
    if (mcClassName.empty()) return;

    jclass mcClass = GameInstance::ResolveClass(env, mcClassName);
    if (!mcClass) return;

    std::string getMcMethod = Mapper::Get("getMinecraft", METHOD_MAPPING);
    std::string mcSig = "()L" + mcClassName + ";";
    jmethodID getMc = env->GetStaticMethodID(mcClass, getMcMethod.c_str(), mcSig.c_str());
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!getMc) { env->DeleteLocalRef(mcClass); return; }

    jobject mc = env->CallStaticObjectMethod(mcClass, getMc);
    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(mcClass); return; }
    if (!mc) { env->DeleteLocalRef(mcClass); return; }

    std::string worldField = Mapper::Get("theWorld", FIELD_MAPPING);
    jfieldID worldFid = nullptr;
    if (!worldField.empty())
        worldFid = jniutil::FieldByName(env, mcClass, worldField.c_str());

    jobject world = nullptr;
    if (worldFid) {
        world = env->GetObjectField(mc, worldFid);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    if (world) {
        jclass worldClass = env->GetObjectClass(world);
        if (worldClass) {
            std::string playersField = Mapper::Get("playerEntities", FIELD_MAPPING);
            if (!playersField.empty()) {
                jfieldID playersFid = jniutil::FieldByName(env, worldClass, playersField.c_str());
                if (playersFid) {
                    jobject playerList = env->GetObjectField(world, playersFid);
                    if (playerList) {
                        jclass listClass = env->GetObjectClass(playerList);
                        jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
                        jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
                        if (sizeMethod && getMethod) {
                            jint sz = env->CallIntMethod(playerList, sizeMethod);
                            for (jint i = 0; i < sz; i++) {
                                jobject entity = env->CallObjectMethod(playerList, getMethod, i);
                                if (entity) {
                                    RestorePlayer(env, entity);
                                    env->DeleteLocalRef(entity);
                                }
                            }
                        }
                        env->DeleteLocalRef(listClass);
                        env->DeleteLocalRef(playerList);
                    }
                }
            }
            env->DeleteLocalRef(worldClass);
        }
        env->DeleteLocalRef(world);
    }

    env->DeleteLocalRef(mc);
    env->DeleteLocalRef(mcClass);

    m_enabled = false;
}

} // namespace omega::dll::core
