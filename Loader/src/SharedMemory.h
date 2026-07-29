#pragma once
#include <windows.h>
#include "Shared.h"

namespace omega::loader
{
    // Owns the file mapping the injected DLL opens to read the settings from.
    // It has to exist before the DLL is injected, so the Loader creates it on startup.
    class SharedMemory
    {
    public:
        SharedMemory() = default;
        ~SharedMemory();

        bool Create();
        void Destroy();

        shared::SharedConfig* Get() const { return m_config; }
        bool IsValid() const { return m_config != nullptr; }

    private:
        HANDLE m_handle = nullptr;
        shared::SharedConfig* m_config = nullptr;
    };
}
