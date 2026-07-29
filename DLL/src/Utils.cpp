#include "Utils.h"
#include <psapi.h>

namespace omega::dll
{

    struct EnumWindowsData
    {
        HWND hwnd = nullptr;
        std::wstring className;
    };

    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
    {
        auto* data = reinterpret_cast<EnumWindowsData*>(lParam);
        wchar_t className[256];
        if (GetClassNameW(hwnd, className, 256))
        {
            std::wstring cls(className);
            if (cls.find(L"LWJGL") != std::wstring::npos ||
                cls.find(L"GLFW") != std::wstring::npos ||
                cls == L"Minecraft" ||
                cls.find(L"Java") != std::wstring::npos)
            {
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                wchar_t exePath[MAX_PATH];
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProc)
                {
                    if (GetModuleBaseNameW(hProc, nullptr, exePath, MAX_PATH))
                    {
                        std::wstring exe(exePath);
                        if (exe.find(L"javaw") != std::wstring::npos ||
                            exe.find(L"java") != std::wstring::npos ||
                            exe.find(L"Minecraft") != std::wstring::npos)
                        {
                            data->hwnd = hwnd;
                            data->className = cls;
                            CloseHandle(hProc);
                            return FALSE;
                        }
                    }
                    CloseHandle(hProc);
                }
            }
        }
        return TRUE;
    }

    HWND FindMinecraftWindow()
    {
        EnumWindowsData data;
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
        return data.hwnd;
    }

    std::wstring GetModulePath(HMODULE hModule)
    {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(hModule, path, MAX_PATH))
        {
            wchar_t* lastSlash = wcsrchr(path, L'\\');
            if (lastSlash)
                *lastSlash = L'\0';
            return std::wstring(path);
        }
        return std::wstring();
    }

    bool IsForegroundWindowMinecraft()
    {
        HWND foreground = GetForegroundWindow();
        if (!foreground)
            return false;

        wchar_t className[256];
        if (!GetClassNameW(foreground, className, 256))
            return false;

        std::wstring cls(className);
        return cls.find(L"LWJGL") != std::wstring::npos ||
               cls.find(L"GLFW") != std::wstring::npos ||
               cls == L"Minecraft";
    }

    HANDLE SafeThreadCreate(LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter)
    {
        DWORD threadId;
        HANDLE hThread = CreateThread(
            nullptr,
            0,
            lpStartAddress,
            lpParameter,
            0,
            &threadId
        );
        return hThread;
    }

}
