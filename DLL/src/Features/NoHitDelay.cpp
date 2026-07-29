#include "NoHitDelay.h"
#include "../game/jni/GameInstance.h"
#include "../game/classes/Minecraft.h"

namespace omega::dll::features {

    void NoHitDelay::SetEnabled(bool enabled)
    {
        m_enabled.store(enabled);
    }

    bool NoHitDelay::IsEnabled() const
    {
        return m_enabled.load();
    }

    void NoHitDelay::Tick(JNIEnv* env)
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
            Minecraft::SetLeftClickCounter(0, env);
            env->DeleteLocalRef(player);
        }

        env->DeleteLocalRef(world);
    }

}
