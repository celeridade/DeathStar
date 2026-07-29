#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

#include <imgui.h>
#include <imgui_edited.hpp>
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "font.h"

#include "Shared.h"
#include "Config.h"
#include "Injector.h"
#include "Menu.h"
#include "SharedMemory.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace omega;

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int kWindowWidth = shared::WINDOW_WIDTH;
static const int kWindowHeight = shared::WINDOW_HEIGHT + (int)loader::Menu::TOP_BAR;

// Set every frame by the menu: true wherever the cursor is not over a widget,
// which is what lets the whole panel drag the window around.
static bool g_allowDrag = true;

// Keys pure black out of the window so the panels keep their rounded corners and
// the gap around the info bar stays see-through. Set to false for a plain opaque
// window if a driver ever renders the layered window badly.
static constexpr bool kTransparentBackground = true;

static void LoadFonts()
{
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig cfg;
    // The ttf data lives in font.h, the atlas must not try to free it.
    cfg.FontDataOwnedByAtlas = false;

    font::lexend_general_bold = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 18.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::lexend_title = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 34.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::lexend_bold = io.Fonts->AddFontFromMemoryTTF(lexend_regular, sizeof(lexend_regular), 17.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::lexend_regular = io.Fonts->AddFontFromMemoryTTF(lexend_regular, sizeof(lexend_regular), 14.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::icomoon = io.Fonts->AddFontFromMemoryTTF(icomoon, sizeof(icomoon), 20.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    font::icomoon_widget = io.Fonts->AddFontFromMemoryTTF(icomoon_widget, sizeof(icomoon_widget), 15.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::icomoon_widget2 = io.Fonts->AddFontFromMemoryTTF(icomoon, sizeof(icomoon), 16.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    // A second loader would fight over the shared memory and the injection.
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, shared::MUTEX_NAME);
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr, L"DeathStar is already running.", shared::NAME, MB_OK | MB_ICONINFORMATION);
        CloseHandle(instanceMutex);
        return 0;
    }

    loader::SharedMemory sharedMemory;
    if (!sharedMemory.Create())
    {
        MessageBoxW(nullptr, L"Could not create the shared memory block.", shared::NAME, MB_OK | MB_ICONERROR);
        return 1;
    }

    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr,
                       ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, shared::WINDOW_CLASS, nullptr };
    ::RegisterClassExW(&wc);

    const int x = (::GetSystemMetrics(SM_CXSCREEN) - kWindowWidth) / 2;
    const int y = (::GetSystemMetrics(SM_CYSCREEN) - kWindowHeight) / 2;

    const DWORD exStyle = WS_EX_APPWINDOW | (kTransparentBackground ? WS_EX_LAYERED : 0);

    HWND hwnd = ::CreateWindowExW(exStyle, wc.lpszClassName, shared::NAME,
        WS_OVERLAPPED, x, y, kWindowWidth, kWindowHeight, nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd)
    {
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    if (kTransparentBackground)
        ::SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    LoadFonts();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    c::background::size = ImVec2((float)shared::WINDOW_WIDTH, (float)shared::WINDOW_HEIGHT);

    loader::Config config;
    loader::Injector injector;
    loader::Menu menu;

    injector.Refresh();

    menu.SetConfig(&config);
    menu.SetInjector(&injector);
    menu.SetShared(sharedMemory.Get());

    bool done = false;
    // Tracks the enabled flag we last wrote, so a change coming back from the
    // dll (the in-game keybind toggle) can be told apart from our own writes.
    bool lastEnabled = config.settings.autoClickerEnabled;
    bool lastHideClan = config.settings.hideClanEnabled;

    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        injector.Poll();

        // The keybind toggles the clicker from inside the game; adopt that so the
        // checkbox in the menu turns on and off with it.
        shared::SharedConfig* sc = sharedMemory.Get();
        if (sc->autoClickerEnabled != lastEnabled)
            config.settings.autoClickerEnabled = sc->autoClickerEnabled;
        if (sc->hideClanEnabled != lastHideClan)
            config.settings.hideClanEnabled = sc->hideClanEnabled;

        config.ApplyTo(*sc);
        lastEnabled = sc->autoClickerEnabled;
        lastHideClan = sc->hideClanEnabled;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        menu.Draw();
        g_allowDrag = menu.AllowDrag();

        if (menu.WantsExit() || ImGui::IsKeyPressed(ImGuiKey_Escape))
            done = true;

        ImGui::Render();

        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    config.Save();

    // Exit pulls the dll out of the game too, so nothing of it is left running.
    {
        std::string error;
        injector.Eject(sharedMemory.Get(), error);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (instanceMutex)
    {
        ::ReleaseMutex(instanceMutex);
        ::CloseHandle(instanceMutex);
    }

    return 0;
}

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (!pBackBuffer)
        return;

    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

static void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        // Any empty spot on the panel drags the window; the menu tells us when
        // the cursor is not over a widget so a drag never eats a click.
        if (g_allowDrag)
            return HTCAPTION;

        break;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
