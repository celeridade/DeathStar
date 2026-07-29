#include "Injector.h"
#include <tlhelp32.h>
#include <algorithm>
#include <cstring>
#include <cwchar>

namespace omega::loader
{
    namespace
    {
        std::string ToUtf8(const std::wstring& value)
        {
            if (value.empty())
                return {};

            const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), nullptr, 0, nullptr, nullptr);
            std::string result(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), result.data(), size, nullptr, nullptr);
            return result;
        }

        bool IsTargetProcess(const wchar_t* name)
        {
            for (int i = 0; i < shared::PROCESS_NAME_COUNT; i++)
            {
                if (_wcsicmp(name, shared::PROCESS_NAMES[i]) == 0)
                    return true;
            }
            return false;
        }

        std::string LastErrorText(const char* what)
        {
            return std::string(what) + " (error " + std::to_string(GetLastError()) + ")";
        }

        struct TitleSearch
        {
            DWORD pid = 0;
            std::wstring title;
        };

        BOOL CALLBACK TitleForPidProc(HWND window, LPARAM param)
        {
            auto* search = reinterpret_cast<TitleSearch*>(param);

            DWORD owner = 0;
            GetWindowThreadProcessId(window, &owner);
            if (owner != search->pid)
                return TRUE;

            // Top-level, visible, and actually titled: that is the game window,
            // not the launcher's hidden helpers.
            if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr)
                return TRUE;

            const int length = GetWindowTextLengthW(window);
            if (length <= 0)
                return TRUE;

            std::wstring text(length + 1, L'\0');
            const int copied = GetWindowTextW(window, text.data(), length + 1);
            text.resize(copied > 0 ? copied : 0);

            if (!text.empty())
            {
                search->title = std::move(text);
                return FALSE;   // found it, stop enumerating
            }

            return TRUE;
        }

        // Empty until the game opens its window; the picker falls back to the
        // exe name in the meantime.
        std::string WindowTitleForPid(DWORD pid)
        {
            TitleSearch search;
            search.pid = pid;
            EnumWindows(TitleForPidProc, reinterpret_cast<LPARAM>(&search));
            return ToUtf8(search.title);
        }
    }

    void Injector::ResolveDllPath()
    {
        wchar_t path[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
        {
            m_dllFound = false;
            return;
        }

        std::wstring directory(path);
        const size_t slash = directory.find_last_of(L'\\');
        if (slash != std::wstring::npos)
            directory.resize(slash + 1);

        m_dllPath = directory + shared::DLL_NAME;
        m_dllPathUtf8 = ToUtf8(m_dllPath);

        const DWORD attributes = GetFileAttributesW(m_dllPath.c_str());
        m_dllFound = (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool Injector::IsDllLoaded(DWORD pid) const
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (snapshot == INVALID_HANDLE_VALUE)
            return false;

        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        bool found = false;
        if (Module32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szModule, shared::DLL_NAME) == 0)
                {
                    found = true;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return found;
    }

    void Injector::Refresh()
    {
        ResolveDllPath();

        const ProcessEntry* previous = SelectedProcess();
        const DWORD previousPid = previous ? previous->pid : 0;

        m_processes.clear();
        m_labels.clear();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);

            if (Process32FirstW(snapshot, &entry))
            {
                do
                {
                    if (!IsTargetProcess(entry.szExeFile))
                        continue;

                    ProcessEntry process;
                    process.pid = entry.th32ProcessID;
                    process.name = entry.szExeFile;
                    process.nameUtf8 = ToUtf8(process.name);
                    process.title = WindowTitleForPid(process.pid);

                    // Window title once the game has one, exe name before that.
                    process.label = process.Display();

                    if (HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.pid))
                    {
                        BOOL wow64 = FALSE;
                        if (IsWow64Process(handle, &wow64))
                            process.is64bit = (wow64 == FALSE);
                        CloseHandle(handle);
                    }

                    m_processes.push_back(std::move(process));
                } while (Process32NextW(snapshot, &entry));
            }

            CloseHandle(snapshot);
        }

        std::sort(m_processes.begin(), m_processes.end(),
            [](const ProcessEntry& a, const ProcessEntry& b) { return a.pid < b.pid; });

        // Built only once the vector is final, the labels point into its strings.
        m_labels.reserve(m_processes.size());
        for (const ProcessEntry& process : m_processes)
            m_labels.push_back(process.label.c_str());

        m_selected = 0;
        if (previousPid != 0)
        {
            for (size_t i = 0; i < m_processes.size(); i++)
            {
                if (m_processes[i].pid == previousPid)
                {
                    m_selected = (int)i;
                    break;
                }
            }
        }

        const ProcessEntry* selected = SelectedProcess();
        m_injected = selected && IsDllLoaded(selected->pid);
    }

    void Injector::Poll()
    {
        const ULONGLONG now = GetTickCount64();
        if (m_lastPoll != 0 && now - m_lastPoll < 1000)
            return;

        m_lastPoll = now;
        Refresh();
    }

    const ProcessEntry* Injector::SelectedProcess() const
    {
        if (m_selected < 0 || m_selected >= (int)m_processes.size())
            return nullptr;

        return &m_processes[m_selected];
    }

    bool Injector::Inject(std::string& error)
    {
        const ProcessEntry* target = SelectedProcess();
        if (!target)
        {
            error = "No Minecraft process selected.";
            return false;
        }

        if (!m_dllFound)
        {
            error = "DeathStar.dll was not found next to the loader.";
            return false;
        }

        if (!target->is64bit)
        {
            error = "The selected process is 32-bit, DeathStar is x64.";
            return false;
        }

        // Already there, so picking the same game again just re-opens the menu.
        if (IsDllLoaded(target->pid))
        {
            m_injected = true;
            return true;
        }

        HANDLE process = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE, target->pid);

        if (!process)
        {
            error = LastErrorText("Could not open the process, try running the loader as administrator");
            return false;
        }

        bool success = false;
        const SIZE_T size = (m_dllPath.size() + 1) * sizeof(wchar_t);
        LPVOID remotePath = VirtualAllocEx(process, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (!remotePath)
        {
            error = LastErrorText("Could not allocate memory in the process");
        }
        else if (!WriteProcessMemory(process, remotePath, m_dllPath.c_str(), size, nullptr))
        {
            error = LastErrorText("Could not write the dll path into the process");
        }
        else
        {
            // kernel32 sits at the same address in every process of the session,
            // so the local LoadLibraryW address is valid in the target too.
            auto loadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");

            HANDLE thread = loadLibrary
                ? CreateRemoteThread(process, nullptr, 0, loadLibrary, remotePath, 0, nullptr)
                : nullptr;

            if (!thread)
            {
                error = LastErrorText("Could not start the remote thread");
            }
            else
            {
                WaitForSingleObject(thread, 10000);

                DWORD exitCode = 0;
                GetExitCodeThread(thread, &exitCode);
                CloseHandle(thread);

                if (exitCode == 0)
                    error = "LoadLibrary failed inside the game, the dll was rejected.";
                else
                    success = true;
            }
        }

        if (remotePath)
            VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);

        CloseHandle(process);

        if (success)
            m_injected = true;

        return success;
    }

    bool Injector::Eject(shared::SharedConfig* config, std::string& error)
    {
        const ProcessEntry* target = SelectedProcess();
        if (!target)
        {
            error = "No Minecraft process selected.";
            return false;
        }

        if (!IsDllLoaded(target->pid))
        {
            error = "DeathStar is not loaded in that process.";
            m_injected = false;
            return false;
        }

        // Preferred path: ask the DLL to stop its threads and unload itself.
        if (config && config->dllAttached)
        {
            config->shutdownRequested = true;

            for (int i = 0; i < 60 && config->dllAttached; i++)
                Sleep(50);

            if (!config->dllAttached)
            {
                // Give FreeLibraryAndExitThread a moment to actually drop the module.
                for (int i = 0; i < 20 && IsDllLoaded(target->pid); i++)
                    Sleep(50);

                m_injected = IsDllLoaded(target->pid);
                if (!m_injected)
                    return true;
            }

            config->shutdownRequested = false;
            error = "The dll did not answer the unload request, restart the game.";
            return false;
        }

        // Fallback: the dll is loaded but never attached, so nothing of it is running.
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, target->pid);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            error = LastErrorText("Could not read the module list");
            return false;
        }

        HMODULE remoteModule = nullptr;
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (Module32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szModule, shared::DLL_NAME) == 0)
                {
                    remoteModule = entry.hModule;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);

        if (!remoteModule)
        {
            error = "DeathStar is not loaded in that process.";
            m_injected = false;
            return false;
        }

        HANDLE process = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE, target->pid);

        if (!process)
        {
            error = LastErrorText("Could not open the process");
            return false;
        }

        auto freeLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");

        HANDLE thread = freeLibrary
            ? CreateRemoteThread(process, nullptr, 0, freeLibrary, remoteModule, 0, nullptr)
            : nullptr;

        bool success = false;
        if (!thread)
        {
            error = LastErrorText("Could not start the remote thread");
        }
        else
        {
            WaitForSingleObject(thread, 10000);
            CloseHandle(thread);
            success = !IsDllLoaded(target->pid);

            if (!success)
                error = "FreeLibrary did not unload the dll.";
        }

        CloseHandle(process);
        m_injected = IsDllLoaded(target->pid);
        return success;
    }
}
