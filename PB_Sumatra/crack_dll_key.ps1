# ============================================================
# CHEAT DLL KEY SYSTEM CRACKER
# File: decrypted_payload.dll (781KB native x86 DLL)
#
# Patches:
#   1. Validation call site 1: jnz -> jmp (always "passed")
#   2. Validation call site 2: jz  -> NOP (always fall through)
#   3a. Menu render check 1: jnz -> NOP (always render)
#   3b. Menu render check 2: jnz -> NOP (always render)
#
# Result: ANY key (or no key) works. No license needed.
# ============================================================

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9

# ---- portable paths: repo root auto-detected from script location ----
$Sumatra = $PSScriptRoot
$Pbct    = Split-Path $PSScriptRoot -Parent
# ------------------------------------------------------------
$sourcePath = (Join-Path $Sumatra 'decrypted_payload.dll')
$outputPath = (Join-Path $Pbct 'decrypted_payload_cracked.dll')

$bytes = [System.IO.File]::ReadAllBytes($sourcePath)
$origHash = (Get-FileHash $sourcePath -Algorithm SHA256).Hash

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

# Patch 1: Call site 1 - jnz -> jmp
$ok = (Verify 0x4A012 @(0x0F,0x85,0xB1,0x00,0x00,0x00) "Call site 1 jnz") -and $ok

# Patch 2: Call site 2 - jz -> NOP
$ok = (Verify 0x4A0C3 @(0x0F,0x84,0x57,0xFF,0xFF,0xFF) "Call site 2 jz") -and $ok

# Patch 3a: Menu check 1 - jnz -> NOP
$ok = (Verify 0x3507A @(0x0F,0x85,0x42,0x07,0x00,0x00) "Menu check 1 jnz") -and $ok

# Patch 3b: Menu check 2 - jnz -> NOP
$ok = (Verify 0x35087 @(0x0F,0x85,0x35,0x07,0x00,0x00) "Menu check 2 jnz") -and $ok

if (-not $ok) {
    Write-Output ""
    Write-Output "ABORTED: Byte verification failed!"
    exit 1
}

Write-Output ""
Write-Output "=== APPLYING PATCHES ==="

# Patch 1: jnz -> jmp (always take passed path after validation)
# 0F 85 B1 00 00 00 (jnz +0xB1) -> E9 B2 00 00 00 90 (jmp +0xB2, nop)
$bytes[0x4A012] = 0xE9
$bytes[0x4A013] = 0xB2
$bytes[0x4A014] = 0x00
$bytes[0x4A015] = 0x00
$bytes[0x4A016] = 0x00
$bytes[0x4A017] = 0x90
Write-Output "  [1] FO 0x4A012: jnz -> jmp (validation always passes)"

# Patch 2: jz -> NOP x6 (remove retry loop, always fall through to passed)
for ($i = 0; $i -lt 6; $i++) { $bytes[0x4A0C3 + $i] = 0x90 }
Write-Output "  [2] FO 0x4A0C3: jz -> NOP x6 (no retry loop)"

# Patch 3a: jnz -> NOP x6 (menu always renders)
for ($i = 0; $i -lt 6; $i++) { $bytes[0x3507A + $i] = 0x90 }
Write-Output "  [3a] FO 0x3507A: jnz -> NOP x6 (menu check 1 bypassed)"

# Patch 3b: jnz -> NOP x6 (menu always renders)
for ($i = 0; $i -lt 6; $i++) { $bytes[0x35087 + $i] = 0x90 }
Write-Output "  [3b] FO 0x35087: jnz -> NOP x6 (menu check 2 bypassed)"

# Save
[System.IO.File]::WriteAllBytes($outputPath, $bytes)
$newHash = (Get-FileHash $outputPath -Algorithm SHA256).Hash

Write-Output ""
Write-Output "=== DONE ==="
Write-Output "Original SHA256: $origHash"
Write-Output "Cracked  SHA256: $newHash"
Write-Output "Saved to: $outputPath"
Write-Output ""
Write-Output "=== CRACK SUMMARY ==="
Write-Output "KEY SYSTEM FULLY BYPASSED:"
Write-Output "  - Any serial key works (or no key at all)"
Write-Output "  - No VIP license purchase needed"
Write-Output "  - Server validation result is ignored"
Write-Output "  - Menu always renders regardless of license status"
Write-Output ""
Write-Output "=== HOW TO USE ==="
Write-Output "1. Replace the decrypted DLL with the cracked version:"
Write-Output "   Copy decrypted_payload_cracked.dll over the temp DLL"
Write-Output "   after LoaderVIP.exe decrypts it (in %TEMP%)"
Write-Output ""
Write-Output "2. OR use the already-patched LoaderVIP_cracked.exe"
Write-Output "   which skips the serial check entirely"
Write-Output ""
Write-Output "3. The loader downloads the encrypted DLL, decrypts it,"
Write-Output "   and injects it. You need to replace the decrypted DLL"
Write-Output "   with the cracked one BEFORE injection happens."
Write-Output "   (Or we can patch the loader to write the cracked DLL)"