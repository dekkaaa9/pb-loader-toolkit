# ============================================================
# LoaderVIP.exe CRACKER
# Patches:
#   1. Serial validation bypass (NOP the jz -> always download path)
#   2. File size check bypass (jle -> jmp, always skip error)
#   3. Timer offset maximize (add ~forever instead of 7 hours)
# ============================================================

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9

# ---- portable paths: repo root auto-detected from script location ----
$Sumatra = $PSScriptRoot
$Pbct    = Split-Path $PSScriptRoot -Parent
# ------------------------------------------------------------
$sourcePath = 'C:\Users\PIXWAR\AppData\Local\Temp\LoaderTrialTemp\LoaderVIP.exe'
$outputPath = (Join-Path $Sumatra 'LoaderVIP_cracked.exe')

$bytes = [System.IO.File]::ReadAllBytes($sourcePath)
$origHash = (Get-FileHash $sourcePath -Algorithm SHA256).Hash

# Verify original bytes before patching
function Verify($fo, $expected, $label) {
    $actual = @()
    for ($i = 0; $i -lt $expected.Length; $i++) { $actual += $bytes[$fo + $i] }
    $expHex = ($expected | ForEach-Object { $_.ToString('X2') }) -join ' '
    $actHex = ($actual | ForEach-Object { $_.ToString('X2') }) -join ' '
    if ($actHex -eq $expHex) {
        Write-Output "  [OK] $label`: $actHex"
        return $true
    } else {
        Write-Output "  [FAIL] $label`: expected $expHex, got $actHex"
        return $false
    }
}

Write-Output "=== VERIFYING PATCH POINTS ==="
$ok = $true

# Patch 1: Serial validation bypass
# VA 0x403BA9, FO 0x2FA9: 0F 84 5D 0A 00 00 (jz -> error path)
# Replace with NOPs so it always falls through to download
$ok = (Verify 0x2FA9 @(0x0F,0x84,0x5D,0x0A,0x00,0x00) "Serial jz") -and $ok

# Patch 2: File size check bypass  
# VA 0x4044CA, FO 0x38CA: 7E 4C (jle -> skip error if <= 5000)
# Replace with EB 4C (jmp -> always skip error)
$ok = (Verify 0x38CA @(0x7E,0x4C) "File size jle") -and $ok

# Patch 3: Timer offset - add eax part
# VA 0x4043AA, FO 0x37AA: 00 D8 5E AC (add eax, 0xAC5ED800)
# Replace with FF FF FF FF (add eax, 0xFFFFFFFF)
$ok = (Verify 0x37AA @(0x00,0xD8,0x5E,0xAC) "Timer add eax") -and $ok

# Patch 3b: Timer offset - adc ecx part
# VA 0x4043B5, FO 0x37B5: 3A (adc ecx, 0x3A)
# Replace with 7F (adc ecx, 0x7F)
$ok = (Verify 0x37B5 @(0x3A) "Timer adc ecx") -and $ok

if (-not $ok) {
    Write-Output ""
    Write-Output "ABORTED: Byte verification failed. Wrong file or already patched?"
    exit 1
}

Write-Output ""
Write-Output "=== APPLYING PATCHES ==="

# Patch 1: NOP the serial jz (6 bytes)
for ($i = 0; $i -lt 6; $i++) { $bytes[0x2FA9 + $i] = 0x90 }
Write-Output "  [PATCH 1] FO 0x2FA9: 0F 84 5D 0A 00 00 -> 90 90 90 90 90 90 (serial bypass)"

# Patch 2: jle -> jmp
$bytes[0x38CA] = 0xEB
Write-Output "  [PATCH 2] FO 0x38CA: 7E 4C -> EB 4C (file size check bypass)"

# Patch 3: add eax, 0xFFFFFFFF
$bytes[0x37AA] = 0xFF
$bytes[0x37AB] = 0xFF
$bytes[0x37AC] = 0xFF
$bytes[0x37AD] = 0xFF
Write-Output "  [PATCH 3] FO 0x37AA: 00 D8 5E AC -> FF FF FF FF (timer offset max low)"

# Patch 3b: adc ecx, 0x7F
$bytes[0x37B5] = 0x7F
Write-Output "  [PATCH 3b] FO 0x37B5: 3A -> 7F (timer offset max high)"

# Save
[System.IO.File]::WriteAllBytes($outputPath, $bytes)
$newHash = (Get-FileHash $outputPath -Algorithm SHA256).Hash

Write-Output ""
Write-Output "=== DONE ==="
Write-Output "Original SHA256: $origHash"
Write-Output "Patched  SHA256: $newHash"
Write-Output "Saved to: $outputPath"
Write-Output ""
Write-Output "=== PATCH SUMMARY ==="
Write-Output "1. SERIAL BYPASS: The 'menten' blacklist check is NOPed."
Write-Output "   Any serial key will be accepted (even wrong/empty ones)."
Write-Output ""
Write-Output "2. FILE SIZE BYPASS: The 5000-byte minimum check is skipped."
Write-Output "   Downloaded files of any size will be accepted."
Write-Output ""
Write-Output "3. TIMER MAXIMIZED: The expiry timestamp offset is changed"
Write-Output "   from ~7 hours to ~forever. The countdown display will"
Write-Output "   show a far-future date instead of expiring."
Write-Output ""
Write-Output "=== IMPORTANT NOTE ==="
Write-Output "The 30-minute enforcement is in the INJECTED DLL (downloaded"
Write-Output "from server, XOR-decrypted, written to temp, then injected)."
Write-Output "After running the cracked loader, grab the decrypted DLL from"
Write-Output "%TEMP% to analyze/patch its internal timer."