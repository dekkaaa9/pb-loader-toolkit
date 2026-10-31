# Decrypt the SIZE and INVALID strings and find their xrefs
# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$bytes = [System.IO.File]::ReadAllBytes('C:\Users\PIXWAR\AppData\Local\Temp\LoaderTrialTemp\LoaderVIP.exe')

function VaToFO($va) {
    $rva = $va - 0x400000
    if ($rva -ge 0x1000 -and $rva -lt 0xC000) { return 0x400 + ($rva - 0x1000) }
    if ($rva -ge 0xE000 -and $rva -lt 0x12000) { return 0xD400 + ($rva - 0xE000) }
    if ($rva -ge 0x12000 -and $rva -lt 0x16000) { return 0x10E00 + ($rva - 0x12000) }
    return $null
}

function FOToVA($fo) {
    if ($fo -ge 0x400 -and $fo -lt 0xC400) { return 0x400000 + 0x1000 + ($fo - 0x400) }
    if ($fo -ge 0xD400 -and $fo -lt 0x10E00) { return 0x400000 + 0xE000 + ($fo - 0xD400) }
    if ($fo -ge 0x10E00 -and $fo -lt 0x16000) { return 0x400000 + 0x12000 + ($fo - 0x10E00) }
    return $null
}

# Decrypt with incrementing XOR key
function DecryptInc($fo, $startKey, $len) {
    $result = ""
    for ($i = 0; $i -lt $len; $i++) {
        $c = $bytes[$fo + $i] -bxor (($startKey + $i) -band 0xFF)
        if ($c -ge 32 -and $c -le 126) { $result += [char]$c }
        else { break }
    }
    return $result
}

Write-Output "=== DECRYPTED STRINGS ==="
Write-Output ""

# SIZE string at FO 0xDB1A with incKey 0x36
Write-Output "FO 0xDB1A incKey=0x36: '$(DecryptInc 0xDB1A 0x36 50)'"
# Try different start positions for SIZE
for ($off = 0xDB10; $off -le 0xDB30; $off++) {
    for ($k = 0x30; $k -le 0x40; $k++) {
        $s = DecryptInc $off $k 60
        if ($s -match "SIZE|size") {
            Write-Output "  FO 0x$($off.ToString('X')) key=0x$($k.ToString('X2')): '$s'"
        }
    }
}

Write-Output ""
# INVALID string at FO 0xDBD1 with incKey 0xAE
for ($off = 0xDBC0; $off -le 0xDBE0; $off++) {
    for ($k = 0xA0; $k -le 0xB5; $k++) {
        $s = DecryptInc $off $k 60
        if ($s -match "INVALID|invalid|Invalid") {
            Write-Output "  FO 0x$($off.ToString('X')) key=0x$($k.ToString('X2')): '$s'"
        }
    }
}

Write-Output ""
Write-Output "=== SEARCH FOR MORE ERROR STRINGS ==="
# Broader search for error-related strings with incrementing XOR
for ($fo = 0xD400; $fo -lt 0x10E00 - 10; $fo++) {
    for ($k = 1; $k -lt 256; $k++) {
        $s = DecryptInc $fo $k 80
        if ($s.Length -ge 8) {
            $lower = $s.ToLower()
            if ($lower -match "size error|wrong size|file size|invalid|expired|license|not valid|failed|error") {
                Write-Output "  FO 0x$($fo.ToString('X')) key=0x$($k.ToString('X2')): '$s'"
            }
        }
    }
}