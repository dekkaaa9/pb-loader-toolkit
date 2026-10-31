// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
// PBInjector — Manual Map DLL Injector for Point Blank ID
// Compile as 32-bit: injects 32-bit PBCheat.dll into PointBlank.exe
// Usage: PBInjector.exe <path-to-PBCheat.dll> [process-name]

#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <vector>
#include <string>

// ============================================================
// Dual-output logger: console + log file
// ============================================================
static FILE* g_log = nullptr;

static void Log(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    fputs(buf, stdout);
    fflush(stdout);
    if (g_log) {
        fputs(buf, g_log);
        fflush(g_log);
    }
}

// ============================================================
// PE structures (32-bit)
// ============================================================
typedef struct {
    WORD  Machine;
    WORD  NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD  SizeOfOptionalHeader;
    WORD  Characteristics;
} MY_FILE_HEADER;

typedef struct {
    DWORD VirtualAddress;
    DWORD Size;
} MY_DATA_DIRECTORY;

typedef struct {
    WORD  Magic;
    BYTE  MajorLinkerVersion;
    BYTE  MinorLinkerVersion;
    DWORD SizeOfCode;
    DWORD SizeOfInitializedData;
    DWORD SizeOfUninitializedData;
    DWORD AddressOfEntryPoint;
    DWORD BaseOfCode;
    DWORD BaseOfData;
    DWORD ImageBase;
    DWORD SectionAlignment;
    DWORD FileAlignment;
    WORD  MajorOperatingSystemVersion;
    WORD  MinorOperatingSystemVersion;
    WORD  MajorImageVersion;
    WORD  MinorImageVersion;
    WORD  MajorSubsystemVersion;
    WORD  MinorSubsystemVersion;
    DWORD Win32VersionValue;
    DWORD SizeOfImage;
    DWORD SizeOfHeaders;
    DWORD CheckSum;
    WORD  Subsystem;
    WORD  DllCharacteristics;
    DWORD SizeOfStackReserve;
    DWORD SizeOfStackCommit;
    DWORD SizeOfHeapReserve;
    DWORD SizeOfHeapCommit;
    DWORD LoaderFlags;
    DWORD NumberOfRvaAndSizes;
    MY_DATA_DIRECTORY DataDirectory[16];
} MY_OPTIONAL_HEADER32;

typedef struct {
    DWORD Signature;
    MY_FILE_HEADER FileHeader;
    MY_OPTIONAL_HEADER32 OptionalHeader;
} MY_NT_HEADERS32;

typedef struct {
    BYTE  Name[8];
    DWORD VirtualSize;
    DWORD VirtualAddress;
    DWORD SizeOfRawData;
    DWORD PointerToRawData;
    DWORD PointerToRelocations;
    DWORD PointerToLinenumbers;
    WORD  NumberOfRelocations;
    WORD  NumberOfLinenumbers;
    DWORD Characteristics;
} MY_SECTION_HEADER;

// ============================================================
// Shellcode: call DllMain(hModule, DLL_PROCESS_ATTACH, NULL)
// x86 (32-bit) code injected into the target process
// ============================================================
struct ShellcodeData {
    uint32_t pDllMain;   // address of DllMain in target
    uint32_t hModule;    // base address of mapped DLL in target
    uint32_t pLoadLibraryA;
    uint32_t pGetProcAddress;
    uint32_t pRtlAddFunctionTable; // optional, for SEH
};

// The shellcode is position-independent x86 assembly:
//   push 0
//   push 1
//   push hModule
//   call pDllMain
//   ret
static const uint8_t g_shellcode[] = {
    0x6A, 0x00,                         // push 0
    0x6A, 0x01,                         // push 1
    0x68, 0x00, 0x00, 0x00, 0x00,       // push hModule (patched)
    0xB8, 0x00, 0x00, 0x00, 0x00,       // mov eax, pDllMain (patched)
    0xFF, 0xD0,                         // call eax
    0xC3                                // ret
};

// ============================================================
// Find process by name
// ============================================================
static DWORD FindProcess(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// ============================================================
// Forward declarations
// ============================================================
static DWORD RvaToFileOffset(const std::vector<uint8_t>& data, DWORD rva);

// ============================================================
// Get a module's base address inside the target process
// ============================================================
static DWORD GetRemoteModuleBase(DWORD pid, const char* moduleName)
{
    wchar_t wide[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, moduleName, -1, wide, MAX_PATH);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W me = { sizeof(me) };
    DWORD base = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, wide) == 0) {
                base = (DWORD)(uintptr_t)me.modBaseAddr;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

// ============================================================
// Manual Map
// ============================================================
static bool ManualMap(HANDLE hProc, const std::vector<uint8_t>& dllData)
{
    if (dllData.size() < sizeof(MY_NT_HEADERS32)) {
        Log("[-] DLL too small\n");
        return false;
    }

    auto* dos = (IMAGE_DOS_HEADER*)dllData.data();
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        Log("[-] Invalid DOS header\n");
        return false;
    }
    auto* nt = (MY_NT_HEADERS32*)(dllData.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        Log("[-] Invalid NT signature\n");
        return false;
    }
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        Log("[-] DLL is not 32-bit (x86)\n");
        return false;
    }

    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    DWORD headerSize = nt->OptionalHeader.SizeOfHeaders;
    uint32_t preferredBase = nt->OptionalHeader.ImageBase;

    Log("[*] Image size: %u bytes, preferred base: 0x%X\n", (unsigned)imageSize, preferredBase);

    // Allocate memory in target process
    // Try preferred base first, then let OS choose
    LPVOID remoteImage = VirtualAllocEx(hProc, (LPVOID)(uintptr_t)preferredBase,
        imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteImage) {
        remoteImage = VirtualAllocEx(hProc, nullptr, imageSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    if (!remoteImage) {
        Log("[-] VirtualAllocEx failed: %d\n", (int)GetLastError());
        return false;
    }
    Log("[+] Allocated at 0x%p\n", remoteImage);

    uint32_t baseAddr = (uint32_t)(uintptr_t)remoteImage;
    uint32_t delta = baseAddr - preferredBase;

    // Write headers
    if (!WriteProcessMemory(hProc, remoteImage, dllData.data(), headerSize, nullptr)) {
        Log("[-] Failed to write headers\n");
        return false;
    }

    // Write sections
    auto* sect = (MY_SECTION_HEADER*)((BYTE*)&nt->OptionalHeader +
        nt->FileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sect[i].SizeOfRawData == 0) continue;
        LPVOID dest = (LPVOID)(uintptr_t)(baseAddr + sect[i].VirtualAddress);
        if (!WriteProcessMemory(hProc, dest,
            dllData.data() + sect[i].PointerToRawData,
            sect[i].SizeOfRawData, nullptr)) {
            Log("[-] Failed to write section %d\n", i);
            return false;
        }
    }
    Log("[+] Sections written\n");

    // Apply base relocations
    auto& relocDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir.Size > 0 && delta != 0) {
        DWORD relocFileOff = RvaToFileOffset(dllData, relocDir.VirtualAddress);
        auto* relocBlock = (IMAGE_BASE_RELOCATION*)(dllData.data() + relocFileOff);
        DWORD processed = 0;
        while (processed < relocDir.Size && relocBlock->SizeOfBlock > 0) {
            DWORD count = (relocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            WORD* entries = (WORD*)((BYTE*)relocBlock + sizeof(IMAGE_BASE_RELOCATION));
            for (DWORD j = 0; j < count; j++) {
                int type = entries[j] >> 12;
                int offset = entries[j] & 0xFFF;
                if (type == IMAGE_REL_BASED_HIGHLOW) {
                    uint32_t* patch = (uint32_t*)(uintptr_t)(baseAddr + relocBlock->VirtualAddress + offset);
                    uint32_t orig = 0;
                    // Read from our local copy (relocations already in section data)
                    orig = *(uint32_t*)(dllData.data() +
                        RvaToFileOffset(dllData, relocBlock->VirtualAddress + offset));
                    orig += delta;
                    WriteProcessMemory(hProc, patch, &orig, sizeof(uint32_t), nullptr);
                }
            }
            processed += relocBlock->SizeOfBlock;
            relocBlock = (IMAGE_BASE_RELOCATION*)((BYTE*)relocBlock + relocBlock->SizeOfBlock);
        }
        Log("[+] Relocations applied (delta=0x%X)\n", delta);
    }

    // Resolve imports
    auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.Size > 0) {
        DWORD pid = GetProcessId(hProc);
        DWORD importFileOff = RvaToFileOffset(dllData, importDir.VirtualAddress);
        auto* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)(dllData.data() + importFileOff);
        while (importDesc->Name != 0) {
            const char* dllName = (const char*)(dllData.data() +
                RvaToFileOffset(dllData, importDesc->Name));

            // Load locally so we can call GetProcAddress
            HMODULE hLocal = GetModuleHandleA(dllName);
            if (!hLocal) hLocal = LoadLibraryA(dllName);
            if (!hLocal) {
                Log("[!] Cannot load dependency: %s\n", dllName);
                importDesc++;
                continue;
            }

            // Find the same DLL's base address inside the target process
            DWORD remoteBase = GetRemoteModuleBase(pid, dllName);
            if (!remoteBase) {
                Log("[!] Not loaded in target: %s\n", dllName);
                importDesc++;
                continue;
            }

            DWORD localBase = (DWORD)(uintptr_t)hLocal;

            DWORD thunkFileOff = RvaToFileOffset(dllData, importDesc->FirstThunk);
            DWORD origThunkFileOff = importDesc->OriginalFirstThunk ?
                RvaToFileOffset(dllData, importDesc->OriginalFirstThunk) : thunkFileOff;
            auto* origThunk = (IMAGE_THUNK_DATA32*)(dllData.data() + origThunkFileOff);

            int idx = 0;
            while (origThunk[idx].u1.AddressOfData != 0) {
                uint32_t funcAddr = 0;
                if (origThunk[idx].u1.Ordinal & IMAGE_ORDINAL_FLAG32) {
                    DWORD ordinal = IMAGE_ORDINAL32(origThunk[idx].u1.Ordinal);
                    FARPROC lp = GetProcAddress(hLocal, (LPCSTR)(uintptr_t)ordinal);
                    if (lp) funcAddr = remoteBase + ((DWORD)(uintptr_t)lp - localBase);
                } else {
                    auto* importName = (IMAGE_IMPORT_BY_NAME*)(dllData.data() +
                        RvaToFileOffset(dllData, origThunk[idx].u1.AddressOfData));
                    FARPROC lp = GetProcAddress(hLocal, importName->Name);
                    if (lp) funcAddr = remoteBase + ((DWORD)(uintptr_t)lp - localBase);
                }
                uint32_t* patch = (uint32_t*)(uintptr_t)(baseAddr + importDesc->FirstThunk + idx * 4);
                WriteProcessMemory(hProc, patch, &funcAddr, sizeof(uint32_t), nullptr);
                idx++;
            }
            Log("[+] Resolved %s (%d imports)\n", dllName, idx);
            importDesc++;
        }
        Log("[+] Imports resolved\n");
    }

    // Get DllMain address
    uint32_t entryRva = nt->OptionalHeader.AddressOfEntryPoint;
    uint32_t dllMainAddr = baseAddr + entryRva;

    // Write shellcode
    uint8_t sc[sizeof(g_shellcode)];
    memcpy(sc, g_shellcode, sizeof(sc));
    *(uint32_t*)(sc + 5) = baseAddr;        // push hModule
    *(uint32_t*)(sc + 10) = dllMainAddr;    // mov eax, pDllMain

    LPVOID remoteSc = VirtualAllocEx(hProc, nullptr, sizeof(sc),
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteSc) {
        Log("[-] Failed to allocate shellcode memory\n");
        return false;
    }
    WriteProcessMemory(hProc, remoteSc, sc, sizeof(sc), nullptr);

    Log("[*] DllMain at 0x%X, shellcode at 0x%p\n", dllMainAddr, remoteSc);

    // Execute shellcode
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)remoteSc, nullptr, 0, nullptr);
    if (!hThread) {
        Log("[-] CreateRemoteThread failed: %d\n", (int)GetLastError());
        return false;
    }
    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    Log("[+] DllMain returned: %d\n", (int)exitCode);

    // Free shellcode
    VirtualFreeEx(hProc, remoteSc, 0, MEM_RELEASE);

    return true;
}

// Helper: convert RVA to file offset
static DWORD RvaToFileOffset(const std::vector<uint8_t>& data, DWORD rva)
{
    auto* dos = (IMAGE_DOS_HEADER*)data.data();
    auto* nt = (MY_NT_HEADERS32*)(data.data() + dos->e_lfanew);
    auto* sect = (MY_SECTION_HEADER*)((BYTE*)&nt->OptionalHeader +
        nt->FileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (rva >= sect[i].VirtualAddress &&
            rva < sect[i].VirtualAddress + sect[i].SizeOfRawData) {
            return sect[i].PointerToRawData + (rva - sect[i].VirtualAddress);
        }
    }
    return rva; // fallback
}

// ============================================================
// Main
// ============================================================
int wmain(int argc, wchar_t* argv[])
{
    // Open log file next to the exe
    wchar_t logPath[MAX_PATH];
    GetModuleFileNameW(nullptr, logPath, MAX_PATH);
    wchar_t* slash = wcsrchr(logPath, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat_s(logPath, L"injector_log.txt");
    g_log = _wfopen(logPath, L"w");
    if (g_log) Log("[*] Logging to: %ls\n", logPath);

    const wchar_t* dllPath = L"D:\\pbct\\PBCheat.dll";
    const wchar_t* procName = L"PointBlank.exe";

    if (argc > 1) dllPath = argv[1];
    if (argc > 2) procName = argv[2];

    Log("=== PBInjector - Point Blank ID Manual Map Injector ===\n");
    Log("[*] DLL: %ls\n", dllPath);
    Log("[*] Target: %ls\n", procName);

    int result = 0;
    DWORD fileSize = 0;
    std::vector<uint8_t> dllData;
    DWORD pid = 0;
    HANDLE hProc = nullptr;

    // Read DLL file
    HANDLE hFile = CreateFileW(dllPath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        Log("[-] Cannot open DLL file\n");
        result = 1;
        goto done;
    }
    fileSize = GetFileSize(hFile, nullptr);
    dllData.resize(fileSize);
    {
        DWORD bytesRead;
        ReadFile(hFile, dllData.data(), fileSize, &bytesRead, nullptr);
    }
    CloseHandle(hFile);
    Log("[+] DLL loaded: %u bytes\n", (unsigned)fileSize);

    // Find target process
    pid = FindProcess(procName);
    if (!pid) {
        Log("[-] Process '%ls' not found. Is the game running?\n", procName);
        result = 1;
        goto done;
    }
    Log("[+] Found PID: %u\n", (unsigned)pid);

    // Open process
    hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        Log("[-] OpenProcess failed: %d\n", (int)GetLastError());
        Log("    Try running as Administrator\n");
        result = 1;
        goto done;
    }

    // Manual map
    {
        bool ok = ManualMap(hProc, dllData);
        CloseHandle(hProc);
        hProc = nullptr;

        if (ok) {
            Log("\n[+] INJECTION SUCCESSFUL\n");
            Log("    Hotkeys:\n");
            Log("      INSERT - toggle mod menu\n");
            Log("      END    - panic (disable ESP + hide menu)\n");
        } else {
            Log("\n[-] INJECTION FAILED\n");
            result = 1;
        }
    }

done:
    Log("\nPress any key to exit...\n");
    system("pause");
    if (g_log) { fclose(g_log); g_log = nullptr; }
    return result;
}