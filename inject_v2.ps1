# ============================================================
# INJECTOR v2 - inject DLL ke proses PointBlank (32-bit target)
# FIX v2: [IntPtr]::Size (bukan Is32BitProcess yg ga eksis di .NET4),
#         try/catch relaunch + EXIT kalau gagal, GetExitCodeThread
#         buat tau LoadLibraryA return value
# Output: D:\pbct\logs\inject_out.txt
# ============================================================

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$ErrorActionPreference = 'Continue'

# --- DLL yang mau di-inject (ganti ke original buat tes kalau perlu) ---
$dllPath = 'D:\pbct\decrypted_payload_cracked.dll'

# --- cek 32-bit: PB itu x86, injector HARUS 32-bit ---
if ([IntPtr]::Size -eq 8) {
    Write-Host "Proses 64-bit terdeteksi - relaunch sebagai 32-bit + elevated..."
    $ps32 = "$env:SystemRoot\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
    if (-not (Test-Path $ps32)) { Write-Host "FATAL: $ps32 gak ada!"; exit }
    try {
        Start-Process $ps32 -Verb RunAs -ArgumentList '-ExecutionPolicy','Bypass','-File',$MyInvocation.MyCommand.Path
        Write-Host "Relaunch OK - proses 64-bit ini exit. Cek window baru (32-bit)."
    } catch {
        Write-Host "FATAL: relaunch GAGAL (UAC declined?): $_"
        Write-Host "JANGAN lanjut sebagai 64-bit - inject akan crash. Coba lagi, klik YES di UAC."
    }
    exit
}

# --- self elevate (kalau 32-bit tapi belum admin) ---
$id = [System.Security.Principal.WindowsIdentity]::GetCurrent()
$pr = New-Object System.Security.Principal.WindowsPrincipal($id)
if (-not $pr.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Elevating..."
    try {
        Start-Process powershell -Verb RunAs -ArgumentList '-ExecutionPolicy','Bypass','-File',$MyInvocation.MyCommand.Path
    } catch {
        Write-Host "FATAL: elevasi gagal: $_"
    }
    exit
}

$log = New-Object System.Collections.ArrayList
function L($msg) {
    $line = "[$(Get-Date -Format 'HH:mm:ss.fff')] $msg"
    [void]$log.Add($line)
    Write-Host $line
}

$src = @'
using System;
using System.Runtime.InteropServices;
public class INJ2 {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr VirtualAllocEx(IntPtr h, IntPtr addr, IntPtr size, uint type, uint prot);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool WriteProcessMemory(IntPtr h, IntPtr addr, byte[] buf, IntPtr size, out IntPtr written);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr CreateRemoteThread(IntPtr h, IntPtr attr, IntPtr stack, IntPtr start, IntPtr param, uint flags, out IntPtr tid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool GetExitCodeThread(IntPtr h, out uint code);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool VirtualFreeEx(IntPtr h, IntPtr addr, IntPtr size, uint type);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr GetModuleHandle(string name);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr GetProcAddress(IntPtr mod, string name);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool Module32First(IntPtr snap, ref MODULEENTRY32 me);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool Module32Next(IntPtr snap, ref MODULEENTRY32 me);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Ansi)]
    public struct MODULEENTRY32 {
        public uint dwSize; public uint th32ModuleID; public uint th32ProcessID;
        public uint GlblcntUsage; public uint ProccntUsage; public IntPtr modBaseAddr;
        public uint modBaseSize; public IntPtr hModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string szModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=260)] public string szExePath;
    }
}
'@
Add-Type -TypeDefinition $src

L "=== INJECTOR v2 - $dllPath -> PointBlank ==="
L "env: IntPtr.Size=$([IntPtr]::Size) (4=32bit OK) | PS=$($PSVersionTable.PSVersion) | elevated=OK"

if (-not (Test-Path $dllPath)) {
    L "FATAL: DLL gak ada: $dllPath"
    $log | Out-File 'D:\pbct\logs\inject_out.txt' -Encoding utf8
    exit
}
$md5 = (Get-FileHash $dllPath -Algorithm MD5).Hash
$fsz = (Get-Item $dllPath).Length
L "DLL: size=$fsz md5=$md5"

$pbs = @(Get-Process PointBlank -ErrorAction SilentlyContinue)
if ($pbs.Count -eq 0) { L "PB gak jalan!"; $log | Out-File 'D:\pbct\logs\inject_out.txt' -Encoding utf8; exit }

# --- pilih target: yang punya main window ---
$targets = @($pbs | Where-Object { $_.MainWindowTitle -ne '' })
if ($targets.Count -eq 0) {
    L "ga ada PB dgn window - inject ke SEMUA proses PB"
    $targets = $pbs
}
foreach ($pb in $pbs) {
    L "  kandidat: pid=$($pb.Id) window='$($pb.MainWindowTitle)' start=$($pb.StartTime.ToString('HH:mm:ss'))"
}

$dllBytes = [System.Text.Encoding]::ASCII.GetBytes($dllPath + [char]0)

foreach ($pb in $targets) {
    L ""
    L ">>> inject ke pid=$($pb.Id) window='$($pb.MainWindowTitle)'"
    $h = [INJ2]::OpenProcess(0x1F0FFF, $false, [uint32]$pb.Id)
    if ($h -eq [IntPtr]::Zero) {
        L "  OpenProcess DENIED err=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        continue
    }
    L "  OpenProcess(ALL) OK"

    # cek udah ke-load belom
    $snap = [INJ2]::CreateToolhelp32Snapshot(0x18, [uint32]$pb.Id)
    $me = New-Object 'INJ2+MODULEENTRY32'
    $me.dwSize = [System.Runtime.InteropServices.Marshal]::SizeOf($me)
    $already = $false
    if ([INJ2]::Module32First($snap, [ref]$me)) {
        do {
            if ($me.szModule -match '^\d+\.dll$' -or $me.szModule -like '*decrypted_payload*' -or $me.modBaseSize -eq 851968) { $already = $true; L "  sudah ada: $($me.szModule) @ 0x$($me.modBaseAddr.ToString('X8'))" }
        } while ([INJ2]::Module32Next($snap, [ref]$me))
    }
    [INJ2]::CloseHandle($snap) | Out-Null
    if ($already) { L "  SKIP (cheat DLL udah ada di proses ini)"; [INJ2]::CloseHandle($h) | Out-Null; continue }

    # alloc + write path
    $size = [IntPtr]$dllBytes.Length
    $mem = [INJ2]::VirtualAllocEx($h, [IntPtr]::Zero, $size, 0x3000, 0x04)
    if ($mem -eq [IntPtr]::Zero) {
        L "  VirtualAllocEx GAGAL err=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        [INJ2]::CloseHandle($h) | Out-Null; continue
    }
    $written = [IntPtr]::Zero
    if (-not [INJ2]::WriteProcessMemory($h, $mem, $dllBytes, $size, [ref]$written)) {
        L "  WriteProcessMemory GAGAL err=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        [INJ2]::VirtualFreeEx($h, $mem, [IntPtr]::Zero, 0x8000) | Out-Null
        [INJ2]::CloseHandle($h) | Out-Null; continue
    }
    L "  path ditulis @ 0x$($mem.ToString('X8')) ($($written.ToInt32()) bytes): '$dllPath'"

    # resolve LoadLibraryA dari kernel32 32-bit (shared di WOW64)
    $k32 = [INJ2]::GetModuleHandle("kernel32.dll")
    $lla = [INJ2]::GetProcAddress($k32, "LoadLibraryA")
    L "  kernel32=0x$($k32.ToString('X8')) LoadLibraryA=0x$($lla.ToString('X8'))"
    if ($lla -eq [IntPtr]::Zero) {
        L "  LoadLibraryA gak ketemu!"
        [INJ2]::VirtualFreeEx($h, $mem, [IntPtr]::Zero, 0x8000) | Out-Null
        [INJ2]::CloseHandle($h) | Out-Null; continue
    }

    # fire
    $tid = [IntPtr]::Zero
    $th = [INJ2]::CreateRemoteThread($h, [IntPtr]::Zero, [IntPtr]::Zero, $lla, $mem, 0, [ref]$tid)
    if ($th -eq [IntPtr]::Zero) {
        L "  CreateRemoteThread GAGAL err=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        [INJ2]::VirtualFreeEx($h, $mem, [IntPtr]::Zero, 0x8000) | Out-Null
        [INJ2]::CloseHandle($h) | Out-Null; continue
    }
    L "  remote thread tid=$($tid.ToInt32()) - waiting..."
    $wait = [INJ2]::WaitForSingleObject($th, 30000)
    $exitCode = 0
    [void][INJ2]::GetExitCodeThread($th, [ref]$exitCode)
    L "  wait=$wait exitcode=0x$($exitCode.ToString('X8'))"
    if ($exitCode -eq 0) {
        L "  >>> LoadLibraryA RETURN 0 = DLL GAGAL LOAD (DllMain nolak / dependency hilang / file ga kebaca)"
    } elseif ($exitCode -ge 0xC0000000) {
        L "  >>> THREAD CRASH (0x$($exitCode.ToString('X8'))) - kemungkinan injector 64-bit / address salah!"
    } else {
        L "  >>> LoadLibraryA SUKSES hmodule=0x$($exitCode.ToString('X8'))"
    }
    [INJ2]::CloseHandle($th) | Out-Null

    # verify
    Start-Sleep -Milliseconds 800
    $snap2 = [INJ2]::CreateToolhelp32Snapshot(0x18, [uint32]$pb.Id)
    $me2 = New-Object 'INJ2+MODULEENTRY32'
    $me2.dwSize = [System.Runtime.InteropServices.Marshal]::SizeOf($me2)
    $found = $null
    if ([INJ2]::Module32First($snap2, [ref]$me2)) {
        do {
            if ($me2.szModule -match '^\d+\.dll$') { $found = "$($me2.szModule) @ 0x$($me2.modBaseAddr.ToString('X8')) size=$($me2.modBaseSize)" }
        } while ([INJ2]::Module32Next($snap2, [ref]$me2))
    }
    [INJ2]::CloseHandle($snap2) | Out-Null
    if ($found) { L "  >>> MODULE LIST: $found" } else { L "  >>> MODULE LIST: DLL TETAP GAK ADA" }

    [INJ2]::VirtualFreeEx($h, $mem, [IntPtr]::Zero, 0x8000) | Out-Null
    [INJ2]::CloseHandle($h) | Out-Null
}

L ""
L "SELESAI - kalau exitcode=0xC0000005 -> injector masih 64-bit (UAC declined?)"
L "      - kalau exitcode=0 -> DLL nolak load (coba inject original buat tes)"
$log | Out-File 'D:\pbct\logs\inject_out.txt' -Encoding utf8