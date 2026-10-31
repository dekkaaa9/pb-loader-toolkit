// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#include <windows.h>
#include <cstdio>
#include "bypass.h"
#include "hooks.h"
#include "config.h"

// Config global
namespace PB {
    Config g_cfg;
}

static FILE* g_mainLog = nullptr;
static void MLog(const char* msg) {
    if (!g_mainLog) g_mainLog = fopen("D:\\pbct\\cheat_log.txt", "w");
    if (!g_mainLog) return;
    fprintf(g_mainLog, "%s\n", msg);
    fflush(g_mainLog);
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        MLog("[*] DllMain: DLL_PROCESS_ATTACH");
        DisableThreadLibraryCalls(hInst);
        PB::Hooks::Initialize();
        MLog("[*] DllMain: Hooks::Initialize returned");
        break;

    case DLL_PROCESS_DETACH:
        if (!reserved) {
            PB::Hooks::Shutdown();
        }
        break;
    }
    return TRUE;
}