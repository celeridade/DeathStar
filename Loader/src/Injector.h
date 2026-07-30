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
        std::string title;      // raw window title, e.g. "Badlion Minecraft Client v4.4.4-... (1.8.9)"
        std::string shortName;  // trimmed for the picker, e.g. "Badlion 1.8.9"
        std::string label;      // what the combo shows, kept short
        bool is64bit = true;

        // Short client name when we could work one out, exe name until then.
        const std::string& Display() const { return shortName.empty() ? nameUtf8 : shortName; }
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

        // Set the shared memory so Inject can flag the dll about its load path
        // before the dll main thread reads it.
        void SetShared(shared::SharedConfig* shared) { m_shared = shared; }

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

        // Manual mapping: the embedded dll bytes loaded from the resource and
        // the base address of the image we mapped into the target. The address
        // is zero when nothing is mapped, which replaces the old "DeathStar.dll
        // in the module list" check.
        std::vector<unsigned char> m_dllBytes;
        void* m_remoteBase = nullptr;
        shared::SharedConfig* m_shared = nullptr;
    };
}
