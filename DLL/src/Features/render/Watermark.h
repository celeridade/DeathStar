#pragma once
#include <jni.h>
#include <string>

struct ImFont;

namespace omega::shared { struct SharedConfig; }

namespace omega::dll::features::render
{
    // The bar the loader used to draw in its own window. It belongs on top of
    // the game, so it is drawn from the render hook instead.
    class Watermark
    {
    public:
        static void SetConfig(shared::SharedConfig* config);
        static void SetFonts(ImFont* bold, ImFont* regular);

        // Called on the render thread, inside the swap hook.
        static void Draw(float screenWidth, float screenHeight, int fps);

        // Called on the game thread, where reading the player is legal.
        static void RefreshFromGame(JNIEnv* env);

        // Clicks per second, fed by the autoclicker and by real presses.
        static void SetClickCount(unsigned int count);

        // Zeroes the cached player name on unload so it is not left in memory.
        static void Shutdown();
    };
}
