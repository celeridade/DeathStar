#include "RenderHook.h"
#include "../features/render/Watermark.h"

#include <windows.h>
#include <gl/GL.h>
#include <atomic>
#include <chrono>

#include "MinHook.h"

#include <imgui.h>
#include "imgui_impl_opengl2.h"
#include "font.h"

#pragma comment(lib, "opengl32.lib")

namespace omega::dll::core
{
    namespace
    {
        using SwapBuffersFn = BOOL(WINAPI*)(HDC);

        SwapBuffersFn g_original = nullptr;
        void* g_target = nullptr;

        std::atomic<bool> g_installed{ false };
        std::atomic<bool> g_shuttingDown{ false };
        std::atomic<int> g_activeCalls{ 0 };
        std::atomic<int> g_fps{ 0 };

        bool g_imguiReady = false;
        ImGuiContext* g_context = nullptr;
        std::chrono::steady_clock::time_point g_lastFrame;
        std::chrono::steady_clock::time_point g_lastFpsSample;
        int g_frameCount = 0;

        void SetupImGui()
        {
            IMGUI_CHECKVERSION();
            g_context = ImGui::CreateContext();
            ImGui::SetCurrentContext(g_context);

            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            // The watermark never takes input. Not wiring the win32 backend at
            // all is what keeps the overlay from stealing clicks from the game.
            io.MouseDrawCursor = false;

            ImFontConfig cfg;
            cfg.FontDataOwnedByAtlas = false;

            features::render::Watermark::SetFonts(
                io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 17.f, &cfg, io.Fonts->GetGlyphRangesCyrillic()),
                io.Fonts->AddFontFromMemoryTTF(lexend_regular, sizeof(lexend_regular), 14.f, &cfg, io.Fonts->GetGlyphRangesCyrillic()));

            ImGui_ImplOpenGL2_Init();

            g_lastFrame = std::chrono::steady_clock::now();
            g_lastFpsSample = g_lastFrame;
            g_imguiReady = true;
        }

        void DrawOverlay(HDC hdc)
        {
            HWND window = WindowFromDC(hdc);
            if (!window)
                return;

            RECT client{};
            if (!GetClientRect(window, &client))
                return;

            const float width = (float)(client.right - client.left);
            const float height = (float)(client.bottom - client.top);
            if (width < 1.f || height < 1.f)
                return;

            if (!g_imguiReady)
                SetupImGui();

            ImGui::SetCurrentContext(g_context);

            const auto now = std::chrono::steady_clock::now();

            g_frameCount++;
            const auto sinceSample = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastFpsSample).count();
            if (sinceSample >= 500)
            {
                g_fps.store((int)(g_frameCount * 1000.0 / (double)sinceSample));
                g_frameCount = 0;
                g_lastFpsSample = now;
            }

            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(width, height);

            const float delta = std::chrono::duration<float>(now - g_lastFrame).count();
            io.DeltaTime = (delta > 0.f && delta < 1.f) ? delta : (1.f / 60.f);
            g_lastFrame = now;

            ImGui_ImplOpenGL2_NewFrame();
            ImGui::NewFrame();

            features::render::Watermark::Draw(width, height, g_fps.load());

            ImGui::Render();
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        }

        BOOL WINAPI SwapBuffersHook(HDC hdc)
        {
            SwapBuffersFn original = g_original;

            if (g_shuttingDown.load(std::memory_order_acquire) || !original)
                return original ? original(hdc) : FALSE;

            g_activeCalls.fetch_add(1, std::memory_order_acq_rel);

            // Never let a drawing problem take the game down with it.
            __try
            {
                DrawOverlay(hdc);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                g_imguiReady = false;
            }

            g_activeCalls.fetch_sub(1, std::memory_order_acq_rel);

            return original(hdc);
        }
    }

    bool InstallRenderHook()
    {
        if (g_installed.load())
            return true;

        g_shuttingDown.store(false, std::memory_order_release);
        g_activeCalls.store(0, std::memory_order_release);

        HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
        if (!opengl)
            opengl = LoadLibraryW(L"opengl32.dll");

        void* target = opengl ? (void*)GetProcAddress(opengl, "wglSwapBuffers") : nullptr;

        if (!target)
        {
            HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
            target = gdi ? (void*)GetProcAddress(gdi, "SwapBuffers") : nullptr;
        }

        if (!target)
            return false;

        if (MH_CreateHook(target, (void*)SwapBuffersHook, (void**)&g_original) != MH_OK)
            return false;

        if (MH_EnableHook(target) != MH_OK)
        {
            MH_RemoveHook(target);
            return false;
        }

        g_target = target;
        g_installed.store(true);
        return true;
    }

    void RemoveRenderHook()
    {
        if (!g_installed.load())
            return;

        g_shuttingDown.store(true, std::memory_order_release);

        if (g_target)
        {
            MH_DisableHook(g_target);
            MH_RemoveHook(g_target);
        }

        for (int i = 0; i < 200 && g_activeCalls.load(std::memory_order_acquire) > 0; i++)
            Sleep(5);

        // The GL objects belong to the render thread, which is gone from here.
        // Dropping the context without it would corrupt the game's GL state, so
        // the atlas is simply left to the process teardown.
        g_imguiReady = false;
        g_target = nullptr;
        g_original = nullptr;
        g_installed.store(false);
    }

    int GetFramesPerSecond()
    {
        return g_fps.load();
    }
}
