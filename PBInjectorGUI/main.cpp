// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
// ============================================================
// PBInjectorGUI v3 - PbletZ-style layout
// sidebar nav + stat cards + control panel + log panel
// Replika inject_v2.ps1 (LoadLibraryA remote thread) dalam exe.
// Target: i686 (32-bit) - PB itu x86, injector HARUS 32-bit.
// Build: lihat build.bat section PBInjectorGUI.
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <cwctype>
#include <string>
#include <vector>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// ---------------- config ----------------
// Paths resolved at startup relative to this executable's location,
// so the repo works from any folder it is extracted to.
static std::wstring DLL_PATH_W;
static std::wstring LOG_DIR_W;
static std::wstring LOG_PATH_W;
static std::wstring EXE_DIR_W;
static std::wstring PB_EXE_W   = L"pointblank.exe";

static void ResolvePaths() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wchar_t* slash = wcsrchr(exe, L'\\');
    if (slash) *(slash + 1) = L'\0';
    EXE_DIR_W = exe;

    DLL_PATH_W  = EXE_DIR_W + L"decrypted_payload_cracked.dll";
    LOG_DIR_W   = EXE_DIR_W + L"logs";
    LOG_PATH_W  = LOG_DIR_W + L"\\inject_out.txt";
}

// ---------------- ids ----------------
#define IDC_LOG 1002
#define WM_APP_LOG  (WM_APP + 1)
#define WM_APP_DONE (WM_APP + 2)
#define WM_APP_AUTO (WM_APP + 3)

// ---------------- palette (PbletZ-ish dark) ----------------
static const COLORREF C_BG       = RGB(0x0A, 0x0E, 0x14);
static const COLORREF C_SIDE     = RGB(0x0C, 0x11, 0x18);
static const COLORREF C_SIDE_BD  = RGB(0x1B, 0x23, 0x30);
static const COLORREF C_CARD     = RGB(0x14, 0x1A, 0x24);
static const COLORREF C_CARD_BD  = RGB(0x22, 0x2D, 0x3D);
static const COLORREF C_TEXT     = RGB(0xE6, 0xED, 0xF3);
static const COLORREF C_DIM      = RGB(0x8B, 0x94, 0x9E);
static const COLORREF C_GREEN    = RGB(0x3F, 0xB9, 0x50);
static const COLORREF C_RED      = RGB(0xF8, 0x51, 0x49);
static const COLORREF C_AMBER    = RGB(0xD2, 0x99, 0x22);
static const COLORREF C_ACCENT   = RGB(0x2E, 0xA0, 0x43);
static const COLORREF C_ACCENT_H = RGB(0x3F, 0xB9, 0x50);
static const COLORREF C_ACCENT_P = RGB(0x24, 0x7E, 0x36);
static const COLORREF C_LOG_BG   = RGB(0x08, 0x0B, 0x10);
static const COLORREF C_LOG_TX   = RGB(0xB8, 0xC2, 0xCC);
static const COLORREF C_BTN_BD   = RGB(0x2A, 0x36, 0x48);

// ---------------- globals ----------------
static HINSTANCE g_hInst    = NULL;
static HWND      g_hMain    = NULL;
static HWND      g_hLog     = NULL;
static HFONT     g_fontApp  = NULL;
static HFONT     g_fontNav  = NULL;
static HFONT     g_fontLbl  = NULL;
static HFONT     g_fontVal  = NULL;
static HFONT     g_fontBtn  = NULL;
static HFONT     g_fontPill = NULL;
static HFONT     g_fontDesc = NULL;
static HFONT     g_fontMono = NULL;
static HFONT     g_fontGlyph= NULL;
static HFONT     g_fontGlyphBig = NULL;
static HFONT     g_fontCredit = NULL;
static HBRUSH    g_brBg     = NULL;
static HBRUSH    g_brSide   = NULL;
static HBRUSH    g_brSideBd = NULL;
static HBRUSH    g_brLog    = NULL;
static HANDLE    g_hLogFile = INVALID_HANDLE_VALUE;
static bool      g_busy     = false;
static bool      g_admin    = false;
static bool      g_dllOk    = false;
static bool      g_trkLea   = false;
static UINT      g_dpi      = 96;
static double    g_scale    = 1.0;

// hover / press / active state
static int  g_hoverNav  = -1;
static int  g_hoverAct  = 0;   // 1=inject 2=patch 3=logs 4=clear
static int  g_pressAct  = 0;
static int  g_activeNav = 0;

// layout rects
static RECT g_rcCard[3];
static RECT g_rcPanel;
static RECT g_rcLogPanel;
static RECT g_rcInject;
static RECT g_rcPatch;
static RECT g_rcLogs;
static RECT g_rcClear;
static RECT g_rcPill;
static RECT g_rcDesc;
static RECT g_rcNav[4];
static RECT g_rcLog;

struct CardState {
    std::wstring label;
    std::wstring value;
    COLORREF color;
};
static CardState g_cardPB  = { L"POINTBLANK",   L"cek...", C_DIM };
static CardState g_cardAdm = { L"ADMIN",        L"...",    C_DIM };
static CardState g_cardDll = { L"PAYLOAD DLL",  L"...",    C_DIM };

static std::wstring g_pillText = L"CHECKING";
static COLORREF     g_pillCol  = C_DIM;
static COLORREF     g_pillBg   = RGB(0x1A, 0x20, 0x2A);

static const wchar_t* NAV_LABELS[4] = { L"Inject", L"Patch DLL", L"Logs", L"About" };
static const wchar_t* NAV_GLYPHS[4] = { L"\u26A1", L"\u2699", L"\u2630", L"\u2139" };
static const wchar_t* CARD_GLYPHS[3]= { L"\u25B6", L"\u2605", L"\u2630" };

static int S(int v) { return (int)(v * g_scale + 0.5); }

// ============================================================
// logging
// ============================================================

static std::wstring NowStamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t b[40];
    swprintf(b, 40, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return std::wstring(b);
}

static void AppendLogLine(const std::wstring& msg) {
    std::wstring line = NowStamp() + msg + L"\r\n";
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());

    if (g_hLogFile != INVALID_HANDLE_VALUE) {
        char buf[1200];
        int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, buf, (int)sizeof(buf), NULL, NULL);
        if (n <= 0) {
            std::wstring cut = line.substr(0, 400);
            n = WideCharToMultiByte(CP_UTF8, 0, cut.c_str(), -1, buf, (int)sizeof(buf), NULL, NULL);
        }
        if (n > 1) {
            DWORD w = 0;
            WriteFile(g_hLogFile, buf, (DWORD)(n - 1), &w, NULL);
            FlushFileBuffers(g_hLogFile);
        }
    }
}

struct LogMsg { std::wstring str; };

static void PostLog(const std::wstring& msg) {
    LogMsg* m = new LogMsg();
    m->str = msg;
    PostMessageW(g_hMain, WM_APP_LOG, 0, (LPARAM)m);
}

static void LogLineUI(const std::wstring& msg) {
    AppendLogLine(msg);
}

// ============================================================
// elevation
// ============================================================

static bool IsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

static void ElevateSelf() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.lpParameters = L"/auto";
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOASYNC;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            LogLineUI(L"FATAL: elevasi gagal (UAC ditolak) - klik YES pas dialog muncul");
        } else {
            LogLineUI(L"FATAL: elevasi gagal err=" + std::to_wstring((unsigned long)err));
        }
        g_busy = false;
        InvalidateRect(g_hMain, NULL, FALSE);
        return;
    }
    LogLineUI(L"Relaunch elevated OK - window ini tutup, window baru auto-inject.");
    PostQuitMessage(0);
}

// ============================================================
// process / module helpers
// ============================================================

struct PbProc {
    DWORD pid;
    std::wstring title;
    bool hasWindow;
};

static BOOL CALLBACK EnumWinProc(HWND hwnd, LPARAM lp) {
    std::vector<PbProc>* procs = (std::vector<PbProc>*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    for (size_t i = 0; i < procs->size(); i++) {
        if ((*procs)[i].pid == pid) {
            if (IsWindowVisible(hwnd)) {
                int n = GetWindowTextLengthW(hwnd);
                if (n > 0) {
                    std::wstring t((size_t)n + 1, L'\0');
                    GetWindowTextW(hwnd, &t[0], n + 1);
                    t.resize((size_t)n);
                    (*procs)[i].hasWindow = true;
                    if (t.size() > (*procs)[i].title.size()) (*procs)[i].title = t;
                }
            }
            break;
        }
    }
    return TRUE;
}

static void FindPB(std::vector<PbProc>& out) {
    out.clear();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring exe = pe.szExeFile;
            for (size_t i = 0; i < exe.size(); i++) exe[i] = towlower(exe[i]);
            if (exe == PB_EXE_W) {
                PbProc p;
                p.pid = pe.th32ProcessID;
                p.hasWindow = false;
                out.push_back(p);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!out.empty()) EnumWindows(EnumWinProc, (LPARAM)&out);
}

static bool IsNumericDll(const std::wstring& lower) {
    if (lower.size() < 5) return false;
    if (lower.compare(lower.size() - 4, 4, L".dll") != 0) return false;
    for (size_t i = 0; i + 4 < lower.size(); i++)
        if (!iswdigit(lower[i])) return false;
    return true;
}

static bool ModuleScan(DWORD pid, bool numericOnly, std::wstring& foundOut) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            std::wstring lower = me.szModule;
            for (size_t i = 0; i < lower.size(); i++) lower[i] = towlower(lower[i]);
            bool numeric = IsNumericDll(lower);
            bool payload = lower.find(L"decrypted_payload") != std::wstring::npos;
            bool sizeHit = (me.modBaseSize == 851968);
            bool hit = numericOnly ? numeric : (numeric || payload || sizeHit);
            if (hit) {
                wchar_t b[300];
                swprintf(b, 300, L"%s @ 0x%08X size=%lu",
                         me.szModule,
                         (unsigned long)(uintptr_t)me.modBaseAddr,
                         (unsigned long)me.modBaseSize);
                foundOut = b;
                found = true;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

// ============================================================
// injection core — stealth manual map
// Ported from PBInjector (manual map) + PBCheat\bypass.cpp
// (CB killer, PEB clear). LoadLibraryA kept only as fallback.
// ============================================================

// ---- PE structures (32-bit) ----
typedef struct {
    WORD  Machine;
    WORD  NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD  SizeOfOptionalHeader;
    WORD  Characteristics;
} MM_FILE_HEADER;

typedef struct {
    DWORD VirtualAddress;
    DWORD Size;
} MM_DATA_DIRECTORY;

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
    MM_DATA_DIRECTORY DataDirectory[16];
} MM_OPTIONAL_HEADER32;

typedef struct {
    DWORD Signature;
    MM_FILE_HEADER FileHeader;
    MM_OPTIONAL_HEADER32 OptionalHeader;
} MM_NT_HEADERS32;

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
} MM_SECTION_HEADER;

// NtQueryInformationProcess -> ProcessBasicInformation
typedef struct {
    LONG  ExitStatus;
    PVOID PebBaseAddress;
    ULONG AffinityMask;
    LONG  BasePriority;
    ULONG UniqueProcessId;
    ULONG InheritedFromUniqueProcessId;
} MM_PBI;

typedef LONG (WINAPI* fnNtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);

// Shellcode: call DllMain(hModule, DLL_PROCESS_ATTACH, NULL)
static const uint8_t MM_SHELLCODE[] = {
    0x6A, 0x00,                         // push 0
    0x6A, 0x01,                         // push 1
    0x68, 0x00, 0x00, 0x00, 0x00,       // push hModule (patched)
    0xB8, 0x00, 0x00, 0x00, 0x00,       // mov eax, pDllMain (patched)
    0xFF, 0xD0,                         // call eax
    0xC3                                // ret
};

static DWORD MmRvaToFileOffset(const std::vector<uint8_t>& d, DWORD rva) {
    auto* dos = (IMAGE_DOS_HEADER*)d.data();
    auto* nt  = (MM_NT_HEADERS32*)(d.data() + dos->e_lfanew);
    auto* sect = (MM_SECTION_HEADER*)((BYTE*)&nt->OptionalHeader +
        nt->FileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (rva >= sect[i].VirtualAddress &&
            rva < sect[i].VirtualAddress + sect[i].SizeOfRawData) {
            return sect[i].PointerToRawData + (rva - sect[i].VirtualAddress);
        }
    }
    return rva;
}

static DWORD MmRemoteModuleBase(DWORD pid, const char* name) {
    wchar_t wide[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, name, -1, wide, MAX_PATH);
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

static DWORD MmFindProcess(const wchar_t* name) {
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

static void MmTerminate(DWORD pid) {
    if (!pid) return;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (h) { TerminateProcess(h, 0); CloseHandle(h); }
}

// Kill CB.exe + CBConfig.exe before injecting so the AC is not
// alive to scan the freshly mapped image.
static int KillCheatBlocker() {
    int killed = 0;
    const wchar_t* names[] = { L"CB.exe", L"CBConfig.exe" };
    for (int i = 0; i < 2; i++) {
        for (int guard = 0; guard < 8; guard++) {
            DWORD pid = MmFindProcess(names[i]);
            if (!pid) break;
            MmTerminate(pid);
            killed++;
            Sleep(60);
        }
    }
    return killed;
}

// Clear BeingDebugged + NtGlobalFlag inside the TARGET process PEB.
static bool PatchRemotePEB(HANDLE hProc) {
    HMODULE hNt = GetModuleHandleA("ntdll.dll");
    if (!hNt) return false;
    fnNtQIP NtQIP = (fnNtQIP)GetProcAddress(hNt, "NtQueryInformationProcess");
    if (!NtQIP) return false;

    MM_PBI pbi;
    ULONG got = 0;
    if (NtQIP(hProc, 0, &pbi, sizeof(pbi), &got) != 0) return false;
    if (!pbi.PebBaseAddress) return false;

    BYTE* peb = (BYTE*)pbi.PebBaseAddress;
    SIZE_T w = 0;
    BYTE  b0 = 0;
    DWORD d0 = 0;
    WriteProcessMemory(hProc, peb + 2, &b0, 1, &w);      // BeingDebugged
    WriteProcessMemory(hProc, peb + 0x68, &d0, 4, &w);   // NtGlobalFlag
    return true;
}

static bool ReadDllBytes(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(f, nullptr);
    if (sz == INVALID_FILE_SIZE || sz == 0) { CloseHandle(f); return false; }
    out.resize(sz);
    DWORD got = 0;
    BOOL ok = ReadFile(f, out.data(), sz, &got, nullptr);
    CloseHandle(f);
    return ok && got == sz;
}

// Manual map with per-section protections (no single RWX blob) so
// memory scanners do not flag a private executable region.
static bool ManualMapStealth(HANDLE hProc, const std::vector<uint8_t>& dll,
                             DWORD& outBase, DWORD& outHeaderSize) {
    outBase = 0;
    outHeaderSize = 0;
    if (dll.size() < sizeof(MM_NT_HEADERS32)) return false;

    auto* dos = (IMAGE_DOS_HEADER*)dll.data();
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        PostLog(L"  [-] DOS header invalid");
        return false;
    }
    auto* nt = (MM_NT_HEADERS32*)(dll.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        PostLog(L"  [-] NT signature invalid");
        return false;
    }
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        PostLog(L"  [-] DLL bukan 32-bit (x86)");
        return false;
    }

    DWORD imageSize  = nt->OptionalHeader.SizeOfImage;
    DWORD headerSize = nt->OptionalHeader.SizeOfHeaders;
    uint32_t preferredBase = nt->OptionalHeader.ImageBase;

    {
        wchar_t b[160];
        swprintf(b, 160, L"  [*] image=%u bytes, headers=%u, prefBase=0x%08X",
                 (unsigned)imageSize, (unsigned)headerSize, (unsigned)preferredBase);
        PostLog(b);
    }

    // Allocate READ/WRITE first — never leave one big RWX region behind.
    LPVOID remoteImage = VirtualAllocEx(hProc, (LPVOID)(uintptr_t)preferredBase,
        imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteImage) {
        remoteImage = VirtualAllocEx(hProc, nullptr, imageSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    if (!remoteImage) {
        wchar_t b[128];
        swprintf(b, 128, L"  [-] VirtualAllocEx GAGAL err=%lu", (unsigned long)GetLastError());
        PostLog(b);
        return false;
    }

    uint32_t baseAddr = (uint32_t)(uintptr_t)remoteImage;
    uint32_t delta = baseAddr - preferredBase;
    {
        wchar_t b[128];
        swprintf(b, 128, L"  [+] allocated @ 0x%08X (delta=0x%08X)", (unsigned)baseAddr, (unsigned)delta);
        PostLog(b);
    }

    if (!WriteProcessMemory(hProc, remoteImage, dll.data(), headerSize, nullptr)) {
        PostLog(L"  [-] gagal tulis headers");
        return false;
    }

    auto* sect = (MM_SECTION_HEADER*)((BYTE*)&nt->OptionalHeader +
        nt->FileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sect[i].SizeOfRawData == 0) continue;
        if (!WriteProcessMemory(hProc, (LPVOID)(uintptr_t)(baseAddr + sect[i].VirtualAddress),
                dll.data() + sect[i].PointerToRawData, sect[i].SizeOfRawData, nullptr)) {
            wchar_t b[128];
            swprintf(b, 128, L"  [-] gagal tulis section %d", i);
            PostLog(b);
            return false;
        }
    }
    PostLog(L"  [+] sections ditulis");

    // base relocations
    auto& relocDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir.Size > 0 && delta != 0) {
        DWORD relocFileOff = MmRvaToFileOffset(dll, relocDir.VirtualAddress);
        auto* relocBlock = (IMAGE_BASE_RELOCATION*)(dll.data() + relocFileOff);
        DWORD processed = 0;
        int applied = 0;
        while (processed < relocDir.Size && relocBlock->SizeOfBlock > 0) {
            DWORD count = (relocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            WORD* entries = (WORD*)((BYTE*)relocBlock + sizeof(IMAGE_BASE_RELOCATION));
            for (DWORD j = 0; j < count; j++) {
                int type = entries[j] >> 12;
                int offset = entries[j] & 0xFFF;
                if (type == IMAGE_REL_BASED_HIGHLOW) {
                    uint32_t* patch = (uint32_t*)(uintptr_t)(baseAddr + relocBlock->VirtualAddress + offset);
                    uint32_t orig = *(uint32_t*)(dll.data() +
                        MmRvaToFileOffset(dll, relocBlock->VirtualAddress + offset));
                    orig += delta;
                    WriteProcessMemory(hProc, patch, &orig, sizeof(uint32_t), nullptr);
                    applied++;
                }
            }
            processed += relocBlock->SizeOfBlock;
            relocBlock = (IMAGE_BASE_RELOCATION*)((BYTE*)relocBlock + relocBlock->SizeOfBlock);
        }
        wchar_t b[128];
        swprintf(b, 128, L"  [+] relocations applied (%d)", applied);
        PostLog(b);
    }

    // imports
    auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.Size > 0) {
        DWORD pid = GetProcessId(hProc);
        DWORD importFileOff = MmRvaToFileOffset(dll, importDir.VirtualAddress);
        auto* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)(dll.data() + importFileOff);
        while (importDesc->Name != 0) {
            const char* dllName = (const char*)(dll.data() +
                MmRvaToFileOffset(dll, importDesc->Name));
            HMODULE hLocal = GetModuleHandleA(dllName);
            if (!hLocal) hLocal = LoadLibraryA(dllName);
            if (!hLocal) {
                wchar_t b[160];
                swprintf(b, 160, L"  [!] dependency gak ke-load: %S", dllName);
                PostLog(b);
                importDesc++;
                continue;
            }
            DWORD remoteBase = MmRemoteModuleBase(pid, dllName);
            if (!remoteBase) {
                wchar_t b[160];
                swprintf(b, 160, L"  [!] gak ke-load di target: %S", dllName);
                PostLog(b);
                importDesc++;
                continue;
            }
            DWORD localBase = (DWORD)(uintptr_t)hLocal;
            DWORD thunkFileOff = MmRvaToFileOffset(dll, importDesc->FirstThunk);
            DWORD origThunkFileOff = importDesc->OriginalFirstThunk ?
                MmRvaToFileOffset(dll, importDesc->OriginalFirstThunk) : thunkFileOff;
            auto* origThunk = (IMAGE_THUNK_DATA32*)(dll.data() + origThunkFileOff);
            int idx = 0;
            while (origThunk[idx].u1.AddressOfData != 0) {
                uint32_t funcAddr = 0;
                if (origThunk[idx].u1.Ordinal & IMAGE_ORDINAL_FLAG32) {
                    DWORD ordinal = IMAGE_ORDINAL32(origThunk[idx].u1.Ordinal);
                    FARPROC lp = GetProcAddress(hLocal, (LPCSTR)(uintptr_t)ordinal);
                    if (lp) funcAddr = remoteBase + ((DWORD)(uintptr_t)lp - localBase);
                } else {
                    auto* importName = (IMAGE_IMPORT_BY_NAME*)(dll.data() +
                        MmRvaToFileOffset(dll, origThunk[idx].u1.AddressOfData));
                    FARPROC lp = GetProcAddress(hLocal, importName->Name);
                    if (lp) funcAddr = remoteBase + ((DWORD)(uintptr_t)lp - localBase);
                }
                uint32_t* patch = (uint32_t*)(uintptr_t)(baseAddr + importDesc->FirstThunk + idx * 4);
                WriteProcessMemory(hProc, patch, &funcAddr, sizeof(uint32_t), nullptr);
                idx++;
            }
            importDesc++;
        }
        PostLog(L"  [+] imports resolved");
    }

    // per-section protections — kill the RWX signature
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD ch = sect[i].Characteristics;
        bool r = (ch & IMAGE_SCN_MEM_READ) != 0;
        bool w = (ch & IMAGE_SCN_MEM_WRITE) != 0;
        bool x = (ch & IMAGE_SCN_MEM_EXECUTE) != 0;
        DWORD prot;
        if (x && w)      prot = PAGE_EXECUTE_READWRITE;
        else if (x && r) prot = PAGE_EXECUTE_READ;
        else if (x)      prot = PAGE_EXECUTE;
        else if (w)      prot = PAGE_READWRITE;
        else if (r)      prot = PAGE_READONLY;
        else             prot = PAGE_NOACCESS;
        DWORD sz = sect[i].SizeOfRawData ? sect[i].SizeOfRawData : sect[i].VirtualSize;
        DWORD old = 0;
        VirtualProtectEx(hProc, (LPVOID)(uintptr_t)(baseAddr + sect[i].VirtualAddress),
                         sz, prot, &old);
    }
    PostLog(L"  [+] section protections applied (no RWX blob)");

    uint32_t entryRva = nt->OptionalHeader.AddressOfEntryPoint;
    uint32_t dllMainAddr = baseAddr + entryRva;

    uint8_t sc[sizeof(MM_SHELLCODE)];
    memcpy(sc, MM_SHELLCODE, sizeof(sc));
    *(uint32_t*)(sc + 5)  = baseAddr;
    *(uint32_t*)(sc + 10) = dllMainAddr;

    LPVOID remoteSc = VirtualAllocEx(hProc, nullptr, sizeof(sc),
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteSc) {
        PostLog(L"  [-] gagal alloc shellcode");
        return false;
    }
    WriteProcessMemory(hProc, remoteSc, sc, sizeof(sc), nullptr);

    {
        wchar_t b[128];
        swprintf(b, 128, L"  [*] DllMain=0x%08X shellcode=0x%08X",
                 (unsigned)dllMainAddr, (unsigned)(uintptr_t)remoteSc);
        PostLog(b);
    }

    HANDLE th = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)remoteSc, nullptr, 0, nullptr);
    if (!th) {
        wchar_t b[128];
        swprintf(b, 128, L"  [-] CreateRemoteThread GAGAL err=%lu", (unsigned long)GetLastError());
        PostLog(b);
        VirtualFreeEx(hProc, remoteSc, 0, MEM_RELEASE);
        return false;
    }
    WaitForSingleObject(th, 15000);
    DWORD ec = 0;
    GetExitCodeThread(th, &ec);
    CloseHandle(th);
    VirtualFreeEx(hProc, remoteSc, 0, MEM_RELEASE);

    {
        wchar_t b[128];
        swprintf(b, 128, L"  [+] DllMain returned %d", (int)ec);
        PostLog(b);
    }

    outBase = baseAddr;
    outHeaderSize = headerSize;
    return true;
}

// Wipe the MZ + PE signatures in the mapped image so memory scans
// looking for PE headers in private regions come up empty.
static void EraseRemoteHeaders(HANDLE hProc, DWORD base, DWORD headerSize) {
    if (!base || !headerSize) return;
    DWORD wipe = headerSize < 0x400 ? headerSize : 0x400;
    std::vector<uint8_t> zeros(wipe, 0);
    if (WriteProcessMemory(hProc, (LPVOID)(uintptr_t)base, zeros.data(), wipe, nullptr)) {
        wchar_t b[128];
        swprintf(b, 128, L"  [+] PE headers wiped (%u bytes)", (unsigned)wipe);
        PostLog(b);
    }
}

// Fallback path — LoadLibraryA remote thread (detectable, only used
// when manual map fails).
static bool InjectLoadLibraryA(HANDLE h, DWORD pid) {
    char dllA[MAX_PATH];
    int n = WideCharToMultiByte(CP_ACP, 0, DLL_PATH_W.c_str(), -1, dllA, MAX_PATH, NULL, NULL);
    if (n <= 0) {
        PostLog(L"  path convert gagal");
        return false;
    }
    SIZE_T size = (SIZE_T)n;

    LPVOID mem = VirtualAllocEx(h, NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) {
        wchar_t b[128];
        swprintf(b, 128, L"  VirtualAllocEx GAGAL err=%lu", (unsigned long)GetLastError());
        PostLog(b);
        return false;
    }
    SIZE_T written = 0;
    if (!WriteProcessMemory(h, mem, dllA, size, &written)) {
        wchar_t b[128];
        swprintf(b, 128, L"  WriteProcessMemory GAGAL err=%lu", (unsigned long)GetLastError());
        PostLog(b);
        VirtualFreeEx(h, mem, 0, MEM_RELEASE);
        return false;
    }

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC lla = GetProcAddress(k32, "LoadLibraryA");
    if (!lla) {
        PostLog(L"  LoadLibraryA gak ketemu!");
        VirtualFreeEx(h, mem, 0, MEM_RELEASE);
        return false;
    }

    DWORD tid = 0;
    HANDLE th = CreateRemoteThread(h, NULL, 0, (LPTHREAD_START_ROUTINE)lla, mem, 0, &tid);
    if (!th) {
        wchar_t b[128];
        swprintf(b, 128, L"  CreateRemoteThread GAGAL err=%lu", (unsigned long)GetLastError());
        PostLog(b);
        VirtualFreeEx(h, mem, 0, MEM_RELEASE);
        return false;
    }
    DWORD wait = WaitForSingleObject(th, 30000);
    DWORD exitCode = 0;
    GetExitCodeThread(th, &exitCode);
    {
        wchar_t b[128];
        swprintf(b, 128, L"  wait=%lu exitcode=0x%08X", (unsigned long)wait, (unsigned long)exitCode);
        PostLog(b);
    }
    CloseHandle(th);
    VirtualFreeEx(h, mem, 0, MEM_RELEASE);

    if (exitCode == 0) {
        PostLog(L"  >>> LoadLibraryA RETURN 0 = DLL GAGAL LOAD");
    } else if (exitCode >= 0xC0000000) {
        PostLog(L"  >>> THREAD CRASH - kemungkinan injector 64-bit / address salah!");
    } else {
        wchar_t b[128];
        swprintf(b, 128, L"  >>> LoadLibraryA SUKSES hmodule=0x%08X", (unsigned long)exitCode);
        PostLog(b);
    }

    Sleep(800);
    std::wstring mod;
    if (ModuleScan(pid, true, mod)) {
        PostLog(L"  >>> MODULE LIST: " + mod);
    } else {
        PostLog(L"  >>> MODULE LIST: DLL TETEP GAK ADA");
    }
    return (exitCode != 0 && exitCode < 0xC0000000);
}

static bool InjectToPid(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) {
        wchar_t b[128];
        swprintf(b, 128, L"  OpenProcess DENIED err=%lu", (unsigned long)GetLastError());
        PostLog(b);
        return false;
    }
    PostLog(L"  OpenProcess(ALL) OK");

    std::wstring found;
    if (ModuleScan(pid, false, found)) {
        PostLog(L"  sudah ada: " + found);
        PostLog(L"  SKIP (cheat DLL udah ada di proses ini)");
        CloseHandle(h);
        return true;
    }

    std::vector<uint8_t> dll;
    if (!ReadDllBytes(DLL_PATH_W, dll)) {
        PostLog(L"  FATAL: DLL gak kebaca: " + DLL_PATH_W);
        CloseHandle(h);
        return false;
    }
    {
        wchar_t b[128];
        swprintf(b, 128, L"  DLL dibaca: %u bytes", (unsigned)dll.size());
        PostLog(b);
    }

    // 1. kill the anti-cheat before touching the target
    int killed = KillCheatBlocker();
    if (killed > 0) {
        wchar_t b[128];
        swprintf(b, 128, L"  [+] CB.exe/CBConfig.exe dimatiin (%d proses)", killed);
        PostLog(b);
    } else {
        PostLog(L"  [i] CB.exe gak jalan - lanjut");
    }

    // 2. clear BeingDebugged / NtGlobalFlag in the target
    if (PatchRemotePEB(h)) {
        PostLog(L"  [+] remote PEB patched (BeingDebugged + NtGlobalFlag = 0)");
    } else {
        PostLog(L"  [!] remote PEB patch gagal - lanjut");
    }

    // 3. stealth manual map
    DWORD base = 0, hdrSize = 0;
    bool ok = ManualMapStealth(h, dll, base, hdrSize);
    if (ok) {
        EraseRemoteHeaders(h, base, hdrSize);
        PostLog(L"  >>> MANUAL MAP SUKSES - DLL gak muncul di module list (by design)");
        PostLog(L"  >>> ESP/menu harusnya aktif dalam beberapa detik");
        CloseHandle(h);
        return true;
    }

    // 4. fallback
    PostLog(L"  [!] manual map gagal - fallback ke LoadLibraryA (kedeteksi)");
    bool fb = InjectLoadLibraryA(h, pid);
    CloseHandle(h);
    return fb;
}

static DWORD WINAPI InjectThread(LPVOID) {
    PostLog(L"=== INJECTOR GUI - " + DLL_PATH_W + L" -> PointBlank ===");
    PostLog(L"env: elevated=OK (GUI 32-bit)");

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(DLL_PATH_W.c_str(), GetFileExInfoStandard, &fad)) {
        PostLog(L"FATAL: DLL gak ada: " + DLL_PATH_W);
        PostMessageW(g_hMain, WM_APP_DONE, 1, 0);
        return 0;
    }
    ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    {
        wchar_t b[128];
        swprintf(b, 128, L"DLL: size=%llu", (unsigned long long)sz);
        PostLog(b);
    }

    std::vector<PbProc> procs;
    FindPB(procs);
    if (procs.empty()) {
        PostLog(L"PB gak jalan!");
        PostMessageW(g_hMain, WM_APP_DONE, 2, 0);
        return 0;
    }
    for (size_t i = 0; i < procs.size(); i++) {
        std::wstring line = L"  kandidat: pid=" + std::to_wstring((unsigned long)procs[i].pid);
        if (procs[i].hasWindow) line += L" window='" + procs[i].title + L"'";
        PostLog(line);
    }

    std::vector<DWORD> targets;
    bool anyWin = false;
    for (size_t i = 0; i < procs.size(); i++)
        if (procs[i].hasWindow) { targets.push_back(procs[i].pid); anyWin = true; }
    if (!anyWin) {
        PostLog(L"ga ada PB dgn window - inject ke SEMUA proses PB");
        for (size_t i = 0; i < procs.size(); i++) targets.push_back(procs[i].pid);
    }

    int ok = 0;
    for (size_t i = 0; i < targets.size(); i++) {
        std::wstring title;
        for (size_t j = 0; j < procs.size(); j++)
            if (procs[j].pid == targets[i]) title = procs[j].title;
        std::wstring line = L">>> inject ke pid=" + std::to_wstring((unsigned long)targets[i]);
        if (!title.empty()) line += L" window='" + title + L"'";
        PostLog(L"");
        PostLog(line);
        if (InjectToPid(targets[i])) ok++;
    }

    PostLog(L"");
    PostLog(L"SELESAI - " + std::to_wstring(ok) + L" proses ke-inject");
    PostMessageW(g_hMain, WM_APP_DONE, ok > 0 ? 0 : 3, 0);
    return 0;
}

static void UpdatePill() {
    if (g_busy) {
        g_pillText = L"INJECTING...";
        g_pillCol  = C_AMBER;
        g_pillBg   = RGB(0x2A, 0x22, 0x10);
        return;
    }
    std::vector<PbProc> procs;
    FindPB(procs);
    if (procs.empty()) {
        g_pillText = L"PB NOT RUNNING";
        g_pillCol  = C_RED;
        g_pillBg   = RGB(0x2E, 0x14, 0x14);
    } else {
        g_pillText = L"READY";
        g_pillCol  = C_GREEN;
        g_pillBg   = RGB(0x12, 0x2A, 0x18);
    }
}

static void StartInjection() {
    if (g_busy) return;
    if (!g_dllOk) {
        LogLineUI(L"WARNING: DLL file gak ketemu di " + EXE_DIR_W + L" - inject kemungkinan gagal");
    }
    if (!g_admin) {
        LogLineUI(L"belum admin - minta elevasi (UAC)...");
        ElevateSelf();
        return;
    }
    g_busy = true;
    UpdatePill();
    InvalidateRect(g_hMain, NULL, FALSE);
    CloseHandle(CreateThread(NULL, 0, InjectThread, NULL, 0, NULL));
}

// ============================================================
// actions (nav / secondary buttons)
// ============================================================

static void DoPatch() {
    LogLineUI(L">>> re-patch DLL (patch_dll_final.ps1)...");
    std::wstring args = L"-ExecutionPolicy Bypass -File \"" + EXE_DIR_W + L"patch_dll_final.ps1\"";
    ShellExecuteW(NULL, L"open", L"powershell.exe", args.c_str(), NULL, SW_SHOW);
    LogLineUI(L"    window powershell kebuka - liat hasilnya di sana");
}

static void DoLogs() {
    LogLineUI(L">>> buka folder logs...");
    ShellExecuteW(NULL, L"open", L"explorer.exe", LOG_DIR_W.c_str(), NULL, SW_SHOW);
}

static void DoAbout() {
    MessageBoxW(g_hMain,
                L"PB Sumatra - VIP Injector v1.0.0\n"
                L"\n"
                L"Permanent VIP bypass untuk Point Blank ID.\n"
                L"Inject decrypted_payload_cracked.dll ke proses PointBlank.\n"
                L"\n"
                L"----------------------------------------\n"
                L"CRACKED BY: dekkaaa9\n"
                L"GitHub    : https://github.com/dekkaaa9\n"
                L"----------------------------------------\n"
                L"\n"
                L"TOOLS:\n"
                L"  - REA + Ghidra        (reverse engineering)\n"
                L"  - PowerShell 5.1      (crack + patch scripts)\n"
                L"  - llvm-mingw i686     (build, 32-bit)\n"
                L"  - Win32 API           (GUI + injector)\n"
                L"  - DirectX 9           (ESP rendering)\n"
                L"\n"
                L"pb-loader-toolkit - this repo\n"
                L"PB_Sumatra         - crack chain\n"
                L"\n"
                L"Hotkey in-game: INSERT (menu), END (panic)",
                L"About - PB Sumatra", MB_OK | MB_ICONINFORMATION);
}

static void DoClear() {
    SetWindowTextW(g_hLog, L"");
}

// ============================================================
// paint helpers
// ============================================================

static HFONT MakeFont(int pt, int weight, bool mono) {
    if (mono) {
        return CreateFontW(-MulDiv(pt, (int)g_dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }
    return CreateFontW(-MulDiv(pt, (int)g_dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static HFONT MakeGlyphFont(int pt) {
    return CreateFontW(-MulDiv(pt, (int)g_dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
}

static COLORREF Blend(COLORREF a, COLORREF b, float t) {
    int r = (int)(GetRValue(a) * t + GetRValue(b) * (1.0f - t));
    int g = (int)(GetGValue(a) * t + GetGValue(b) * (1.0f - t));
    int c = (int)(GetBValue(a) * t + GetBValue(b) * (1.0f - t));
    return RGB(r, g, c);
}

static void DrawText2(HDC hdc, HFONT f, const std::wstring& s, RECT rc, COLORREF col, UINT fmt) {
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, s.c_str(), -1, &rc, fmt | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
}

static void DrawGlyph(HDC hdc, HFONT f, const wchar_t* g, RECT rc, COLORREF col) {
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, g, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
}

static void RR(HDC hdc, RECT rc, COLORREF fill, COLORREF border, int rad) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    HPEN op = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, rad, rad);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(br);
    DeleteObject(pen);
}

static void UpdatePbCard() {
    std::vector<PbProc> procs;
    FindPB(procs);
    if (procs.empty()) {
        g_cardPB.value = L"TIDAK JALAN";
        g_cardPB.color = C_RED;
    } else {
        std::wstring s = L"JALAN";
        for (size_t i = 0; i < procs.size(); i++) {
            s += (i == 0) ? L"   pid " : L", ";
            s += std::to_wstring((unsigned long)procs[i].pid);
            if (procs[i].hasWindow && !procs[i].title.empty()) {
                std::wstring t = procs[i].title;
                if (t.size() > 20) t = t.substr(0, 20) + L"...";
                s += L" '" + t + L"'";
            }
        }
        g_cardPB.value = s;
        g_cardPB.color = C_GREEN;
    }
}

static void Layout(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    int sbw = S(210);
    int pad = S(20);
    int cx = sbw + pad;
    int cw = rc.right - cx - pad;

    // stat cards
    int gap = S(14);
    int cardW = (cw - gap * 2) / 3;
    for (int i = 0; i < 3; i++) {
        g_rcCard[i].left = cx + i * (cardW + gap);
        g_rcCard[i].right = g_rcCard[i].left + cardW;
        g_rcCard[i].top = pad;
        g_rcCard[i].bottom = pad + S(78);
    }

    // control panel
    g_rcPanel.left = cx;
    g_rcPanel.right = cx + cw;
    g_rcPanel.top = g_rcCard[0].bottom + gap;
    g_rcPanel.bottom = g_rcPanel.top + S(196);

    // log panel
    g_rcLogPanel.left = cx;
    g_rcLogPanel.right = cx + cw;
    g_rcLogPanel.top = g_rcPanel.bottom + gap;
    g_rcLogPanel.bottom = rc.bottom - pad;

    // inject button
    g_rcInject.left = cx + pad;
    g_rcInject.right = cx + cw - pad;
    g_rcInject.top = g_rcPanel.top + S(128);
    g_rcInject.bottom = g_rcInject.top + S(52);

    // secondary buttons (top-right control panel)
    int bw = S(118), bh = S(26);
    g_rcLogs.right = g_rcPanel.right - pad;
    g_rcLogs.left = g_rcLogs.right - bw;
    g_rcLogs.top = g_rcPanel.top + S(12);
    g_rcLogs.bottom = g_rcLogs.top + bh;
    g_rcPatch.right = g_rcLogs.left - S(8);
    g_rcPatch.left = g_rcPatch.right - bw;
    g_rcPatch.top = g_rcLogs.top;
    g_rcPatch.bottom = g_rcLogs.bottom;

    // pill
    g_rcPill.left = cx + pad;
    g_rcPill.top = g_rcPanel.top + S(46);
    g_rcPill.right = g_rcPill.left + S(158);
    g_rcPill.bottom = g_rcPill.top + S(28);

    // description
    g_rcDesc.left = cx + pad;
    g_rcDesc.right = cx + cw - pad;
    g_rcDesc.top = g_rcPanel.top + S(84);
    g_rcDesc.bottom = g_rcPanel.top + S(120);

    // clear button (log panel header)
    g_rcClear.right = g_rcLogPanel.right - pad;
    g_rcClear.left = g_rcClear.right - S(64);
    g_rcClear.top = g_rcLogPanel.top + S(10);
    g_rcClear.bottom = g_rcClear.top + S(22);

    // log edit
    g_rcLog.left = cx + pad;
    g_rcLog.right = cx + cw - pad;
    g_rcLog.top = g_rcLogPanel.top + S(42);
    g_rcLog.bottom = g_rcLogPanel.bottom - pad;

    // nav
    int navY = S(96);
    for (int i = 0; i < 4; i++) {
        g_rcNav[i].left = S(12);
        g_rcNav[i].right = sbw - S(12);
        g_rcNav[i].top = navY + i * S(42);
        g_rcNav[i].bottom = g_rcNav[i].top + S(38);
    }
}

// ============================================================
// paint
// ============================================================

static void DrawSolidBtn(HDC mem, RECT rc, const wchar_t* label, bool hot, bool press) {
    COLORREF fill = press ? C_ACCENT_P : (hot ? C_ACCENT_H : C_ACCENT);
    RR(mem, rc, fill, fill, S(10));
    DrawText2(mem, g_fontBtn, label, rc, RGB(0xFF, 0xFF, 0xFF), DT_CENTER);
}

static void DrawOutlineBtn(HDC mem, RECT rc, const wchar_t* label, bool hot, bool press) {
    COLORREF bd = hot ? RGB(0x3D, 0x4E, 0x66) : C_BTN_BD;
    RR(mem, rc, press ? RGB(0x18, 0x20, 0x2C) : C_CARD, bd, S(8));
    DrawText2(mem, g_fontNav, label, rc, hot ? C_TEXT : C_DIM, DT_CENTER);
}

static void PaintUI(HWND hWnd, HDC hdc) {
    RECT rc;
    GetClientRect(hWnd, &rc);

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP ob = (HBITMAP)SelectObject(mem, bmp);

    FillRect(mem, &rc, g_brBg);

    int sbw = S(210);
    int pad = S(20);

    // ---------- sidebar ----------
    RECT sb = { 0, 0, sbw, rc.bottom };
    FillRect(mem, &sb, g_brSide);
    RECT sbd = { sbw - 1, 0, sbw, rc.bottom };
    FillRect(mem, &sbd, g_brSideBd);

    // app icon
    RECT ai = { S(20), S(20), S(20) + S(34), S(20) + S(34) };
    RR(mem, ai, C_ACCENT, C_ACCENT, S(9));
    DrawGlyph(mem, g_fontGlyphBig, L"\u26A1", ai, RGB(0xFF, 0xFF, 0xFF));

    DrawText2(mem, g_fontApp, L"PB Sumatra",
              { S(66), S(20), sbw - S(12), S(38) }, C_TEXT, DT_LEFT);
    DrawText2(mem, g_fontNav, L"VIP Injector",
              { S(66), S(38), sbw - S(12), S(54) }, C_DIM, DT_LEFT);
    DrawText2(mem, g_fontCredit, L"by dekkaaa9",
              { S(66), S(54), sbw - S(12), S(68) }, C_GREEN, DT_LEFT);

    RECT sep = { S(20), S(74), sbw - S(20), S(75) };
    FillRect(mem, &sep, g_brSideBd);

    // nav items
    for (int i = 0; i < 4; i++) {
        RECT r = g_rcNav[i];
        bool act = (g_activeNav == i);
        bool hov = (g_hoverNav == i);
        bool prs = (g_pressAct == 10 + i);
        if (act) {
            RR(mem, r, RGB(0x17, 0x20, 0x2E), RGB(0x17, 0x20, 0x2E), S(8));
            HBRUSH barBr = CreateSolidBrush(C_ACCENT);
            RECT bar = { r.left, r.top + S(8), r.left + S(3), r.bottom - S(8) };
            FillRect(mem, &bar, barBr);
            DeleteObject(barBr);
        } else if (prs) {
            RR(mem, r, RGB(0x18, 0x20, 0x2C), RGB(0x18, 0x20, 0x2C), S(8));
        } else if (hov) {
            RR(mem, r, RGB(0x13, 0x1A, 0x26), RGB(0x13, 0x1A, 0x26), S(8));
        }
        RECT gr = { r.left + S(14), r.top, r.left + S(42), r.bottom };
        DrawGlyph(mem, g_fontGlyph, NAV_GLYPHS[i], gr, (act || hov) ? C_TEXT : C_DIM);
        RECT tr = { r.left + S(46), r.top, r.right - S(8), r.bottom };
        DrawText2(mem, g_fontNav, NAV_LABELS[i], tr, (act || hov) ? C_TEXT : C_DIM, DT_LEFT);
    }

    DrawText2(mem, g_fontNav, L"v1.0.0",
              { S(20), rc.bottom - S(38), sbw - S(20), rc.bottom - S(20) }, C_DIM, DT_LEFT);

    // ---------- stat cards ----------
    CardState* cards[3] = { &g_cardPB, &g_cardAdm, &g_cardDll };
    for (int i = 0; i < 3; i++) {
        RECT c = g_rcCard[i];
        RR(mem, c, C_CARD, C_CARD_BD, S(10));
        int d = S(32);
        RECT ir = { c.left + S(16), c.top + (c.bottom - c.top - d) / 2,
                    c.left + S(16) + d, c.top + (c.bottom - c.top + d) / 2 };
        RR(mem, ir, Blend(cards[i]->color, C_CARD, 0.22f), Blend(cards[i]->color, C_CARD, 0.22f), d / 2);
        DrawGlyph(mem, g_fontGlyph, CARD_GLYPHS[i], ir, cards[i]->color);
        RECT lr = { c.left + S(60), c.top + S(14), c.right - S(12), c.top + S(30) };
        DrawText2(mem, g_fontLbl, cards[i]->label, lr, C_DIM, DT_LEFT);
        RECT vr = { c.left + S(60), c.top + S(32), c.right - S(12), c.top + S(60) };
        DrawText2(mem, g_fontVal, cards[i]->value, vr, cards[i]->color, DT_LEFT | DT_END_ELLIPSIS);
    }

    // ---------- control panel ----------
    RR(mem, g_rcPanel, C_CARD, C_CARD_BD, S(10));
    DrawText2(mem, g_fontLbl, L"CONTROL",
              { g_rcPanel.left + pad, g_rcPanel.top + S(12), g_rcPanel.left + S(200), g_rcPanel.top + S(30) },
              C_DIM, DT_LEFT);
    DrawOutlineBtn(mem, g_rcPatch, L"\u2699  Re-patch DLL", g_hoverAct == 2, g_pressAct == 2);
    DrawOutlineBtn(mem, g_rcLogs, L"\u2630  Open Logs", g_hoverAct == 3, g_pressAct == 3);

    // pill
    RR(mem, g_rcPill, g_pillBg, g_pillBg, S(14));
    DrawText2(mem, g_fontPill, std::wstring(L"\u25CF  ") + g_pillText, g_rcPill, g_pillCol, DT_CENTER);

    // description
    {
        HFONT old = (HFONT)SelectObject(mem, g_fontDesc);
        SetTextColor(mem, C_DIM);
        SetBkMode(mem, TRANSPARENT);
        DrawTextW(mem,
                  L"Klik INJECT buat inject payload VIP (decrypted_payload_cracked.dll) ke proses "
                  L"PointBlank. Pastiin game udah jalan dan lu udah di lobby \u2014 menu muncul otomatis.",
                  -1, &g_rcDesc, DT_LEFT | DT_WORDBREAK | DT_TOP);
        SelectObject(mem, old);
    }

    // inject button
    DrawSolidBtn(mem, g_rcInject, L"\u26A1   INJECT", g_hoverAct == 1, g_pressAct == 1);

    // ---------- log panel ----------
    RR(mem, g_rcLogPanel, C_CARD, C_CARD_BD, S(10));
    DrawText2(mem, g_fontLbl, L"LOG",
              { g_rcLogPanel.left + pad, g_rcLogPanel.top + S(10), g_rcLogPanel.left + S(120), g_rcLogPanel.top + S(32) },
              C_DIM, DT_LEFT);
    DrawOutlineBtn(mem, g_rcClear, L"Clear", g_hoverAct == 4, g_pressAct == 4);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// ============================================================
// window proc
// ============================================================

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        {
            HMODULE u32 = GetModuleHandleW(L"user32.dll");
            if (u32) {
                typedef UINT (WINAPI *FnDpi)(HWND);
                FARPROC p = GetProcAddress(u32, "GetDpiForWindow");
                if (p) g_dpi = ((FnDpi)p)(hWnd);
            }
            if (!g_dpi) {
                HDC dc = GetDC(hWnd);
                g_dpi = GetDeviceCaps(dc, LOGPIXELSY);
                ReleaseDC(hWnd, dc);
            }
            if (!g_dpi) g_dpi = 96;
        }
        g_scale = g_dpi / 96.0;

        Layout(hWnd);

        g_fontApp   = MakeFont(13, FW_SEMIBOLD, false);
        g_fontNav   = MakeFont(10, FW_MEDIUM, false);
        g_fontLbl   = MakeFont(8,  FW_BOLD, false);
        g_fontVal   = MakeFont(13, FW_SEMIBOLD, false);
        g_fontBtn   = MakeFont(14, FW_BOLD, false);
        g_fontPill  = MakeFont(9,  FW_SEMIBOLD, false);
        g_fontDesc  = MakeFont(9,  FW_NORMAL, false);
        g_fontMono  = MakeFont(10, FW_NORMAL, true);
        g_fontGlyph = MakeGlyphFont(13);
        g_fontGlyphBig = MakeGlyphFont(16);
        g_fontCredit = MakeFont(8, FW_SEMIBOLD, false);

        g_hLog = CreateWindowExW(0, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                 ES_AUTOVSCROLL | ES_READONLY,
                                 g_rcLog.left, g_rcLog.top,
                                 g_rcLog.right - g_rcLog.left, g_rcLog.bottom - g_rcLog.top,
                                 hWnd, (HMENU)IDC_LOG, g_hInst, NULL);
        SendMessageW(g_hLog, WM_SETFONT, (WPARAM)g_fontMono, TRUE);
        SendMessageW(g_hLog, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(10), S(10)));

        // admin card
        g_admin = IsAdmin();
        if (g_admin) {
            g_cardAdm.value = L"YA";
            g_cardAdm.color = C_GREEN;
        } else {
            g_cardAdm.value = L"TIDAK \u2014 INJECT minta UAC";
            g_cardAdm.color = C_AMBER;
        }

        // dll card
        {
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(DLL_PATH_W.c_str(), GetFileExInfoStandard, &fad)) {
                ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                wchar_t b[300];
                swprintf(b, 300, L"decrypted_payload_cracked.dll  ·  %llu bytes",
                         (unsigned long long)sz);
                g_cardDll.value = b;
                g_cardDll.color = C_GREEN;
                g_dllOk = true;
            } else {
                g_cardDll.value = L"FILE HILANG";
                g_cardDll.color = C_RED;
                g_dllOk = false;
            }
        }

        UpdatePbCard();
        UpdatePill();
        SetTimer(hWnd, 1, 1000, NULL);

        {
            RECT wr, cr;
            GetWindowRect(hWnd, &wr);
            GetClientRect(hWnd, &cr);
            wchar_t b[300];
            swprintf(b, 300, L"ui: dpi=%u scale=%.2f client=%ldx%ld window=%ldx%ld",
                     g_dpi, g_scale, cr.right, cr.bottom,
                     wr.right - wr.left, wr.bottom - wr.top);
            AppendLogLine(b);
        }
        return 0;
    }

    case WM_TIMER:
        UpdatePbCard();
        UpdatePill();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        PaintUI(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        if ((HWND)lParam == g_hLog) {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, C_LOG_TX);
            SetBkColor(hdc, C_LOG_BG);
            return (LRESULT)g_brLog;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        int hn = -1;
        for (int i = 0; i < 4; i++)
            if (PtInRect(&g_rcNav[i], pt)) hn = i;
        int ha = 0;
        if      (PtInRect(&g_rcInject, pt)) ha = 1;
        else if (PtInRect(&g_rcPatch, pt))  ha = 2;
        else if (PtInRect(&g_rcLogs, pt))   ha = 3;
        else if (PtInRect(&g_rcClear, pt))  ha = 4;
        if (hn != g_hoverNav || ha != g_hoverAct) {
            g_hoverNav = hn;
            g_hoverAct = ha;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        SetCursor(LoadCursorW(NULL, (hn >= 0 || ha) ? IDC_HAND : IDC_ARROW));
        if (!g_trkLea) {
            TRACKMOUSEEVENT tme;
            ZeroMemory(&tme, sizeof(tme));
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
            g_trkLea = true;
        }
        break;
    }

    case WM_MOUSELEAVE:
        g_hoverNav = -1;
        g_hoverAct = 0;
        g_trkLea = false;
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        if (PtInRect(&g_rcInject, pt))      g_pressAct = 1;
        else if (PtInRect(&g_rcPatch, pt))  g_pressAct = 2;
        else if (PtInRect(&g_rcLogs, pt))   g_pressAct = 3;
        else if (PtInRect(&g_rcClear, pt))  g_pressAct = 4;
        else {
            for (int i = 0; i < 4; i++) {
                if (PtInRect(&g_rcNav[i], pt)) {
                    g_pressAct = 10 + i;
                    break;
                }
            }
        }
        if (!g_pressAct) break;
        SetCapture(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONUP: {
        if (GetCapture() == hWnd) ReleaseCapture();
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        int act = g_pressAct;
        g_pressAct = 0;
        if (act >= 10) {
            int nav = act - 10;
            if (PtInRect(&g_rcNav[nav], pt)) {
                g_activeNav = 0;
                if (nav == 1) DoPatch();
                else if (nav == 2) DoLogs();
                else if (nav == 3) DoAbout();
            }
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        InvalidateRect(hWnd, NULL, FALSE);
        if (act == 1 && PtInRect(&g_rcInject, pt)) StartInjection();
        else if (act == 2 && PtInRect(&g_rcPatch, pt)) DoPatch();
        else if (act == 3 && PtInRect(&g_rcLogs, pt)) DoLogs();
        else if (act == 4 && PtInRect(&g_rcClear, pt)) DoClear();
        return 0;
    }

    case WM_APP_LOG: {
        LogMsg* m = (LogMsg*)lParam;
        if (m) {
            AppendLogLine(m->str);
            delete m;
        }
        return 0;
    }

    case WM_APP_AUTO:
        StartInjection();
        return 0;

    case WM_APP_DONE: {
        g_busy = false;
        UpdatePill();
        InvalidateRect(g_hMain, NULL, FALSE);
        int r = (int)wParam;
        if (r == 0) {
            MessageBoxW(hWnd, L"Inject SUKSES \u2014 menu muncul otomatis di lobby.",
                        L"PB Sumatra", MB_OK | MB_ICONINFORMATION);
        } else if (r == 2) {
            MessageBoxW(hWnd, L"PointBlank gak jalan.\nBuka game, masuk lobby, baru klik INJECT.",
                        L"PB Sumatra", MB_OK | MB_ICONWARNING);
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        if (g_hLogFile != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hLogFile);
            g_hLogFile = INVALID_HANDLE_VALUE;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================
// entry
// ============================================================

static void EnableDpiAwareness() {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        typedef BOOL (WINAPI *FnCtx)(void*);
        FARPROC p = GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (p) {
            if (((FnCtx)p)((void*)-4)) return;
            if (((FnCtx)p)((void*)-3)) return;
        }
    }
    SetProcessDPIAware();
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int nShow) {
    g_hInst = hInst;

    bool autoInject = (lpCmdLine && wcsstr(lpCmdLine, L"/auto") != NULL);

    EnableDpiAwareness();

    if (!autoInject) {
        HANDLE hMutex = CreateMutexW(NULL, TRUE, L"PBSumatraInjectorGUI_SingleInstance");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND existing = FindWindowW(L"PBSumatraInjectorGUI", NULL);
            if (existing) {
                ShowWindow(existing, SW_RESTORE);
                SetForegroundWindow(existing);
            }
            return 0;
        }
        (void)hMutex;
    }

    ResolvePaths();

    CreateDirectoryW(LOG_DIR_W.c_str(), NULL);
    {
        char logA[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, LOG_PATH_W.c_str(), -1, logA, MAX_PATH, NULL, NULL);
        g_hLogFile = CreateFileA(logA, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                 OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (g_hLogFile != INVALID_HANDLE_VALUE)
        SetFilePointer(g_hLogFile, 0, NULL, FILE_END);

    g_brBg     = CreateSolidBrush(C_BG);
    g_brSide   = CreateSolidBrush(C_SIDE);
    g_brSideBd = CreateSolidBrush(C_SIDE_BD);
    g_brLog    = CreateSolidBrush(C_LOG_BG);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"PBSumatraInjectorGUI";
    wc.hbrBackground = g_brBg;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    int cw = 980, chh = 680;
    {
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        int maxH = (wa.bottom - wa.top) - S(80);
        int maxW = (wa.right - wa.left) - S(80);
        if (chh > maxH) chh = maxH;
        if (cw > maxW) cw = maxW;
        if (chh < 560) chh = 560;
        if (cw < 720) cw = 720;
    }

    RECT rc = { 0, 0, cw, chh };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    g_hMain = CreateWindowExW(0, wc.lpszClassName, L"PB Sumatra - VIP Injector",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top,
                              NULL, NULL, hInst, NULL);
    if (!g_hMain) return 1;

    {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(g_hMain, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        DWORD corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(g_hMain, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    }

    ShowWindow(g_hMain, nShow);
    UpdateWindow(g_hMain);
    SetForegroundWindow(g_hMain);

    if (autoInject) PostMessageW(g_hMain, WM_APP_AUTO, 0, 0);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}