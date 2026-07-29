#pragma once

#include <windows.h>
#include <string>

namespace omega::dll
{

    HWND FindMinecraftWindow();
    std::wstring GetModulePath(HMODULE hModule = nullptr);
    bool IsForegroundWindowMinecraft();
    HANDLE SafeThreadCreate(LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter);

}
