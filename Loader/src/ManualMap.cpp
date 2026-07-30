#include "ManualMap.h"
#include <cstring>
#include <tlhelp32.h>

namespace omega::loader
{
    namespace
    {
        struct RemoteModule
        {
            void* base = nullptr;
            std::wstring path;
        };

        // Where a given dll actually sits in the target. System dlls happen to
        // share a base across processes, but anything else (jvm.dll and friends)
        // does not, so the target's own base is the only correct answer.
        bool FindRemoteModule(HANDLE process, const char* dllName, RemoteModule& out)
        {
            const DWORD pid = GetProcessId(process);
            if (pid == 0)
                return false;

            wchar_t wanted[MAX_PATH]{};
            if (MultiByteToWideChar(CP_ACP, 0, dllName, -1, wanted, MAX_PATH) == 0)
                return false;

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
                    if (_wcsicmp(entry.szModule, wanted) == 0)
                    {
                        out.base = entry.modBaseAddr;
                        out.path = entry.szExePath;
                        found = true;
                        break;
                    }
                } while (Module32NextW(snapshot, &entry));
            }

            CloseHandle(snapshot);
            return found;
        }
        PIMAGE_NT_HEADERS NtHeaders(unsigned char* base)
        {
            PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
            return reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
        }

        PIMAGE_SECTION_HEADER FirstSection(unsigned char* base)
        {
            PIMAGE_NT_HEADERS nt = NtHeaders(base);
            return IMAGE_FIRST_SECTION(nt);
        }

        DWORD RvaToFileOffset(unsigned char* base, DWORD rva)
        {
            PIMAGE_NT_HEADERS nt = NtHeaders(base);
            PIMAGE_SECTION_HEADER sec = FirstSection(base);
            for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
            {
                if (rva >= sec->VirtualAddress && rva < sec->VirtualAddress + sec->Misc.VirtualSize)
                    return rva - sec->VirtualAddress + sec->PointerToRawData;
            }
            return rva;
        }

        // A tiny x64 shellcode stub that calls DllMain(base, 1, 0). The thread
        // callback only gets one parameter (in rcx), so we cannot pass three
        // directly through CreateRemoteThread. We allocate this stub, write it
        // into the target, and use it as the thread routine. The stub stores the
        // dll base and entry pointer inline; Windows x64 calling convention uses
        // rcx/rdx/r8 for the first three integer args.
        #pragma pack(push, 1)
        struct LoaderStub
        {
            unsigned char movRcx[2];        //  0: 48 B9          mov rcx, imm64
            unsigned long long hinstDll;    //  2: imm64
            unsigned char movEdx;           // 10: BA             mov edx, imm32
            unsigned int reason;            // 11: imm32 (zero-extends into rdx)
            unsigned char xorR8[3];         // 15: 4D 31 C0       xor r8, r8
            unsigned char movRax[2];        // 18: 48 B8          mov rax, imm64
            unsigned long long entry;       // 20: imm64
            unsigned char subRsp[4];        // 28: 48 83 EC 28    sub rsp, 0x28
            unsigned char callRax[2];       // 32: FF D0          call rax
            unsigned char addRsp[4];        // 34: 48 83 C4 28    add rsp, 0x28
            unsigned char ret;              // 38: C3
        };
        #pragma pack(pop)

        static_assert(sizeof(LoaderStub) == 39, "LoaderStub must be 39 bytes");

        void BuildStub(LoaderStub& stub, unsigned long long hinst, unsigned long long entry)
        {
            std::memset(&stub, 0, sizeof(stub));

            // mov rcx, hinst
            stub.movRcx[0] = 0x48;
            stub.movRcx[1] = 0xB9;
            stub.hinstDll  = hinst;

            // mov edx, 1 (DLL_PROCESS_ATTACH). Must be the 32-bit form: the
            // 64-bit "48 BA" needs an 8-byte immediate, and feeding it only four
            // makes the CPU eat the following instructions as part of the
            // immediate, so execution runs off into garbage.
            stub.movEdx = 0xBA;
            stub.reason = 1;

            // xor r8, r8
            stub.xorR8[0] = 0x4D;
            stub.xorR8[1] = 0x31;
            stub.xorR8[2] = 0xC0;

            // mov rax, entry
            stub.movRax[0] = 0x48;
            stub.movRax[1] = 0xB8;
            stub.entry     = entry;

            // sub rsp, 0x28 - 32 bytes of shadow space the callee is allowed to
            // use, plus 8 to land the call on a 16-byte aligned rsp.
            stub.subRsp[0] = 0x48; stub.subRsp[1] = 0x83; stub.subRsp[2] = 0xEC; stub.subRsp[3] = 0x28;

            // call rax
            stub.callRax[0] = 0xFF;
            stub.callRax[1] = 0xD0;

            // add rsp, 0x28
            stub.addRsp[0] = 0x48; stub.addRsp[1] = 0x83; stub.addRsp[2] = 0xC4; stub.addRsp[3] = 0x28;

            // ret
            stub.ret = 0xC3;
        }
    }

    void* ManualMapDll(const void* dllData, size_t dllSize, HANDLE process, std::string& error)
    {
        if (!dllData || dllSize == 0 || !process)
        {
            error = "invalid arguments to ManualMapDll";
            return nullptr;
        }

        auto* base = const_cast<unsigned char*>(static_cast<const unsigned char*>(dllData));

        PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        {
            error = "embedded dll is not a valid PE (bad DOS signature)";
            return nullptr;
        }
        PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
        {
            error = "embedded dll has no PE signature";
            return nullptr;
        }

        const SIZE_T imageSize = nt->OptionalHeader.SizeOfImage;

        LPVOID remoteBase = VirtualAllocEx(process, nullptr, imageSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!remoteBase)
        {
            error = "VirtualAllocEx failed (" + std::to_string(GetLastError()) + ")";
            return nullptr;
        }

        SIZE_T written = 0;
        if (!WriteProcessMemory(process, remoteBase, base, nt->OptionalHeader.SizeOfHeaders, &written))
        {
            error = "WriteProcessMemory(headers) failed (" + std::to_string(GetLastError()) + ")";
            VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
            return nullptr;
        }

        PIMAGE_SECTION_HEADER sec = FirstSection(base);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
        {
            if (sec->SizeOfRawData == 0)
                continue;
            void* dest = reinterpret_cast<unsigned char*>(remoteBase) + sec->VirtualAddress;
            void* src  = base + sec->PointerToRawData;
            if (!WriteProcessMemory(process, dest, src, sec->SizeOfRawData, nullptr))
            {
                error = "WriteProcessMemory(section " + std::to_string(i) + ") failed (" + std::to_string(GetLastError()) + ")";
                VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
                return nullptr;
            }
        }

        //----- Resolve the import address table -------------------------------------
        DWORD importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (importRva != 0)
        {
            DWORD importFile = RvaToFileOffset(base, importRva);
            PIMAGE_IMPORT_DESCRIPTOR imp = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(base + importFile);
            for (; imp->Name != 0; imp++)
            {
                const char* dllName = reinterpret_cast<const char*>(base + RvaToFileOffset(base, imp->Name));

                // Prefer the copy already loaded in the target: load the very
                // same file here (without running it) purely to read export
                // RVAs, then rebase those onto the target's module.
                RemoteModule remoteModule;
                HMODULE localDll = nullptr;
                bool freeLocal = false;
                bool rebase = false;

                if (FindRemoteModule(process, dllName, remoteModule))
                {
                    localDll = LoadLibraryExW(remoteModule.path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
                    if (localDll)
                    {
                        freeLocal = true;
                        rebase = true;
                    }
                }

                if (!localDll)
                {
                    localDll = GetModuleHandleA(dllName);
                    if (!localDll)
                    {
                        localDll = LoadLibraryA(dllName);
                        freeLocal = (localDll != nullptr);
                    }
                }

                if (!localDll)
                {
                    error = std::string("could not load imported dll: ") + dllName;
                    VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
                    return nullptr;
                }

                // Only rebase addresses that really live inside this image;
                // forwarded exports point elsewhere and must be used as-is.
                unsigned char* localBase = reinterpret_cast<unsigned char*>(localDll);
                const DWORD localSize = NtHeaders(localBase)->OptionalHeader.SizeOfImage;

                DWORD thunkRva = imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk;
                DWORD thunkFile = RvaToFileOffset(base, thunkRva);
                PIMAGE_THUNK_DATA thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(base + thunkFile);
                LPVOID iatSlot = reinterpret_cast<unsigned char*>(remoteBase) + imp->FirstThunk;

                for (; thunk->u1.AddressOfData != 0; thunk++, iatSlot = reinterpret_cast<unsigned char*>(iatSlot) + sizeof(IMAGE_THUNK_DATA))
                {
                    FARPROC proc = nullptr;
                    if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal))
                    {
                        proc = GetProcAddress(localDll, MAKEINTRESOURCEA(IMAGE_ORDINAL(thunk->u1.Ordinal)));
                    }
                    else
                    {
                        PIMAGE_IMPORT_BY_NAME in = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(base + RvaToFileOffset(base, static_cast<DWORD>(thunk->u1.AddressOfData)));
                        proc = GetProcAddress(localDll, reinterpret_cast<const char*>(in->Name));
                    }

                    if (!proc)
                    {
                        error = "could not resolve an imported function";
                        if (freeLocal) FreeLibrary(localDll);
                        VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
                        return nullptr;
                    }

                    void* finalProc = reinterpret_cast<void*>(proc);

                    if (rebase)
                    {
                        unsigned char* addr = reinterpret_cast<unsigned char*>(proc);
                        if (addr >= localBase && addr < localBase + localSize)
                            finalProc = reinterpret_cast<unsigned char*>(remoteModule.base) + (addr - localBase);
                    }

                    if (!WriteProcessMemory(process, iatSlot, &finalProc, sizeof(finalProc), nullptr))
                    {
                        error = "WriteProcessMemory(IAT) failed (" + std::to_string(GetLastError()) + ")";
                        if (freeLocal) FreeLibrary(localDll);
                        VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
                        return nullptr;
                    }
                }

                if (freeLocal)
                    FreeLibrary(localDll);
            }
        }

        //----- Apply base relocations ----------------------------------------------
        const DWORD_PTR delta = reinterpret_cast<DWORD_PTR>(remoteBase) - nt->OptionalHeader.ImageBase;
        if (delta != 0)
        {
            DWORD relocRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
            if (relocRva != 0)
            {
                DWORD relocFile = RvaToFileOffset(base, relocRva);
                PIMAGE_BASE_RELOCATION reloc = reinterpret_cast<PIMAGE_BASE_RELOCATION>(base + relocFile);
                DWORD relocSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
                DWORD done = 0;
                while (done < relocSize)
                {
                    // A zero-sized block would spin here forever.
                    if (reloc->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION))
                        break;

                    DWORD blockCount = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                    WORD* entry = reinterpret_cast<WORD*>(reinterpret_cast<unsigned char*>(reloc) + sizeof(IMAGE_BASE_RELOCATION));
                    // Index only: advancing the pointer as well would read
                    // element 2b on step b, applying garbage relocations.
                    for (DWORD b = 0; b < blockCount; b++)
                    {
                        DWORD type = entry[b] >> 12;
                        DWORD offset = entry[b] & 0x0FFF;
                        LPVOID target = reinterpret_cast<unsigned char*>(remoteBase) + reloc->VirtualAddress + offset;
                        switch (type)
                        {
                        case IMAGE_REL_BASED_ABSOLUTE:
                            break;
                        case IMAGE_REL_BASED_HIGHLOW:
                        {
                            DWORD v;
                            if (!ReadProcessMemory(process, target, &v, sizeof(v), nullptr)) break;
                            v += (DWORD)delta;
                            WriteProcessMemory(process, target, &v, sizeof(v), nullptr);
                            break;
                        }
                        case IMAGE_REL_BASED_DIR64:
                        {
                            ULONGLONG v;
                            if (!ReadProcessMemory(process, target, &v, sizeof(v), nullptr)) break;
                            v += (ULONGLONG)delta;
                            WriteProcessMemory(process, target, &v, sizeof(v), nullptr);
                            break;
                        }
                        default:
                            break;
                        }
                    }
                    done += reloc->SizeOfBlock;
                    reloc = reinterpret_cast<PIMAGE_BASE_RELOCATION>(reinterpret_cast<unsigned char*>(reloc) + reloc->SizeOfBlock);
                }
            }
        }

        //----- Call DllMain via a loader stub ---------------------------------------
        DWORD entryRva = nt->OptionalHeader.AddressOfEntryPoint;
        if (entryRva == 0)
        {
            error = "dll has no entry point, cannot start";
            VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
            return nullptr;
        }

        unsigned long long entry = reinterpret_cast<unsigned long long>(remoteBase) + entryRva;
        unsigned long long hinst = reinterpret_cast<unsigned long long>(remoteBase);

        LoaderStub stub;
        BuildStub(stub, hinst, entry);

        LPVOID stubMem = VirtualAllocEx(process, nullptr, sizeof(stub),
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!stubMem)
        {
            error = "VirtualAllocEx(stub) failed (" + std::to_string(GetLastError()) + ")";
            VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
            return nullptr;
        }

        if (!WriteProcessMemory(process, stubMem, &stub, sizeof(stub), nullptr))
        {
            error = "WriteProcessMemory(stub) failed (" + std::to_string(GetLastError()) + ")";
            VirtualFreeEx(process, stubMem, 0, MEM_RELEASE);
            VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
            return nullptr;
        }

        HANDLE thread = CreateRemoteThread(process, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(stubMem),
            nullptr, 0, nullptr);

        if (!thread)
        {
            error = "CreateRemoteThread(stub) failed (" + std::to_string(GetLastError()) + ")";
            VirtualFreeEx(process, stubMem, 0, MEM_RELEASE);
            VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
            return nullptr;
        }

        WaitForSingleObject(thread, 10000);
        DWORD exitCode = 0;
        GetExitCodeThread(thread, &exitCode);
        CloseHandle(thread);

        // Free the stub — it has already run.
        VirtualFreeEx(process, stubMem, 0, MEM_RELEASE);

        if (exitCode == 0)
        {
            error = "DllMain returned FALSE inside the target.";
            VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
            return nullptr;
        }

        return remoteBase;
    }
}
