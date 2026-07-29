#include "hookimgui.h"
#include "../game/jni/GameInstance.h"
#include "../game/jni/JniUtil.h"
#include "../game/mapping/Mapper.h"

#include <jni.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <set>

namespace omega::dll::core {

void HideClan::SetEnabled(bool enabled) {
    m_enabled = enabled;
}

// ---------------------------------------------------------------------------
// Helper: read a jstring into a std::string and delete the local ref.
// ---------------------------------------------------------------------------
static std::string ReadJString(JNIEnv* env, jstring str) {
    if (!str) return {};
    const char* utf = env->GetStringUTFChars(str, nullptr);
    if (!utf) { env->DeleteLocalRef(str); return {}; }
    std::string result(utf);
    env->ReleaseStringUTFChars(str, utf);
    env->DeleteLocalRef(str);
    return result;
}

// ---------------------------------------------------------------------------
// Call a String-returning method on an object.
// ---------------------------------------------------------------------------
static std::string CallStringMethod(JNIEnv* env, jobject obj, jclass clazz, const char* methodName) {
    if (!env || !obj) return {};
    jclass clz = clazz ? clazz : env->GetObjectClass(obj);
    if (!clz) return {};

    jmethodID method = env->GetMethodID(clz, methodName, "()Ljava/lang/String;");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!clazz && clz) env->DeleteLocalRef(clz);
    if (!method) return {};

    jstring str = (jstring)env->CallObjectMethod(obj, method);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    return ReadJString(env, str);
}

// ---------------------------------------------------------------------------
// Get a player's formatted display name (with section sign colour codes).
// First tries getDisplayName, then getName as fallback.
// ---------------------------------------------------------------------------
static std::string GetFormattedName(JNIEnv* env, jobject player) {
    if (!env || !player) return {};

    std::string displayMethod = Mapper::Get("getDisplayName", METHOD_MAPPING);
    std::string name;

    if (!displayMethod.empty()) {
        jclass clazz = env->GetObjectClass(player);
        if (clazz) {
            jmethodID method = env->GetMethodID(clazz, displayMethod.c_str(), "()Ljava/lang/Object;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (method) {
                jobject chatComp = env->CallObjectMethod(player, method);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (chatComp) {
                    // IChatComponent.getUnformattedTextForChat() or getFormattedText()
                    jclass chatClass = env->GetObjectClass(chatComp);
                    if (chatClass) {
                        std::string unformattedMethod = Mapper::Get("getUnformattedTextForChat", METHOD_MAPPING);
                        if (!unformattedMethod.empty()) {
                            jmethodID getText = env->GetMethodID(chatClass, unformattedMethod.c_str(), "()Ljava/lang/String;");
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            if (getText) {
                                jstring text = (jstring)env->CallObjectMethod(chatComp, getText);
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                if (text) name = ReadJString(env, text);
                            }
                        }
                        // If unformatted failed, try getFormattedText
                        if (name.empty()) {
                            std::string formattedMethod = Mapper::Get("getFormattedText", METHOD_MAPPING);
                            if (!formattedMethod.empty()) {
                                jmethodID getFmt = env->GetMethodID(chatClass, formattedMethod.c_str(), "()Ljava/lang/String;");
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                if (getFmt) {
                                    jstring text = (jstring)env->CallObjectMethod(chatComp, getFmt);
                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                    if (text) name = ReadJString(env, text);
                                }
                            }
                        }
                        env->DeleteLocalRef(chatClass);
                    }
                    env->DeleteLocalRef(chatComp);
                }
            }
            env->DeleteLocalRef(clazz);
        }
    }

    // Fallback to getName
    if (name.empty())
        name = CallStringMethod(env, player, nullptr, Mapper::Get("getName", METHOD_MAPPING).c_str());

    return name;
}

// ---------------------------------------------------------------------------
// Normalize a clan tag: lowercase, remove whitespace.
// ---------------------------------------------------------------------------
static std::string NormalizeTag(std::string tag) {
    std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    tag.erase(std::remove_if(tag.begin(), tag.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), tag.end());
    return tag;
}

// ---------------------------------------------------------------------------
// Check if a formatted name has green (ally) color codes and no red codes.
// \247a = green, \2472 = dark green, \247c = red, \2474 = dark red
// ---------------------------------------------------------------------------
static bool IsAllyColor(const std::string& formattedName) {
    if (formattedName.empty()) return false;
    bool hasGreen = formattedName.find("\247a") != std::string::npos ||
                    formattedName.find("\2472") != std::string::npos;
    bool hasRed   = formattedName.find("\247c") != std::string::npos ||
                    formattedName.find("\2474") != std::string::npos;
    return hasGreen && !hasRed;
}

// ---------------------------------------------------------------------------
// Check if text contains "ALLY" as a whole word (case-insensitive, ignoring
// colour codes).
// ---------------------------------------------------------------------------
static bool HasAllyLabel(const std::string& text) {
    if (text.empty()) return false;

    std::string upper;
    upper.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        // Skip Minecraft colour codes (\247 + char)
        if (c == 0xC2 && i + 2 < text.size() &&
            static_cast<unsigned char>(text[i + 1]) == 0xA7) {
            i += 2;
            continue;
        }
        if (c == 0xA7) {
            if (i + 1 < text.size()) ++i;
            continue;
        }
        upper.push_back(static_cast<char>(std::toupper(c)));
    }

    size_t pos = 0;
    while ((pos = upper.find("ALLY", pos)) != std::string::npos) {
        bool leftOk  = pos == 0 || !std::isalnum(static_cast<unsigned char>(upper[pos - 1]));
        bool rightOk = pos + 4 >= upper.size() || !std::isalnum(static_cast<unsigned char>(upper[pos + 4]));
        if (leftOk && rightOk) return true;
        pos += 4;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Parses a trailing [Tag] from a formatted (raw) player name including colour
// codes. Returns the normalized tag and the formatted (coloured) tag.
// ---------------------------------------------------------------------------
struct TrailingTag {
    std::string normalized;
    std::string formatted;
};

static bool ConsumeFormatCode(const std::string& text, size_t idx, size_t& consumed, char& code) {
    consumed = 0;
    code = 0;
    if (idx >= text.size()) return false;

    unsigned char c = static_cast<unsigned char>(text[idx]);
    // UTF-8 encoded section sign: \xC2\xA7 + char
    if (c == 0xC2 && idx + 2 < text.size() &&
        static_cast<unsigned char>(text[idx + 1]) == 0xA7) {
        consumed = 3;
        code = static_cast<char>(std::tolower(static_cast<unsigned char>(text[idx + 2])));
        return true;
    }
    // Raw section sign
    if (c == 0xA7 && idx + 1 < text.size()) {
        consumed = 2;
        code = static_cast<char>(std::tolower(static_cast<unsigned char>(text[idx + 1])));
        return true;
    }
    return false;
}

static TrailingTag ParseTrailingTag(const std::string& formattedText) {
    TrailingTag result;
    if (formattedText.empty()) return result;

    // Strip colour codes into plain text, tracking raw offsets
    std::string plain;
    std::vector<size_t> rawOffsets;
    plain.reserve(formattedText.size());
    rawOffsets.reserve(formattedText.size());

    for (size_t i = 0; i < formattedText.size();) {
        size_t consumed = 0;
        char code = 0;
        if (ConsumeFormatCode(formattedText, i, consumed, code)) {
            i += consumed;
            continue;
        }
        rawOffsets.push_back(i);
        plain.push_back(formattedText[i]);
        ++i;
    }

    // Trim trailing whitespace
    while (!plain.empty() && std::isspace(static_cast<unsigned char>(plain.back()))) {
        plain.pop_back();
        rawOffsets.pop_back();
    }
    if (plain.empty() || plain.back() != ']') return result;

    size_t open = plain.rfind('[');
    if (open == std::string::npos || open + 2 >= plain.size()) return result;

    std::string tag = plain.substr(open + 1, plain.size() - open - 2);
    if (tag.empty() || tag.size() > 16 ||
        !std::all_of(tag.begin(), tag.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-';
        })) {
        return result;
    }

    result.normalized = NormalizeTag(std::move(tag));

    size_t rawStart = rawOffsets[open];
    size_t rawEnd   = rawOffsets.back() + 1;

    // Find the active colour before the bracket
    std::string activeColor;
    for (size_t i = 0; i < rawStart;) {
        size_t consumed = 0;
        char code = 0;
        if (!ConsumeFormatCode(formattedText, i, consumed, code)) {
            ++i;
            continue;
        }
        bool isColor = (code >= '0' && code <= '9') || (code >= 'a' && code <= 'f');
        if (isColor)
            activeColor = formattedText.substr(i, consumed);
        else if (code == 'r')
            activeColor.clear();
        i += consumed;
    }

    result.formatted = activeColor;
    result.formatted.append(formattedText, rawStart, rawEnd - rawStart);
    return result;
}

static std::string ExtractTrailingTag(const std::string& formattedText) {
    return ParseTrailingTag(formattedText).normalized;
}

// ---------------------------------------------------------------------------
// Determine if two players are on the same team using multi-layered fallback
// logic (same approach as crimson HideClans).
// ---------------------------------------------------------------------------
static bool IsSameTeam(
    JNIEnv* env,
    jobject localPlayer,
    jobject otherPlayer,
    const std::string& localFormatted,
    const std::string& otherFormatted,
    const std::string& localClanTag,
    const std::string& otherClanTag)
{
    (void)localPlayer;
    (void)otherPlayer;
    (void)env;

    // 1. Trailing [clan] tag match (works on all clients including CB)
    std::string localTrailing = ExtractTrailingTag(localFormatted);
    std::string otherTrailing = ExtractTrailingTag(otherFormatted);

    if (!localTrailing.empty() && !otherTrailing.empty()) {
        if (localTrailing == otherTrailing)
            return true;
    }

    // 2. Clan tag from scoreboard/team (non-CB clients)
    if (!localClanTag.empty() && !otherClanTag.empty()) {
        if (localClanTag == otherClanTag)
            return true;
    }

    // 3. Ally color fallback (green name = ally, red = enemy)
    if (IsAllyColor(localFormatted) && IsAllyColor(otherFormatted))
        return true;

    // 4. "ALLY" label fallback
    if (HasAllyLabel(otherFormatted))
        return true;

    return false;
}

// ---------------------------------------------------------------------------
// JNI helpers for getting player data
// ---------------------------------------------------------------------------
static std::string GetPlayerName(JNIEnv* env, jobject player) {
    return CallStringMethod(env, player, nullptr, Mapper::Get("getName", METHOD_MAPPING).c_str());
}

static jstring GetClanTag(JNIEnv* env, jobject player) {
    if (!env || !player) return nullptr;

    // Try getClanTag() method (some servers expose this)
    std::string methodName = Mapper::Get("getClanTag", METHOD_MAPPING);
    if (!methodName.empty()) {
        jclass clazz = env->GetObjectClass(player);
        if (clazz) {
            jmethodID method = env->GetMethodID(clazz, methodName.c_str(), "()Ljava/lang/String;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (method) {
                jstring tag = (jstring)env->CallObjectMethod(player, method);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(clazz);
                return tag;
            }
            env->DeleteLocalRef(clazz);
        }
    }

    // Try team/clan via scoreboard
    // For now return empty - clan tags from formatted name is the primary method
    return nullptr;
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
    std::string widthField  = Mapper::Get("width", FIELD_MAPPING);
    std::string heightField = Mapper::Get("height", FIELD_MAPPING);
    jclass clazz = env->GetObjectClass(player);
    if (!clazz) return;

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

// ---------------------------------------------------------------------------
// Main tick: iterate world players, detect allies, hide them.
// ---------------------------------------------------------------------------
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

    // thePlayer
    std::string playerField = Mapper::Get("thePlayer", FIELD_MAPPING);
    if (playerField.empty()) { env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }
    jfieldID playerFid = jniutil::FieldByName(env, mcClass, playerField.c_str());
    if (!playerFid) { env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return; }

    jobject localPlayer = env->GetObjectField(mc, playerFid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); localPlayer = nullptr; }

    // theWorld
    std::string worldField = Mapper::Get("theWorld", FIELD_MAPPING);
    if (worldField.empty()) {
        if (localPlayer) env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }
    jfieldID worldFid = jniutil::FieldByName(env, mcClass, worldField.c_str());
    if (!worldFid) {
        if (localPlayer) env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jobject world = env->GetObjectField(mc, worldFid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); world = nullptr; }
    if (!world || !localPlayer) {
        if (localPlayer) env->DeleteLocalRef(localPlayer);
        if (world) env->DeleteLocalRef(world);
        env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jclass worldClass = env->GetObjectClass(world);
    if (!worldClass) {
        env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(world);
        env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    // playerEntities
    std::string playersField = Mapper::Get("playerEntities", FIELD_MAPPING);
    if (playersField.empty()) {
        env->DeleteLocalRef(worldClass); env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(world); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jfieldID playersFid = jniutil::FieldByName(env, worldClass, playersField.c_str());
    if (!playersFid) {
        env->DeleteLocalRef(worldClass); env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(world); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jobject playerList = env->GetObjectField(world, playersFid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); playerList = nullptr; }
    if (!playerList) {
        env->DeleteLocalRef(worldClass); env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(world); env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jclass listClass = env->GetObjectClass(playerList);
    if (!listClass) {
        env->DeleteLocalRef(playerList); env->DeleteLocalRef(worldClass);
        env->DeleteLocalRef(localPlayer); env->DeleteLocalRef(world);
        env->DeleteLocalRef(mc); env->DeleteLocalRef(mcClass); return;
    }

    jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
    jmethodID getMethod  = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    jint size = 0;
    if (sizeMethod && getMethod) {
        size = env->CallIntMethod(playerList, sizeMethod);
        if (env->ExceptionCheck()) { env->ExceptionClear(); size = 0; }
    }

    // Get local player info
    std::string localName     = GetPlayerName(env, localPlayer);
    std::string localFormatted = GetFormattedName(env, localPlayer);
    jstring localClanJStr    = GetClanTag(env, localPlayer);
    std::string localClan;
    if (localClanJStr) localClan = ReadJString(env, localClanJStr);
    else if (env) localClan = NormalizeTag(localFormatted); // fallback: use formatted

    // Class filter: EntityPlayer
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

        std::string otherName      = GetPlayerName(env, entity);
        std::string otherFormatted = GetFormattedName(env, entity);
        jstring otherClanJStr      = GetClanTag(env, entity);
        std::string otherClan;
        if (otherClanJStr) otherClan = ReadJString(env, otherClanJStr);

        bool sameTeam = IsSameTeam(env, localPlayer, entity,
                                    localFormatted, otherFormatted,
                                    localClan, otherClan);

        if (sameTeam)
            allies.push_back(entity);
        else
            enemies.push_back(entity);
    }

    if (playerClass) env->DeleteLocalRef(playerClass);

    // Restore enemies
    for (auto* e : enemies) {
        RestorePlayer(env, e);
        env->DeleteLocalRef(e);
    }

    // Hide or show allies
    if (!m_showAllies) {
        for (auto* ally : allies) {
            HidePlayer(env, ally);
            env->DeleteLocalRef(ally);
        }
    } else {
        int showCount = (std::max)(1, (std::min)(14, m_showCount));
        int shown = 0;
        for (auto* ally : allies) {
            if (shown < showCount)
                RestorePlayer(env, ally);
            else
                HidePlayer(env, ally);
            shown++;
            env->DeleteLocalRef(ally);
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
        jclass wClass = env->GetObjectClass(world);
        if (wClass) {
            std::string playersField = Mapper::Get("playerEntities", FIELD_MAPPING);
            if (!playersField.empty()) {
                jfieldID pFid = jniutil::FieldByName(env, wClass, playersField.c_str());
                if (pFid) {
                    jobject pList = env->GetObjectField(world, pFid);
                    if (pList) {
                        jclass lClass = env->GetObjectClass(pList);
                        jmethodID sMethod = env->GetMethodID(lClass, "size", "()I");
                        jmethodID gMethod = env->GetMethodID(lClass, "get", "(I)Ljava/lang/Object;");
                        if (sMethod && gMethod) {
                            jint sz = env->CallIntMethod(pList, sMethod);
                            for (jint i = 0; i < sz; i++) {
                                jobject entity = env->CallObjectMethod(pList, gMethod, i);
                                if (entity) {
                                    RestorePlayer(env, entity);
                                    env->DeleteLocalRef(entity);
                                }
                            }
                        }
                        env->DeleteLocalRef(lClass);
                        env->DeleteLocalRef(pList);
                    }
                }
            }
            env->DeleteLocalRef(wClass);
        }
        env->DeleteLocalRef(world);
    }

    env->DeleteLocalRef(mc);
    env->DeleteLocalRef(mcClass);

    m_enabled = false;
}

} // namespace omega::dll::core
