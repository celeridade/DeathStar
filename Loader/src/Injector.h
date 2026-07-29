#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "Shared.h"

namespace omega::loader
{
    struct ProcessEntry
    {
        DWORD pid = 0;
        std::wstring name;
        std::string nameUtf8;   // "javaw.exe"
        std::string title;      // the game window's title, e.g. "CheatBreaker 1.8.9"
        std::string label;      // what the combo shows, kept short
        bool is64bit = true;

        // Window title when the game has one up, exe name until then.
        const std::string& Display() const { return title.empty() ? nameUtf8 : title; }
    };

    class Injector
    {
    public:
        // Rebuilds the process list and the injection status.
        void Refresh();
        // Cheap throttled Refresh, safe to call every frame.
        void Poll();

        const std::vector<ProcessEntry>& Processes() const { return m_processes; }
        const std::vector<const char*>& Labels() const { return m_labels; }
        const ProcessEntry* SelectedProcess() const;
        int* SelectedIndex() { return &m_selected; }

        bool Inject(std::string& error);
        bool Eject(shared::SharedConfig* config, std::string& error);

        bool IsInjected() const { return m_injected; }
        bool HasDll() const { return m_dllFound; }
        const std::wstring& DllPath() const { return m_dllPath; }
        const std::string& DllPathUtf8() const { return m_dllPathUtf8; }

    private:
        void ResolveDllPath();
        bool IsDllLoaded(DWORD pid) const;

        std::vector<ProcessEntry> m_processes;
        std::vector<const char*> m_labels;
        int m_selected = 0;
        bool m_injected = false;
        bool m_dllFound = false;
        std::wstring m_dllPath;
        std::string m_dllPathUtf8;
        ULONGLONG m_lastPoll = 0;
    };
}
