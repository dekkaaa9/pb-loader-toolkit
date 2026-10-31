# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9

# ---- portable paths: repo root auto-detected from script location ----
$Sumatra = $PSScriptRoot
$Pbct    = Split-Path $PSScriptRoot -Parent
# ------------------------------------------------------------
$orig = [System.IO.File]::ReadAllBytes('C:\Users\PIXWAR\AppData\Local\Temp\LoaderTrialTemp\LoaderVIP.exe')
$cracked = [System.IO.File]::ReadAllBytes((Join-Path $Sumatra 'LoaderVIP_cracked.exe'))

Write-Output "=== VERIFY PATCHES IN LoaderVIP_cracked.exe ==="
Write-Output ""

function Check($fo, $expected, $label, $arr) {
    $actual = @()
    for ($i = 0; $i -lt $expected.Length; $i++) { $actual += $arr[$fo + $i] }
    $expHex = ($expected | ForEach-Object { $_.ToString('X2') }) -join ' '
    $actHex = ($actual | ForEach-Object { $_.ToString('X2') }) -join ' '
    $status = if ($actHex -eq $expHex) { "OK" } else { "MISMATCH" }
    Write-Output "  [$status] $label (FO 0x$($fo.ToString('X'))): $actHex"
}

Write-Output "--- Original bytes ---"
Check 0x2FA9 @(0x0F,0x84,0x5D,0x0A,0x00,0x00) "Serial blacklist jz" $orig
Check 0x38CA @(0x7E,0x4C) "File size jle" $orig
Check 0x37AA @(0x00,0xD8,0x5E,0xAC) "Timer add eax" $orig
Check 0x37B5 @(0x3A) "Timer adc ecx" $orig

Write-Output ""
Write-Output "--- Cracked bytes ---"
Check 0x2FA9 @(0x90,0x90,0x90,0x90,0x90,0x90) "Serial blacklist NOP" $cracked
Check 0x38CA @(0xEB,0x4C) "File size jmp" $cracked
Check 0x37AA @(0xFF,0xFF,0xFF,0xFF) "Timer add eax max" $cracked
Check 0x37B5 @(0x7F) "Timer adc ecx max" $cracked

Write-Output ""
Write-Output "=== CONTEXT AROUND FILE SIZE CHECK (FO 0x38B0-0x38E0) ==="
Write-Output "--- Original ---"
for ($i = 0x38B0; $i -lt 0x38E0; $i++) {
    if (($i - 0x38B0) % 16 -eq 0) { Write-Output ""; Write-Output ("FO 0x{0:X}: " -f $i) -NoNewline }
    Write-Output ("{0:X2} " -f $orig[$i]) -NoNewline
}
Write-Output ""
Write-Output "--- Cracked ---"
for ($i = 0x38B0; $i -lt 0x38E0; $i++) {
    if (($i - 0x38B0) % 16 -eq 0) { Write-Output ""; Write-Output ("FO 0x{0:X}: " -f $i) -NoNewline }
    Write-Output ("{0:X2} " -f $cracked[$i]) -NoNewline
}
Write-Output ""