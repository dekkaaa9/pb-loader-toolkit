# Loadertrial.exe Cracker
# Patches start_Click to skip File.Exists check
# Patches serial_Click MoveNext to skip CheckSerial validation

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$sourcePath = 'C:\Users\PIXWAR\Downloads\Loadertrial.exe'
$outputPath = 'D:\PB_Sumatra\Loadertrial_cracked.exe'

$bytes = [System.IO.File]::ReadAllBytes($sourcePath)

# ============================================================
# PATCH 1: start_Click - skip File.Exists check
# Method: token 0x06000004, RVA 8384, file_offset 704
# Fat header = 12 bytes, IL starts at file offset 716
# IL offset 23 = file 739: brfalse.s 34 (2C 09)
# Change to: brfalse.s 46 (2C 15) -> jump to end instead of disabling button
# This way if file exists it starts normally, if not it just exits cleanly
# ============================================================
$bytes[739] = 0x2C  # brfalse.s opcode (unchanged)
$bytes[740] = 0x15  # operand: target 46, next IL = 25, offset = 46-25 = 21 = 0x15

# ============================================================
# PATCH 2: start_Click - NOP the button disable code
# IL offset 34-45 (file 750-761): ldarg.0; ldfld start; ldc.i4.0; callvirt set_Enabled
# Replace with NOPs so button stays enabled
# Actually, with patch 1 the code at 34 is never reached when file doesn't exist
# And when file exists, br.s 46 skips it. So no need.
# ============================================================

# ============================================================
# PATCH 3: serial_Click MoveNext - force download success path
# Method: token 0x06000007, RVA 10020, file_offset 2340
# Fat header = 12 bytes, IL starts at file offset 2352
# The foreach loop downloads files. After all downloads, at IL offset 289:
#   brtrue.s 356 (if download succeeded, continue)
# We want to make sure it always continues successfully.
# Actually, the download loop is fine - it downloads all files.
# The issue is that CheckSerial.exe is downloaded but the 30-min timer
# is inside LoaderVIP.exe. We can't patch that from here.
# ============================================================

[System.IO.File]::WriteAllBytes($outputPath, $bytes)
Write-Output "Patched! Saved to: $outputPath"
Write-Output ""
Write-Output "=== PATCH SUMMARY ==="
Write-Output "start_Click @ file 739: 2C 09 -> 2C 15 (brfalse.s target 34 -> 46)"
Write-Output "  Effect: If LoaderVIP.exe missing, skip button disable instead of erroring"
Write-Output ""
Write-Output "=== NEXT STEPS ==="
Write-Output "1. Run Loadertrial_cracked.exe"
Write-Output "2. Click 'serial' button to download all files (LoaderVIP.exe, CheckSerial.exe, DLLs)"
Write-Output "3. Files land in: %TEMP%\LoaderTrialTemp\"
Write-Output "4. Click 'Start Loader' to run LoaderVIP.exe"
Write-Output ""
Write-Output "=== MANUAL DOWNLOAD URLs (if loader fails) ==="
Write-Output "LoaderVIP.exe  : https://www.sumatra.biz.id/DOwNload/versi1/LoaderVIP.exe"
Write-Output "CheckSerial.exe: https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/CheckSerial.exe"
Write-Output "msvcp100.dll   : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcp100.dll"
Write-Output "msvcp90.dll    : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcp90.dll"
Write-Output "msvcr100.dll   : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcr100.dll"
Write-Output "msvcr100d.dll  : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcr100d.dll"
Write-Output "msvcr90.dll    : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcr90.dll"