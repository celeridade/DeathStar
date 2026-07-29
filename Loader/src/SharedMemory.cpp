#include "SharedMemory.h"

namespace omega::loader
{
    SharedMemory::~SharedMemory()
    {
        Destroy();
    }

    bool SharedMemory::Create()
    {
        if (m_config)
            return true;

        m_handle = CreateFileMappingW(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, sizeof(shared::SharedConfig), shared::SHARED_MEM_NAME);

        if (!m_handle)
            return false;

        const bool existed = (GetLastError() == ERROR_ALREADY_EXISTS);

        m_config = (shared::SharedConfig*)MapViewOfFile(
            m_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(shared::SharedConfig));

        if (!m_config)
        {
            CloseHandle(m_handle);
            m_handle = nullptr;
            return false;
        }

        // A fresh mapping is zeroed, so it needs the defaults written into it.
        // An existing one already has a DLL talking to it: leave it alone.
        if (!existed)
            *m_config = shared::SharedConfig();

        return true;
    }

    void SharedMemory::Destroy()
    {
        if (m_config)
        {
            UnmapViewOfFile(m_config);
            m_config = nullptr;
        }
        if (m_handle)
        {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }
}
