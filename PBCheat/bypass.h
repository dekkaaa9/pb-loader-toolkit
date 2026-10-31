// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PBC_BYPASS_H
#define PBC_BYPASS_H

namespace PB {
namespace Bypass {

    // Kill CB.exe watchdog and start a re-kill thread
    void KillCheatBlocker();

    // Patch PEB BeingDebugged + NtGlobalFlag
    void PatchAntiDebug();

    // Prevent CB.exe from being relaunched by hooking CreateProcessInternalW
    void BlockCBProcessCreation();

    // Run all bypass routines
    void Initialize();

} // namespace Bypass
} // namespace PB

#endif