# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$path = 'C:\Users\PIXWAR\AppData\Local\Temp\LoaderTrialTemp\LoaderVIP.exe'
$bytes = [System.IO.File]::ReadAllBytes($path)
$imageBase = 0x400000

# .rdata: VA=0xE000, Raw=0xD400
function VaToFO($va) {
    $rva = $va - $imageBase
    # .text
    if ($rva -ge 0x1000 -and $rva -lt 0xDE89) { return 0x400 + ($rva - 0x1000) }
    # .rdata
    if ($rva -ge 0xE000 -and $rva -lt 0x119F0) { return 0xD400 + ($rva - 0xE000) }
    # .data
    if ($rva -ge 0x12000 -and $rva -lt 0x16434) { return 0x10E00 + ($rva - 0x12000) }
    return $null
}

function DecryptString($va, $length, $startKey) {
    $fo = VaToFO $va
    if ($fo -eq $null) { return "NOT FOUND" }
    $result = ''
    $key = $startKey
    for ($i = 0; $i -lt $length; $i++) {
        $b = $bytes[$fo + $i]
        $result += [char]($b -bxor ($key -band 0xFF))
        $key = ($key + 1) -band 0xFF
    }
    return $result
}

Write-Output "=== DECRYPTED STRINGS FROM LoaderVIP.exe ==="
Write-Output ""

# From main function
$strings = @(
    @{ VA=0x40E76C; Len=14; Key=0x38; Label="main: CB.exe check" },
    @{ VA=0x40E77C; Len=26; Key=0x17; Label="main: banner/press key" },
    @{ VA=0x40E7C0; Len=29; Key=0x7D; Label="main: pause msg" },
    @{ VA=0x40E798; Len=23; Key=0xFF; Label="main: after keypress" },
    @{ VA=0x40E7B0; Len=13; Key=0xF7; Label="main: error msg" },
    @{ VA=0x40E7E0; Len=24; Key=0x58; Label="main: success msg" },
    @{ VA=0x40E7FC; Len=21; Key=0xA3; Label="main: injecting msg" },
    @{ VA=0x40E824; Len=18; Key=0xF0; Label="main: wait msg" },
    @{ VA=0x40E838; Len=23; Key=0xFC; Label="main: inject msg" },
    @{ VA=0x40E814; Len=15; Key=0x03; Label="main: file error" },
    # From FUN_004039f0 (serial validation)
    @{ VA=0x40E644; Len=17; Key=0x9D; Label="serial: expected string" },
    @{ VA=0x40E748; Len=13; Key=0x21; Label="serial: error msg" }
)

foreach ($s in $strings) {
    $decrypted = DecryptString $s.VA $s.Len $s.Key
    Write-Output "$($s.Label): '$decrypted'"
}

Write-Output ""
Write-Output "=== HARDCODED STRINGS (plaintext in .rdata) ==="
$plain = @(
    @{ VA=0x40E5A5; Label="key/salt 1" },
    @{ VA=0x40E5C9; Label="key/salt 2" },
    @{ VA=0x40E5E9; Label="key/salt 3" },
    @{ VA=0x40E490; Label="charset" },
    @{ VA=0x40E565; Label="digit charset" }
)
foreach ($p in $plain) {
    $fo = VaToFO $p.VA
    $str = ''
    for ($i = 0; $i -lt 40; $i++) {
        $b = $bytes[$fo + $i]
        if ($b -eq 0) { break }
        $str += [char]$b
    }
    Write-Output "$($p.Label) @ 0x$($p.VA.ToString('X')): '$str'"
}