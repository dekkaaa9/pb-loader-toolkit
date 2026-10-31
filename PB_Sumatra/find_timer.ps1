# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$path = 'D:\PB_Sumatra\decrypted_payload.dll'
$bytes = [System.IO.File]::ReadAllBytes($path)

# PE info
$peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
$numSections = [BitConverter]::ToUInt16($bytes, $peOffset + 6)
$optHeaderSize = [BitConverter]::ToUInt16($bytes, $peOffset + 20)
$sectionStart = $peOffset + 24 + $optHeaderSize

Write-Output "=== PE SECTIONS ==="
$sections = @()
for ($i = 0; $i -lt $numSections; $i++) {
    $off = $sectionStart + ($i * 40)
    $name = [System.Text.Encoding]::ASCII.GetString($bytes, $off, 8).TrimEnd([char]0)
    $vsize = [BitConverter]::ToUInt32($bytes, $off + 8)
    $vaddr = [BitConverter]::ToUInt32($bytes, $off + 12)
    $rawsize = [BitConverter]::ToUInt32($bytes, $off + 16)
    $rawptr = [BitConverter]::ToUInt32($bytes, $off + 20)
    $sections += @{ Name=$name; VA=$vaddr; VS=$vsize; Raw=$rawptr; RS=$rawsize }
    Write-Output "  $name VA=0x$($vaddr.ToString('X8')) VS=0x$($vsize.ToString('X8')) Raw=0x$($rawptr.ToString('X8')) RS=0x$($rawsize.ToString('X8'))"
}

function VaToFO($va) {
    $rva = $va - 0x10000000
    foreach ($s in $sections) {
        if ($rva -ge $s.VA -and $rva -lt ($s.VA + $s.VS)) { return $s.Raw + ($rva - $s.VA) }
    }
    return $null
}

function SearchBytes($pattern, $label) {
    $found = @()
    $start = 0
    while ($true) {
        $idx = [Array]::IndexOf($bytes, $pattern[0], $start)
        if ($idx -lt 0 -or $idx -gt $bytes.Length - $pattern.Length) { break }
        $match = $true
        for ($i = 1; $i -lt $pattern.Length; $i++) {
            if ($bytes[$idx + $i] -ne $pattern[$i]) { $match = $false; break }
        }
        if ($match) { $found += $idx }
        $start = $idx + 1
    }
    Write-Output ""
    Write-Output "=== $label (found $($found.Count)) ==="
    foreach ($fo in $found) {
        # Convert FO back to VA
        $va = $null
        foreach ($s in $sections) {
            if ($fo -ge $s.Raw -and $fo -lt ($s.Raw + $s.RS)) {
                $va = 0x10000000 + $s.VA + ($fo - $s.Raw)
                break
            }
        }
        if ($va) {
            $ctx = ($bytes[$fo..([Math]::Min($fo+15, $bytes.Length-1))] | ForEach-Object { $_.ToString('X2') }) -join ' '
            Write-Output "  VA 0x$($va.ToString('X')) (FO 0x$($fo.ToString('X'))): $ctx"
        }
    }
}

# 30 minutes = 1800 seconds = 0x708
# cmp ecx, 0x708 = 81 F9 08 07 00 00
SearchBytes @(0x81,0xF9,0x08,0x07,0x00,0x00) "cmp ecx, 1800 (0x708)"
# cmp eax, 0x708 = 3D 08 07 00 00
SearchBytes @(0x3D,0x08,0x07,0x00,0x00) "cmp eax, 1800 (0x708)"
# cmp edx, 0x708 = 81 FA 08 07 00 00
SearchBytes @(0x81,0xFA,0x08,0x07,0x00,0x00) "cmp edx, 1800 (0x708)"
# cmp esi, 0x708 = 81 FE 08 07 00 00
SearchBytes @(0x81,0xFE,0x08,0x07,0x00,0x00) "cmp esi, 1800 (0x708)"
# cmp ebx, 0x708 = 81 FB 08 07 00 00
SearchBytes @(0x81,0xFB,0x08,0x07,0x00,0x00) "cmp ebx, 1800 (0x708)"

# 30 minutes in ms = 1800000 = 0x1B7740
# cmp ecx, 0x1B7740 = 81 F9 40 77 1B 00
SearchBytes @(0x81,0xF9,0x40,0x77,0x1B,0x00) "cmp ecx, 1800000 (0x1B7740)"
# cmp eax, 0x1B7740 = 3D 40 77 1B 00
SearchBytes @(0x3D,0x40,0x77,0x1B,0x00) "cmp eax, 1800000 (0x1B7740)"
# cmp edx, 0x1B7740 = 81 FA 40 77 1B 00
SearchBytes @(0x81,0xFA,0x40,0x77,0x1B,0x00) "cmp edx, 1800000 (0x1B7740)"

# 30 min in 100ns FILETIME units = 1800 * 10^7 = 18000000000 = 0x430E23400
# This is too large for 32-bit, so it would be in two parts
# Low: 0x0E23400, High: 0x4
# Search for the low part in add instructions: 05 00 34 0E 43 (add eax, 0x430E3400) - wait that's wrong
# 18000000000 = 0x4_30E2_3400... let me recalculate
# 18000000000 in hex: 18000000000 / 16 = 1125000000 r 0
# Actually: 18000000000 = 0x430E23400
# Low 32 bits: 0x30E23400, High: 0x4
# add eax, 0x30E23400 = 05 00 34 E2 30
SearchBytes @(0x05,0x00,0x34,0xE2,0x30) "add eax, 0x30E23400 (30min 100ns low)"
# adc ecx, 4 = 83 D1 04
SearchBytes @(0x83,0xD1,0x04) "adc ecx, 4 (30min 100ns high)"

# Also try 1800 as a 16-bit compare (cmp reg16, 0x708)
# 66 81 F9 08 07 = cmp cx, 0x708
SearchBytes @(0x66,0x81,0xF9,0x08,0x07) "cmp cx, 1800 (16-bit)"

# Try 1800000 as 16-bit won't work, but try smaller values
# 30 * 60 = 1800, but maybe it's stored as 30 * 60 * 1000 = 1800000
# Or maybe it uses 1800.0 as float? Unlikely.
# Let's also try 0x708 in various cmp forms
SearchBytes @(0x81,0xFD,0x08,0x07,0x00,0x00) "cmp ebp, 1800 (0x708)"

# Also search for 1800000 in other cmp registers
SearchBytes @(0x81,0xFB,0x40,0x77,0x1B,0x00) "cmp ebx, 1800000"
SearchBytes @(0x81,0xFE,0x40,0x77,0x1B,0x00) "cmp esi, 1800000"
SearchBytes @(0x81,0xFD,0x40,0x77,0x1B,0x00) "cmp ebp, 1800000"