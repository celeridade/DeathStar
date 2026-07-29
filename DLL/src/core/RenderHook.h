#pragma once

namespace omega::dll::core
{
    // Hooks wglSwapBuffers so the watermark is drawn into Minecraft's own
    // framebuffer, on the frame the game is already presenting.
    bool InstallRenderHook();
    void RemoveRenderHook();

    // Frames per second measured at the swap, which is the number the watermark
    // shows and the only place it can be measured honestly.
    int GetFramesPerSecond();
}
