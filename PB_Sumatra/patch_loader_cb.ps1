# ============================================================
# PATCH LOADER: Remove CB.exe kill switch
# The watchdog loop (FUN_00404d50) checks if "CB.exe" is running
# BEFORE injecting. If CB.exe is running -> injection is SKIPPED.
#
# Fix: NOP the JNZ at RVA 0x72C5 (FO 0x66C5): 75 04 -> 90 90
# Result: loader ALWAYS injects, regardless of CB.exe
# ============================================================

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$loaderPath = 'D:\PB_Sumatra\LoaderVIP_cracked.exe'
$bytes = [System.IO.File]::ReadAllBytes($loaderPath)

# RVA 0x72C5 -> FO = 0x400 + (0x72C5 - 0x1000) = 0x66C5
$fo = 0x66C5

Write-Output "=== BEFORE ==="
Write-Output ("FO 0x{0:X}: {1}" -f $fo, (($bytes[$fo..($fo+7)] | ForEach-Object { $_.ToString('X2') }) -join ' '))
Write-Output "Expected: 85 C0 75 04 B1 01 EB 02 32 C9  (TEST EAX,EAX; JNZ +4; MOV CL,1; JMP +2; XOR CL,CL)"

# Verify context: TEST EAX,EAX (85 C0) before, MOV CL,1 (B1 01) after
if ($bytes[$fo - 2] -ne 0x85 -or $bytes[$fo - 1] -ne 0xC0) {
    Write-Output "[!] ERROR: context mismatch before patch point (expected 85 C0). Aborting."
    exit 1
}
if ($bytes[$fo + 2] -ne 0xB1 -or $bytes[$fo + 3] -ne 0x01) {
    Write-Output "[!] ERROR: context mismatch after patch point (expected B1 01). Aborting."
    exit 1
}
if ($bytes[$fo] -ne 0x75 -or $bytes[$fo + 1] -ne 0x04) {
    Write-Output "[!] ERROR: JNZ bytes not found (expected 75 04). Already patched?"
    if ($bytes[$fo] -eq 0x90 -and $bytes[$fo + 1] -eq 0x90) {
        Write-Output "[*] Already patched (90 90). Nothing to do."
        exit 0
    }
    exit 1
}

# Apply patch: JNZ +4 -> NOP NOP
$bytes[$fo] = 0x90
$bytes[$fo + 1] = 0x90

[System.IO.File]::WriteAllBytes($loaderPath, $bytes)

Write-Output ""
Write-Output "=== AFTER ==="
Write-Output ("FO 0x{0:X}: {1}" -f $fo, (($bytes[$fo..($fo+7)] | ForEach-Object { $_.ToString('X2') }) -join ' '))
Write-Output ""
Write-Output "[+] CB.exe kill switch REMOVED!"
Write-Output "[+] Loader will now inject regardless of CB.exe."
Write-Output ""
Write-Output "Backup of original loader: LoaderVIP_cracked.exe.bak (if not exists)"

# Create backup of the ORIGINAL (pre-this-patch) loader if not already done
$bak = 'D:\PB_Sumatra\LoaderVIP_cracked_precbpatch.exe.bak'
if (-not (Test-Path $bak)) {
    # Re-read the file we just wrote, restore the 2 bytes for the backup
    $bakBytes = [System.IO.File]::ReadAllBytes($loaderPath)
    $bakBytes[$fo] = 0x75
    $bakBytes[$fo + 1] = 0x04
    [System.IO.File]::WriteAllBytes($bak, $bakBytes)
    Write-Output "[+] Backup saved: $bak"
}