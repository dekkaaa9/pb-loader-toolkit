# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9

# ---- portable paths: repo root auto-detected from script location ----
$Sumatra = $PSScriptRoot
$Pbct    = Split-Path $PSScriptRoot -Parent
# ------------------------------------------------------------
$encrypted = [System.IO.File]::ReadAllBytes((Join-Path $Sumatra 'downloaded_payload.bin'))
$key = [System.Text.Encoding]::ASCII.GetBytes('325768235246856876125')
$decrypted = New-Object byte[] $encrypted.Length

for ($i = 0; $i -lt $encrypted.Length; $i++) {
    $decrypted[$i] = $encrypted[$i] -bxor $key[$i % $key.Length]
}

# Check if PE
if ($decrypted[0] -eq 0x4D -and $decrypted[1] -eq 0x5A) {
    Write-Output "SUCCESS: Decrypted file is a PE executable!"
    Write-Output "First 64 bytes:"
    $hex = ($decrypted[0..63] | ForEach-Object { $_.ToString('X2') }) -join ' '
    Write-Output "  $hex"
    
    # Save
    [System.IO.File]::WriteAllBytes((Join-Path $Sumatra 'decrypted_payload.dll'), $decrypted)
    Write-Output ""
    Write-Output "Saved to: $Sumatra\decrypted_payload.dll"
    Write-Output "Size: $($decrypted.Length) bytes"
    
    # Get PE info
    $peOffset = [BitConverter]::ToInt32($decrypted, 0x3C)
    Write-Output "PE offset: 0x$($peOffset.ToString('X'))"
    $machine = [BitConverter]::ToUInt16($decrypted, $peOffset + 4)
    Write-Output "Machine: 0x$($machine.ToString('X')) $(if($machine -eq 0x14C){'(x86)'}elseif($machine -eq 0x8664){'(x64)'}else{''})"
    $numSections = [BitConverter]::ToUInt16($decrypted, $peOffset + 6)
    Write-Output "Sections: $numSections"
    
    # Check if .NET
    $optOffset = $peOffset + 24
    $magic = [BitConverter]::ToUInt16($decrypted, $optOffset)
    Write-Output "Optional header magic: 0x$($magic.ToString('X')) $(if($magic -eq 0x10b){'(PE32)'}elseif($magic -eq 0x20b){'(PE32+)'})"
    
    # Data directories - check for CLI header (index 14)
    $dataDirOffset = if ($magic -eq 0x10b) { $optOffset + 96 } else { $optOffset + 112 }
    $cliRVA = [BitConverter]::ToUInt32($decrypted, $dataDirOffset + (14 * 8))
    $cliSize = [BitConverter]::ToUInt32($decrypted, $dataDirOffset + (14 * 8) + 4)
    if ($cliRVA -ne 0) {
        Write-Output "*** .NET ASSEMBLY (CLI header at RVA 0x$($cliRVA.ToString('X')), size $cliSize) ***"
    } else {
        Write-Output "Native PE (not .NET)"
    }
} else {
    Write-Output "FAILED: Decrypted file is not PE"
    Write-Output "First 32 bytes: $(($decrypted[0..31] | ForEach-Object { $_.ToString('X2') }) -join ' ')"
    
    # Try ASCII view
    $ascii = ''
    for ($i = 0; $i -lt 64; $i++) {
        $b = $decrypted[$i]
        if ($b -ge 32 -and $b -lt 127) { $ascii += [char]$b } else { $ascii += '.' }
    }
    Write-Output "ASCII: $ascii"
}