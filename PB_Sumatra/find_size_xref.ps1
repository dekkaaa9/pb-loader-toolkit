# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$bytes = [System.IO.File]::ReadAllBytes('C:\Users\PIXWAR\AppData\Local\Temp\LoaderTrialTemp\LoaderVIP.exe')

function FOToVA($fo) {
    if ($fo -ge 0x400 -and $fo -lt 0xC400) { return 0x400000 + 0x1000 + ($fo - 0x400) }
    if ($fo -ge 0xD400 -and $fo -lt 0x10E00) { return 0x400000 + 0xE000 + ($fo - 0xD400) }
    return $null
}

# Decrypt full SIZE string
function DecryptInc($fo, $startKey, $len) {
    $result = ""
    for ($i = 0; $i -lt $len; $i++) {
        $c = $bytes[$fo + $i] -bxor (($startKey + $i) -band 0xFF)
        if ($c -ge 32 -and $c -le 126) { $result += [char]$c }
        elseif ($c -eq 0) { $result += '\0'; break }
        else { break }
    }
    return $result
}

Write-Output "=== FULL STRINGS ==="
Write-Output "SIZE (FO 0xDB1C, key 0x38): '$(DecryptInc 0xDB1C 0x38 60)'"
Write-Output "TooManyReq (FO 0xDBC0, key 0x7D): '$(DecryptInc 0xDBC0 0x7D 60)'"
Write-Output "InvalidLink (FO 0xDBD1, key 0x8E): '$(DecryptInc 0xDBD1 0x8E 60)'"

# Also decrypt around SIZE with more context
Write-Output ""
Write-Output "=== CONTEXT AROUND SIZE STRING ==="
for ($off = 0xDB00; $off -le 0xDB40; $off++) {
    $s = DecryptInc $off 0x38 40
    if ($s.Length -ge 3) {
        Write-Output "  FO 0x$($off.ToString('X')) key=0x38: '$s'"
    }
}

# Search for xrefs to SIZE string VA 0x40E71C
$sizeVA = FOToVA 0xDB1C
Write-Output ""
Write-Output "=== XREFS TO SIZE STRING (VA 0x$($sizeVA.ToString('X'))) ==="
$vaBytes = [BitConverter]::GetBytes([int]$sizeVA)
Write-Output "  Searching for bytes: $(($vaBytes | ForEach-Object { $_.ToString('X2') }) -join ' ')"
$found = @()
for ($fo = 0x400; $fo -lt 0xC400 - 4; $fo++) {
    if ($bytes[$fo] -eq $vaBytes[0] -and $bytes[$fo+1] -eq $vaBytes[1] -and $bytes[$fo+2] -eq $vaBytes[2] -and $bytes[$fo+3] -eq $vaBytes[3]) {
        $va = FOToVA $fo
        $found += $va
        Write-Output "  Found at FO 0x$($fo.ToString('X')) (VA 0x$($va.ToString('X')))"
    }
}

# Search for xrefs to "Too many request/Invalid Link" VA
$tmrVA = FOToVA 0xDBC0
Write-Output ""
Write-Output "=== XREFS TO 'Too many request' STRING (VA 0x$($tmrVA.ToString('X'))) ==="
$vaBytes2 = [BitConverter]::GetBytes([int]$tmrVA)
for ($fo = 0x400; $fo -lt 0xC400 - 4; $fo++) {
    if ($bytes[$fo] -eq $vaBytes2[0] -and $bytes[$fo+1] -eq $vaBytes2[1] -and $bytes[$fo+2] -eq $vaBytes2[2] -and $bytes[$fo+3] -eq $vaBytes2[3]) {
        $va = FOToVA $fo
        Write-Output "  Found at FO 0x$($fo.ToString('X')) (VA 0x$($va.ToString('X')))"
    }
}

# Dump code around each xref to SIZE
Write-Output ""
Write-Output "=== CODE AROUND SIZE XREFS ==="
foreach ($va in $found) {
    $fo = 0x400 + ($va - 0x401000)
    Write-Output ""
    Write-Output "--- Around VA 0x$($va.ToString('X')) (FO 0x$($fo.ToString('X'))) ---"
    $start = [Math]::Max(0x400, $fo - 32)
    for ($i = $start; $i -lt $fo + 16; $i++) {
        if (($i - $start) % 16 -eq 0) { Write-Output ""; Write-Output ("FO 0x{0:X}: " -f $i) -NoNewline }
        Write-Output ("{0:X2} " -f $bytes[$i]) -NoNewline
    }
    Write-Output ""
}