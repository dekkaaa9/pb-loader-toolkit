# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$bytes = [System.IO.File]::ReadAllBytes('D:\PB_Sumatra\decrypted_payload.dll')

# .rdata: VA=0x9A000, Raw=0x99200
function VaToFO($va) {
    $rva = $va - 0x10000000
    if ($rva -ge 0x9A000 -and $rva -lt 0xB1CBE) { return 0x99200 + ($rva - 0x9A000) }
    if ($rva -ge 0x1000 -and $rva -lt 0x99CA4) { return 0x400 + ($rva - 0x1000) }
    return $null
}

function DecryptXor($va, $len, $startKey) {
    $fo = VaToFO $va
    if ($fo -eq $null) { return "NOT FOUND" }
    $result = ''
    $key = $startKey
    for ($i = 0; $i -lt $len; $i++) {
        $b = $bytes[$fo + $i]
        $result += [char]($b -bxor ($key -band 0xFF))
        $key = ($key + 1) -band 0xFF
    }
    return $result
}

Write-Output "=== DECRYPTED STRINGS FROM CHEAT DLL ==="
Write-Output ""

# From FUN_10036c00 (validation gate)
Write-Output "FUN_10036c00 string (key 0x4a, 25 bytes): '$(DecryptXor 0x1009C0C0 25 0x4A)'"

# From FUN_100499f0 (main thread)
Write-Output "main: DAT_1009c120 (key 0x9d, 26 bytes): '$(DecryptXor 0x1009C120 26 0x9D)'"
Write-Output "main: DAT_1009c13c (key 0xa0, 24 bytes): '$(DecryptXor 0x1009C13C 24 0xA0)'"
Write-Output "main: DAT_1009c158 (key 0xce, 31 bytes): '$(DecryptXor 0x1009C158 31 0xCE)'"

# Plaintext strings we found
Write-Output ""
Write-Output "=== PLAINTEXT STRINGS ==="
$plain = @(
    @{ VA=0x1009C10C; Label="empty/single char" },
    @{ VA=0x100B5700; Label="path/url part" },
    @{ VA=0x100B4B00; Label="another path" }
)
foreach ($p in $plain) {
    $fo = VaToFO $p.VA
    $str = ''
    for ($i = 0; $i -lt 80; $i++) {
        $b = $bytes[$fo + $i]
        if ($b -eq 0) { break }
        $str += [char]$b
    }
    Write-Output "$($p.Label) @ 0x$($p.VA.ToString('X')): '$str'"
}

# Look for the time format string and other interesting strings
Write-Output ""
Write-Output "=== SEARCHING FOR TIME/TRIAL STRINGS ==="
$searchTerms = @('%d:%d:%d', '%d-%d-%d', '%2x%2x', 'Satu', 'Dua', 'sumatra', 'PointBl', 'pointbl', 'expired', 'trial', 'habis', 'waktu', 'menit', 'detik')
foreach ($term in $searchTerms) {
    $tb = [System.Text.Encoding]::ASCII.GetBytes($term)
    $found = @()
    for ($i = 0; $i -lt $bytes.Length - $tb.Length; $i++) {
        $match = $true
        for ($j = 0; $j -lt $tb.Length; $j++) {
            if ($bytes[$i + $j] -ne $tb[$j]) { $match = $false; break }
        }
        if ($match) { $found += $i }
    }
    if ($found.Count -gt 0) {
        foreach ($fo in $found) {
            $va = $null
            $rva = $fo
            # .rdata
            if ($fo -ge 0x99200 -and $fo -lt 0xB1000) { $va = 0x10000000 + 0x9A000 + ($fo - 0x99200) }
            # .text
            if ($fo -ge 0x400 -and $fo -lt 0x99200) { $va = 0x10000000 + 0x1000 + ($fo - 0x400) }
            # .data
            if ($fo -ge 0xB1000 -and $fo -lt 0xB3800) { $va = 0x10000000 + 0xB2000 + ($fo - 0xB1000) }
            if ($va) {
                Write-Output "  '$term' @ VA 0x$($va.ToString('X')) (FO 0x$($fo.ToString('X'))"
            }
        }
    }
}