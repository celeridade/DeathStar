#include "Player.h"
#include "../jni/JniUtil.h"
#include "../mapping/Mapper.h"
#include <string>

void Player::SetJumpTicks(jobject player, int ticks, JNIEnv* env)
{
    if (!player || !env)
        return;

    const std::string fieldName = Mapper::Get("jumpTicks", FIELD_MAPPING);
    if (fieldName.empty())
        return;

    // The field lives on EntityLivingBase, but going through the player's own
    // class and walking up the hierarchy avoids having to name that class.
    jclass clazz = env->GetObjectClass(player);
    if (!clazz)
    {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    jfieldID field = jniutil::FieldByName(env, clazz, fieldName.c_str(), jniutil::FieldKind::Int);
    env->DeleteLocalRef(clazz);

    if (!field)
        return;

    env->SetIntField(player, field, ticks);
    if (env->ExceptionCheck())
        env->ExceptionClear();
}
