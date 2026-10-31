# ============================================================
# ONE-CLICK CRACK - Sumatra PB Cheat
# Bypasses: serial key, VIP license, server validation
# Usage: Run this script, then start PointBlank
# ============================================================

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------

param(
    [string]$LoaderPath = (Join-Path $PSScriptRoot 'LoaderVIP_cracked.exe'),
    [string]$CrackedDllPath = (Join-Path (Split-Path $PSScriptRoot -Parent) 'decrypted_payload_cracked.dll')
)

# ---- portable paths: repo root auto-detected from script location ----
$Sumatra = $PSScriptRoot
$Pbct    = Split-Path $PSScriptRoot -Parent

$ErrorActionPreference = 'Stop'
$temp = $env:TEMP
$crackedDll = [System.IO.File]::ReadAllBytes($CrackedDllPath)
$crackedSize = $crackedDll.Length

Write-Output "=========================================="
Write-Output "  SUMATRA PB CHEAT - KEY CRACK"
Write-Output "=========================================="
Write-Output ""
Write-Output "[*] Cracked DLL size: $crackedSize bytes"
Write-Output "[*] Watching temp folder: $temp"
Write-Output ""

# Check if PointBlank is running - if so, warn
$pb = Get-Process -Name 'PointBlank' -ErrorAction SilentlyContinue
if ($pb) {
    Write-Output "[!] WARNING: PointBlank is already running!"
    Write-Output "[!] Close PointBlank first, then run this script again."
    exit 1
}

# Snapshot existing files before starting loader
Write-Output "[*] Snapshotting existing temp files..."
$existing = @{}
Get-ChildItem -Path $temp -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
    $existing[$_.FullName] = $_.Length
}

# Start the loader
Write-Output "[*] Starting loader: $LoaderPath"
$loader = Start-Process -FilePath $LoaderPath -PassThru
Write-Output "[*] Loader PID: $($loader.Id)"
Write-Output ""

# Watch for new PE files (the decrypted DLL)
Write-Output "[*] Waiting for loader to download and decrypt DLL..."
$replaced = @()
$timeout = 60 # seconds
$elapsed = 0

while ($elapsed -lt $timeout) {
    Start-Sleep -Milliseconds 500
    $elapsed += 0.5

    # Find new files that are PE and match size
    Get-ChildItem -Path $temp -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $f = $_
        # Skip if already existed
        if ($existing.ContainsKey($f.FullName)) { return }
        # Skip if already replaced
        if ($replaced -contains $f.FullName) { return }
        # Check size (allow some variance)
        if ([Math]::Abs($f.Length - $crackedSize) -gt 1024) { return }

        try {
            $fs = [System.IO.File]::OpenRead($f.FullName)
            $header = New-Object byte[] 2
            $fs.Read($header, 0, 2) | Out-Null
            $fs.Close()
            if ($header[0] -eq 0x4D -and $header[1] -eq 0x5A) {
                # This is the decrypted DLL - replace it!
                Write-Output "[+] FOUND DLL: $($f.FullName) ($($f.Length) bytes)"
                [System.IO.File]::WriteAllBytes($f.FullName, $crackedDll)
                Write-Output "[+] REPLACED with cracked DLL!"
                $replaced += $f.FullName
            }
        } catch {
            # File might be locked, try again next iteration
        }
    }

    if ($replaced.Count -gt 0) {
        # Wait a bit more for additional files
        Start-Sleep -Seconds 2
        # Check for more
        Get-ChildItem -Path $temp -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
            $f = $_
            if ($existing.ContainsKey($f.FullName)) { return }
            if ($replaced -contains $f.FullName) { return }
            if ([Math]::Abs($f.Length - $crackedSize) -gt 1024) { return }
            try {
                $fs = [System.IO.File]::OpenRead($f.FullName)
                $header = New-Object byte[] 2
                $fs.Read($header, 0, 2) | Out-Null
                $fs.Close()
                if ($header[0] -eq 0x4D -and $header[1] -eq 0x5A) {
                    Write-Output "[+] FOUND DLL: $($f.FullName)"
                    [System.IO.File]::WriteAllBytes($f.FullName, $crackedDll)
                    Write-Output "[+] REPLACED!"
                    $replaced += $f.FullName
                }
            } catch { }
        }
        break
    }

    # Show progress
    if ($elapsed % 5 -eq 0) {
        Write-Output "[*] Still waiting... ($elapsed s)"
    }
}

Write-Output ""
if ($replaced.Count -gt 0) {
    Write-Output "=========================================="
    Write-Output "  CRACK APPLIED SUCCESSFULLY!"
    Write-Output "=========================================="
    Write-Output "  Replaced $($replaced.Count) DLL file(s):"
    foreach ($r in $replaced) { Write-Output "    - $r" }
    Write-Output ""
    Write-Output "  >> NOW START POINTBLANK <<"
    Write-Output "  The loader will inject the CRACKED DLL."
    Write-Output "  Any key works. No license needed."
    Write-Output "=========================================="
} else {
    Write-Output "[!] TIMEOUT: No DLL found after $timeout seconds."
    Write-Output "[!] The loader might have failed. Check:"
    Write-Output "    - Is LoaderVIP_cracked.exe in the right path?"
    Write-Output "    - Is the server reachable?"
    Write-Output "    - Check the loader's console output."
}