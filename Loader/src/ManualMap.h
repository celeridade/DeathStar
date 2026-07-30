#pragma once
#include <windows.h>
#include <string>

namespace omega::loader
{
    // Manual-maps a DLL image buffer into a remote process. The dll never touches
    // disk: its bytes are copied straight from the loader's own memory into a
    // freshly allocated image region in the target, its imports are resolved
    // against the target's loaded modules, relocations are applied, and DllMain
    // is called with DLL_PROCESS_ATTACH through a remote thread.
    //
    // dllData  : pointer to the raw PE file bytes (as they would be on disk)
    // dllSize  : size of those bytes
    // process  : opened target process with VM write/query/create-thread rights
    // Returns the remote base address on success, or nullptr on failure. error
    // receives a human readable description when the function fails.
    void* ManualMapDll(const void* dllData, size_t dllSize, HANDLE process, std::string& error);

    // Unloads a previously manual-mapped dll. Walks the export directory for
    // DllMain and calls it with DLL_PROCESS_DETACH through a remote thread.
    // Not currently wired because the shared-memory shutdown path is enough.
}
