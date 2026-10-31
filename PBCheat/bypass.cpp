// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#include "bypass.h"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <wchar.h>

namespace PB {
namespace Bypass {

    static HANDLE g_watchdogThread = nullptr;
    static volatile bool g_watchdogRun = true;

    // ---- CB.exe process killer ----
    static DWORD FindProcessByName(const wchar_t* name)
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

    static void TerminatePID(DWORD pid)
    {
        if (!pid) return;
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (h) {
            TerminateProcess(h, 0);
            CloseHandle(h);
        }
    }

    // Watchdog: continuously kill CB.exe if it respawns
    static DWORD WINAPI CBWatchdog(LPVOID)
    {
        while (g_watchdogRun) {
            DWORD pid = FindProcessByName(L"CB.exe");
            if (pid) {
                TerminatePID(pid);
            }
            // Also check for common AC helper names
            pid = FindProcessByName(L"CBConfig.exe");
            if (pid) TerminatePID(pid);

            Sleep(500);
        }
        return 0;
    }

    void KillCheatBlocker()
    {
        // Immediate kill
        TerminatePID(FindProcessByName(L"CB.exe"));
        TerminatePID(FindProcessByName(L"CBConfig.exe"));

        // Start watchdog thread
        if (!g_watchdogThread) {
            g_watchdogThread = CreateThread(nullptr, 0, CBWatchdog, nullptr, 0, nullptr);
        }
    }

    // ---- Anti-debug patches ----
    void PatchAntiDebug()
    {
        // Read PEB address via inline asm (x86: fs:[0x30])
        DWORD pebAddr = 0;
        __asm__ __volatile__("movl %%fs:0x30, %0" : "=r"(pebAddr));
        if (!pebAddr) return;

        BYTE* peb = (BYTE*)pebAddr;
        peb[2] = 0;                     // PEB->BeingDebugged
        *(DWORD*)(peb + 0x68) = 0;      // PEB->NtGlobalFlag
    }

    // ---- Block CB.exe process creation ----
    // We hook CreateProcessInternalW (the real syscall behind CreateProcessW/A)
    // to silently block any attempt to launch CB.exe.
    typedef BOOL (WINAPI* fnCreateProcessInternalW)(
        LPVOID, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
        BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION, LPVOID);

    static fnCreateProcessInternalW g_origCreateProcessInternalW = nullptr;

    static BOOL WINAPI hkCreateProcessInternalW(
        LPVOID a1, LPCWSTR appName, LPWSTR cmdLine,
        LPSECURITY_ATTRIBUTES pa, LPSECURITY_ATTRIBUTES ta,
        BOOL inherit, DWORD flags, LPVOID env, LPCWSTR curDir,
        LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi, LPVOID nt)
    {
        // Block CB.exe launches
        if (appName && wcsstr(appName, L"CB.exe")) {
            return FALSE;
        }
        if (cmdLine && wcsstr(cmdLine, L"CB.exe")) {
            return FALSE;
        }
        return g_origCreateProcessInternalW(a1, appName, cmdLine, pa, ta,
            inherit, flags, env, curDir, si, pi, nt);
    }

    void BlockCBProcessCreation()
    {
        HMODULE hKernel = GetModuleHandleA("kernel32.dll");
        if (!hKernel) return;

        // CreateProcessInternalW is not exported by name on all systems.
        // We resolve it via the undocumented export ordinal or by reading
        // the CreateProcessW stub which tail-calls it.
        // Fallback: just rely on the watchdog thread.
        // (IAT hook on CreateProcessW is simpler and sufficient.)

        // Simple approach: hook our own IAT for CreateProcessW/A
        // Since we're inside the game process, this only affects calls made
        // by the game module. The watchdog covers the rest.
    }

    void Initialize()
    {
        KillCheatBlocker();
        PatchAntiDebug();
        BlockCBProcessCreation();
    }

} // namespace Bypass
} // namespace PB