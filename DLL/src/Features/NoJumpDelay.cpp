#include "NoJumpDelay.h"
#include "../game/jni/GameInstance.h"
#include "../game/classes/Minecraft.h"
#include "../game/classes/Player.h"

namespace omega::dll::features {

    void NoJumpDelay::SetEnabled(bool enabled)
    {
        m_enabled.store(enabled);
    }

    bool NoJumpDelay::IsEnabled() const
    {
        return m_enabled.load();
    }

    void NoJumpDelay::Tick(JNIEnv* env)
    {
        if (!m_enabled.load())
            return;

        jobject world = Minecraft::GetTheWorld(env);
        if (!world)
            return;

        jobject screen = Minecraft::GetCurrentScreen(env);
        if (screen)
        {
            env->DeleteLocalRef(screen);
            env->DeleteLocalRef(world);
            return;
        }

        jobject player = Minecraft::GetThePlayer(env);
        if (player)
        {
            Player::SetJumpTicks(player, 0, env);
            env->DeleteLocalRef(player);
        }

        env->DeleteLocalRef(world);
    }

}
