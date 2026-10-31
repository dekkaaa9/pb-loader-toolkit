# ============================================================
# FIX LoaderVIP_cracked.exe - file size check patch was WRONG
# Changed jle -> jmp which ALWAYS jumps to SIZE ERROR path
# Correct fix: jle -> NOP NOP (never jump, always continue)
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

Write-Output "=== FIXING FILE SIZE CHECK PATCH ==="
Write-Output ""

# Verify original bytes at FO 0x38CA
$orig = @()
for ($i = 0; $i -lt 2; $i++) { $orig += $bytes[0x38CA + $i] }
$origHex = ($orig | ForEach-Object { $_.ToString('X2') }) -join ' '
Write-Output "  Original FO 0x38CA: $origHex (expected: 7E 4C = jle)"
if ($origHex -ne "7E 4C") {
    Write-Output "  ABORT: unexpected bytes!"
    exit 1
}

# FIX: Change jle (7E 4C) to NOP NOP (90 90)
# This removes the conditional jump entirely.
# Original logic: if file_size <= 5000, jump to SIZE ERROR
# With NOP: never jump, always continue (file size check bypassed)
$bytes[0x38CA] = 0x90
$bytes[0x38CB] = 0x90
Write-Output "  [FIXED] FO 0x38CA: 7E 4C -> 90 90 (jle -> NOP NOP)"
Write-Output ""

# Re-apply the other 3 patches (they were correct)

# Patch 1: Serial blacklist jz -> NOP (FO 0x2FA9)
$p1 = @()
for ($i = 0; $i -lt 6; $i++) { $p1 += $bytes[0x2FA9 + $i] }
$p1Hex = ($p1 | ForEach-Object { $_.ToString('X2') }) -join ' '
if ($p1Hex -eq "0F 84 5D 0A 00 00") {
    for ($i = 0; $i -lt 6; $i++) { $bytes[0x2FA9 + $i] = 0x90 }
    Write-Output "  [1] FO 0x2FA9: jz -> NOP x6 (serial blacklist bypassed)"
} else {
    Write-Output "  [1] FO 0x2FA9: SKIPPED (bytes: $p1Hex)"
}

# Patch 3: Timer offset max (FO 0x37AA and 0x37B5)
# Original: add eax, 0xAC5ED800 ; adc ecx, 0x3A
# Patched:  add eax, 0xFFFFFFFF ; adc ecx, 0x7F
$p3 = @()
for ($i = 0; $i -lt 4; $i++) { $p3 += $bytes[0x37AA + $i] }
$p3Hex = ($p3 | ForEach-Object { $_.ToString('X2') }) -join ' '
if ($p3Hex -eq "00 D8 5E AC") {
    $bytes[0x37AA] = 0xFF
    $bytes[0x37AB] = 0xFF
    $bytes[0x37AC] = 0xFF
    $bytes[0x37AD] = 0xFF
    Write-Output "  [3a] FO 0x37AA: add eax, 0xFFFFFFFF (timer max)"
} else {
    Write-Output "  [3a] FO 0x37AA: SKIPPED (bytes: $p3Hex)"
}

$p3b = $bytes[0x37B5]
if ($p3b -eq 0x3A) {
    $bytes[0x37B5] = 0x7F
    Write-Output "  [3b] FO 0x37B5: adc ecx, 0x7F (timer max)"
} else {
    Write-Output "  [3b] FO 0x37B5: SKIPPED (byte: $($p3b.ToString('X2')))"
}

# Save
[System.IO.File]::WriteAllBytes($outputPath, $bytes)

Write-Output ""
Write-Output "=== VERIFY FINAL PATCHES ==="
$check = [System.IO.File]::ReadAllBytes($outputPath)

function V($fo, $expected, $label) {
    $actual = @()
    for ($i = 0; $i -lt $expected.Length; $i++) { $actual += $check[$fo + $i] }
    $expHex = ($expected | ForEach-Object { $_.ToString('X2') }) -join ' '
    $actHex = ($actual | ForEach-Object { $_.ToString('X2') }) -join ' '
    $status = if ($actHex -eq $expHex) { "OK" } else { "FAIL" }
    Write-Output "  [$status] $label`: $actHex (expected: $expHex)"
}

V 0x2FA9 @(0x90,0x90,0x90,0x90,0x90,0x90) "Serial blacklist NOP"
V 0x38CA @(0x90,0x90) "File size NOP (FIXED)"
V 0x37AA @(0xFF,0xFF,0xFF,0xFF) "Timer add eax max"
V 0x37B5 @(0x7F) "Timer adc ecx max"

Write-Output ""
Write-Output "Saved to: $outputPath"
Write-Output "SHA256: $((Get-FileHash $outputPath -Algorithm SHA256).Hash)"