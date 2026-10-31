# Decrypt strings in LoaderVIP.exe to find error messages
# The loader uses XOR encryption for strings
# From analysis: .rdata VA=0xE000 Raw=0xD400, .data VA=0x12000 Raw=0x10E00
# Key strings found: DAT_0040e7fc (XOR key 0xa3), DAT_0040e77c (XOR key 0x17)

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$bytes = [System.IO.File]::ReadAllBytes('C:\Users\PIXWAR\AppData\Local\Temp\LoaderTrialTemp\LoaderVIP.exe')

function VaToFO($va) {
    $rva = $va - 0x400000
    # .text: VA=0x1000 Raw=0x400
    if ($rva -ge 0x1000 -and $rva -lt 0xC000) { return 0x400 + ($rva - 0x1000) }
    # .rdata: VA=0xE000 Raw=0xD400
    if ($rva -ge 0xE000 -and $rva -lt 0x12000) { return 0xD400 + ($rva - 0xE000) }
    # .data: VA=0x12000 Raw=0x10E00
    if ($rva -ge 0x12000 -and $rva -lt 0x16000) { return 0x10E00 + ($rva - 0x12000) }
    return $null
}

function DecryptXor($va, $len, $key) {
    $fo = VaToFO $va
    if ($fo -eq $null) { return "NOT FOUND" }
    $result = New-Object char[] $len
    for ($i = 0; $i -lt $len; $i++) {
        $result[$i] = [char]($bytes[$fo + $i] -bxor ([byte](($key + $i) -band 0xFF)))
    }
    return -join $result
}

# Also try simple XOR with constant key
function DecryptXorConst($va, $len, $key) {
    $fo = VaToFO $va
    if ($fo -eq $null) { return "NOT FOUND" }
    $result = New-Object char[] $len
    for ($i = 0; $i -lt $len; $i++) {
        $result[$i] = [char]($bytes[$fo + $i] -bxor ([byte]$key))
    }
    return -join $result
}

Write-Output "=== SEARCHING FOR 'size' AND 'error' STRINGS ==="
Write-Output ""

# Search for encrypted strings that might contain "size" or "error"
# Try XOR with common keys (0xa3, 0x17, etc.)
$keys = @(0xa3, 0x17, 0x00, 0x55, 0xAA, 0x42, 0x69)

# Search .rdata and .data for strings
for ($section = 0; $section -lt 2; $section++) {
    if ($section -eq 0) { $start = 0xD400; $end = 0x10E00; $name = ".rdata" }
    else { $start = 0x10E00; $end = [Math]::Min(0x16000, $bytes.Length); $name = ".data" }
    
    for ($fo = $start; $fo -lt $end - 20; $fo++) {
        foreach ($k in $keys) {
            # Try to decrypt 30 bytes with constant XOR key
            $str = ""
            $valid = $true
            for ($i = 0; $i -lt 30; $i++) {
                $c = $bytes[$fo + $i] -bxor $k
                if ($c -ge 32 -and $c -le 126) { $str += [char]$c }
                elseif ($c -eq 0) { break }
                else { $valid = $false; break }
            }
            if ($valid -and $str.Length -ge 5) {
                $lower = $str.ToLower()
                if ($lower -match "size|error|fail|invalid|wrong|expired|license|serial|key") {
                    Write-Output "  FO 0x$($fo.ToString('X')) ($name) key=0x$($k.ToString('X2')): $str"
                }
            }
        }
    }
}

Write-Output ""
Write-Output "=== SEARCHING FOR INCREMENTING XOR KEY STRINGS ==="
# Try incrementing XOR key (key + i) for various starting keys
for ($fo = 0xD400; $fo -lt 0x10E00 - 30; $fo++) {
    for ($startKey = 0; $startKey -lt 256; $startKey++) {
        $str = ""
        $valid = $true
        for ($i = 0; $i -lt 40; $i++) {
            $c = $bytes[$fo + $i] -bxor (($startKey + $i) -band 0xFF)
            if ($c -ge 32 -and $c -le 126) { $str += [char]$c }
            elseif ($c -eq 0) { break }
            else { $valid = $false; break }
        }
        if ($valid -and $str.Length -ge 5) {
            $lower = $str.ToLower()
            if ($lower -match "size|error|fail|invalid|wrong|expired|license|serial|key|download") {
                Write-Output "  FO 0x$($fo.ToString('X')) (.rdata) incKey=0x$($startKey.ToString('X2')): $str"
            }
        }
    }
}