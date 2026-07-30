#include "ManualMap.h"
#include <cstring>

namespace omega::loader
{
    namespace
    {
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
            unsigned char movRcx[2];        // 0: 48 B9
            unsigned long long hinstDll;    // 2: imm64 rcx
            unsigned char movRdx[2];        // 10: 48 BA
            unsigned char reasonByte[4];   // 12: imm32 (sign-ext to rdx)
            unsigned char xorR8[3];         // 16: 4D 31 C0
            unsigned char movRax[2];        // 19: 48 B8
            unsigned long long entry;       // 21: imm64 rax
            unsigned char callRax[2];       // 29: FF D0
            unsigned char ret;              // 31: C3
        };
        #pragma pack(pop)

        static_assert(sizeof(LoaderStub) == 32, "LoaderStub must be 32 bytes");

        void BuildStub(LoaderStub& stub, unsigned long long hinst, unsigned long long entry)
        {
            std::memset(&stub, 0, sizeof(stub));
            // mov rcx, imm64
            stub.movRcx[0] = 0x48;
            stub.movRcx[1] = 0xB9;
            stub.hinstDll  = hinst;
            // mov rdx, imm32 sign-extended (48 BA <4 bytes>)
            stub.movRdx[0] = 0x48;
            stub.movRdx[1] = 0xBA;
            stub.reasonByte[0] = 0x01;
            stub.reasonByte[1] = 0x00;
            stub.reasonByte[2] = 0x00;
            stub.reasonByte[3] = 0x00;
            // xor r8, r8
            stub.xorR8[0] = 0x4D;
            stub.xorR8[1] = 0x31;
            stub.xorR8[2] = 0xC0;
            // mov rax, imm64
            stub.movRax[0] = 0x48;
            stub.movRax[1] = 0xB8;
            stub.entry     = entry;
            // call rax
            stub.callRax[0] = 0xFF;
            stub.callRax[1] = 0xD0;
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

                HMODULE localDll = GetModuleHandleA(dllName);
                if (!localDll)
                    localDll = LoadLibraryA(dllName);
                if (!localDll)
                {
                    error = std::string("could not load imported dll: ") + dllName;
                    VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
                    return nullptr;
                }

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
                        VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
                        return nullptr;
                    }

                    if (!WriteProcessMemory(process, iatSlot, &proc, sizeof(proc), nullptr))
                    {
                        error = "WriteProcessMemory(IAT) failed (" + std::to_string(GetLastError()) + ")";
                        VirtualFreeEx(process, remoteBase, 0, MEM_RELEASE);
                        return nullptr;
                    }
                }
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
                    DWORD blockCount = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                    WORD* entry = reinterpret_cast<WORD*>(reinterpret_cast<unsigned char*>(reloc) + sizeof(IMAGE_BASE_RELOCATION));
                    for (DWORD b = 0; b < blockCount; b++, entry++)
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
