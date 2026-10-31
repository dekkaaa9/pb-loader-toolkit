# ============================================================
# PATCH DLL PERMANEN - decrypted_payload_cracked.dll
# PATCH A: FO 0x05F5E4 (RVA 0x0601E4)  74 EA             -> 90 90
#          init thread wait-loop-2: call[vtable+0xA8]==0 -> jz loop. NOP = keluar loop -> init-complete=1
# PATCH B: FO 0x05E82C (RVA 0x05F42C)  0F 84 DC 01 00 00 -> 90 90 90 90 90 90
#          render chain FUN_1005f310: call[vtable+0xA8]==0 -> jz bail. NOP = menu tetap render
# PATCH C: FO 0x05E2FE (RVA 0x05EEFE)  0F 84 71 01 00 00 -> 90 90 90 90 90 90
#          FUN_1005eec0 GATE 1: call[vtable+0x9C]==0 -> jz return. NOP = lanjut ke FUN_10035bc0 (menu render)
# PATCH D: FO 0x05D82A (RVA 0x05E42A)  75 10             -> EB 10
#          FUN_1005e3f0: jnz->jmp, SKIP refresh menu-visible dari method_0x20 (return 0)
# PATCH E: FO 0x035830 (RVA 0x036430)  75 12             -> EB 12
#          FUN_10036420: jnz->jmp, SKIP refresh menu-visible dari method_0x20 (return 0)
# PATCH F: FO 0x05F65D (RVA 0x06025D)  75 B1             -> 90 90
#          init thread wait-loop-3 (state 2/4/5): jnz loop. NOP = init thread lanjut kelar
# PATCH G: FO 0x031CE5 (RVA 0x0328E5)  00 00 00 00       -> 01 00 00 00
#          init: mov [menu-visible], 0 -> mov [menu-visible], 1 (menu langsung visible)
# Backup: decrypted_payload_cracked.dll.bak
# ============================================================

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9

# ---- portable paths: repo root auto-detected from script location ----
$Root = $PSScriptRoot
# ------------------------------------------------------------
$dllPath = (Join-Path $Root 'decrypted_payload_cracked.dll')
$bakPath = (Join-Path $Root 'decrypted_payload_cracked.dll.bak')

$patches = @(
    @{ fo = 0x05F5E4; orig = @(0x74,0xEA);                    patched = @(0x90,0x90);                   desc = 'PATCH A init-thread wait-loop-2' },
    @{ fo = 0x05E82C; orig = @(0x0F,0x84,0xDC,0x01,0x00,0x00); patched = @(0x90,0x90,0x90,0x90,0x90,0x90); desc = 'PATCH B render-chain 0xA8 gate' },
    @{ fo = 0x05E2FE; orig = @(0x0F,0x84,0x71,0x01,0x00,0x00); patched = @(0x90,0x90,0x90,0x90,0x90,0x90); desc = 'PATCH C FUN_1005eec0 GATE1 0x9C' },
    @{ fo = 0x05D82A; orig = @(0x75,0x10);                    patched = @(0xEB,0x10);                   desc = 'PATCH D FUN_1005e3f0 skip refresh' },
    @{ fo = 0x035830; orig = @(0x75,0x12);                    patched = @(0xEB,0x12);                   desc = 'PATCH E FUN_10036420 skip refresh' },
    @{ fo = 0x05F65D; orig = @(0x75,0xB1);                    patched = @(0x90,0x90);                   desc = 'PATCH F init-thread wait-loop-3' },
    @{ fo = 0x031CE5; orig = @(0x00,0x00,0x00,0x00);          patched = @(0x01,0x00,0x00,0x00);         desc = 'PATCH G init menu-visible 0->1' }
)

$dll = [System.IO.File]::ReadAllBytes($dllPath)
Write-Host "DLL: $dllPath ($($dll.Length) bytes)"

$needWrite = $false
foreach ($p in $patches) {
    $fo = $p['fo']
    $live = $dll[$fo..($fo + $p['patched'].Length - 1)]
    $liveHex = ($live | ForEach-Object { $_.ToString('X2') }) -join ' '
    $origHex = ($p['orig'] | ForEach-Object { $_.ToString('X2') }) -join ' '
    $newHex  = ($p['patched'] | ForEach-Object { $_.ToString('X2') }) -join ' '
    Write-Host ("  {0} FO 0x{1:X6}: live=[{2}] orig=[{3}] new=[{4}]" -f $p['desc'], $fo, $liveHex, $origHex, $newHex)

    $isOrig = $true; $isNew = $true
    for ($k = 0; $k -lt $p['orig'].Length; $k++) {
        if ($live[$k] -ne $p['orig'][$k]) { $isOrig = $false }
        if ($live[$k] -ne $p['patched'][$k]) { $isNew = $false }
    }
    if ($isNew) { Write-Host "    -> sudah dipatch, skip"; continue }
    if (-not $isOrig) { Write-Host "    -> WARNING: byte bukan orig maupun patched! skip demi aman"; continue }

    for ($k = 0; $k -lt $p['patched'].Length; $k++) { $dll[$fo + $k] = $p['patched'][$k] }
    $needWrite = $true
    Write-Host "    -> PATCHED"
}

if ($needWrite) {
    if (-not (Test-Path $bakPath)) {
        Copy-Item $dllPath $bakPath
        Write-Host "Backup: $bakPath"
    }
    [System.IO.File]::WriteAllBytes($dllPath, $dll)
    Write-Host "SAVED: $dllPath"

    # verifikasi ulang dari disk
    $chk = [System.IO.File]::ReadAllBytes($dllPath)
    $allOk = $true
    foreach ($p in $patches) {
        $fo = $p['fo']
        for ($k = 0; $k -lt $p['patched'].Length; $k++) {
            if ($chk[$fo + $k] -ne $p['patched'][$k]) { $allOk = $false }
        }
    }
    Write-Host "VERIFY FROM DISK: $allOk"
} else {
    Write-Host "Tidak ada yang diubah (semua sudah dipatch)."
}
Write-Host "SELESAI"