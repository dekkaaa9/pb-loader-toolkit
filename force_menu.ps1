# ============================================================
# FORCE MENU PATCHER - patch runtime proses PB yang HIDUP
# PATCH A: RVA 0x0601E4  74 EA             -> 90 90            (init thread wait-loop-2 jz -> NOP)
# PATCH B: RVA 0x05F42C  0F 84 DC 01 00 00 -> 90 90 90 90 90 90 (render chain 0xA8 gate jz -> NOP)
# PATCH C: RVA 0x05EEFE  0F 84 71 01 00 00 -> 90 90 90 90 90 90 (FUN_1005eec0 GATE 1: 0x9C cookie jz -> NOP)
# PATCH D: RVA 0x05E42A  75 10             -> EB 10            (FUN_1005e3f0: jnz->jmp, SKIP refresh menu-visible)
# PATCH E: RVA 0x036430  75 12             -> EB 12            (FUN_10036420: jnz->jmp, SKIP refresh menu-visible)
# PATCH F: RVA 0x06025D  75 B1             -> 90 90            (init thread wait-loop-3 jnz -> NOP)
# + RUNTIME WRITE: menu-visible (RVA 0xB2498) = 1
# Pakai:  force_menu.ps1          = apply patch
#         force_menu.ps1 revert   = balikin byte asli
# Output: D:\pbct\logs\force_out.txt
# ============================================================
# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
param([string]$mode = 'apply')

$ErrorActionPreference = 'Continue'

# --- self elevate ---
$id = [System.Security.Principal.WindowsIdentity]::GetCurrent()
$pr = New-Object System.Security.Principal.WindowsPrincipal($id)
if (-not $pr.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Elevating..."
    Start-Process powershell -Verb RunAs -ArgumentList '-ExecutionPolicy','Bypass','-File',$MyInvocation.MyCommand.Path,$mode
    exit
}

$log = New-Object System.Collections.ArrayList
function L($m) {
    $line = "[$(Get-Date -Format 'HH:mm:ss.fff')] $m"
    [void]$log.Add($line)
    Write-Host $line
}

$src = @'
using System;
using System.Runtime.InteropServices;
public class PT8 {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", EntryPoint="ReadProcessMemory", SetLastError=true)]
    public static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out int read);
    [DllImport("kernel32.dll", EntryPoint="WriteProcessMemory", SetLastError=true)]
    public static extern bool WriteProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out int written);
    [DllImport("kernel32.dll", EntryPoint="VirtualProtectEx", SetLastError=true)]
    public static extern bool VirtualProtectEx(IntPtr h, IntPtr addr, IntPtr size, uint newProt, out uint oldProt);
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
    public const uint TH32CS_SNAPMODULE  = 0x00000008;
    public const uint TH32CS_SNAPMODULE32 = 0x00000010;
}
'@
Add-Type -TypeDefinition $src

L "=== FORCE MENU PATCHER (mode=$mode) ==="

# --- cari PB yang punya window game ---
$pb = $null
foreach ($p in @(Get-Process PointBlank -ErrorAction SilentlyContinue)) {
    if ($p.MainWindowTitle -eq 'Point Blank') { $pb = $p; break }
}
if ($null -eq $pb) {
    L "ERROR: proses PointBlank (window='Point Blank') TIDAK JALAN!"
    L "Start PB dulu, masuk lobby, inject DLL, baru jalankan script ini."
    $log | Out-File 'D:\pbct\logs\force_out.txt' -Encoding utf8
    exit
}
L "PB pid=$($pb.Id) window='$($pb.MainWindowTitle)'"

# --- cari base DLL cheat ---
$snap = [PT8]::CreateToolhelp32Snapshot(0x18, [uint32]$pb.Id)
if ($snap.ToInt64() -eq -1 -or $snap -eq [IntPtr]::Zero) {
    L "ERROR: module snapshot gagal"
    $log | Out-File 'D:\pbct\logs\force_out.txt' -Encoding utf8
    exit
}
$me = New-Object 'PT8+MODULEENTRY32'
$me.dwSize = [System.Runtime.InteropServices.Marshal]::SizeOf($me)
$dllBase = [IntPtr]::Zero
if ([PT8]::Module32First($snap, [ref]$me)) {
    do {
        if ($me.szModule -like '*decrypted_payload*' -or $me.modBaseSize -eq 851968) {
            $dllBase = $me.modBaseAddr
            L ("DLL cheat: {0} base=0x{1:X8} size={2}" -f $me.szModule, $dllBase, $me.modBaseSize)
        }
    } while ([PT8]::Module32Next($snap, [ref]$me))
}
[PT8]::CloseHandle($snap) | Out-Null
if ($dllBase -eq [IntPtr]::Zero) {
    L "ERROR: DLL cheat TIDAK KE-LOAD di proses PB!"
    L "Jalankan inject_v2.ps1 dulu (PB harus di lobby)."
    $log | Out-File 'D:\pbct\logs\force_out.txt' -Encoding utf8
    exit
}

$h = [PT8]::OpenProcess(0x1F0FFF, $false, [uint32]$pb.Id)
if ($h -eq [IntPtr]::Zero) {
    L ("ERROR: OpenProcess denied err={0}" -f [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    $log | Out-File 'D:\pbct\logs\force_out.txt' -Encoding utf8
    exit
}
L ("OpenProcess OK h={0}" -f $h)

# --- definisi patch ---
$patches = @(
    @{ rva = 0x0601E4; orig = @(0x74,0xEA);                    patched = @(0x90,0x90);                   desc = 'PATCH A init-thread wait-loop-2 jz' },
    @{ rva = 0x05F42C; orig = @(0x0F,0x84,0xDC,0x01,0x00,0x00); patched = @(0x90,0x90,0x90,0x90,0x90,0x90); desc = 'PATCH B render-chain 0xA8 gate jz' },
    @{ rva = 0x05EEFE; orig = @(0x0F,0x84,0x71,0x01,0x00,0x00); patched = @(0x90,0x90,0x90,0x90,0x90,0x90); desc = 'PATCH C FUN_1005eec0 GATE1 0x9C jz' },
    @{ rva = 0x05E42A; orig = @(0x75,0x10);                    patched = @(0xEB,0x10);                   desc = 'PATCH D FUN_1005e3f0 skip refresh jnz->jmp' },
    @{ rva = 0x036430; orig = @(0x75,0x12);                    patched = @(0xEB,0x12);                   desc = 'PATCH E FUN_10036420 skip refresh jnz->jmp' },
    @{ rva = 0x06025D; orig = @(0x75,0xB1);                    patched = @(0x90,0x90);                   desc = 'PATCH F init-thread wait-loop-3 jnz' }
)

function ReadAt($addr, $len) {
    $buf = [byte[]]::new($len)
    $n = 0
    if ([PT8]::ReadProcessMemory($h, $addr, $buf, $len, [ref]$n)) { return $buf }
    return $null
}

function HexOf($b) {
    if ($null -eq $b) { return 'READ-FAIL' }
    return ($b | ForEach-Object { $_.ToString('X2') }) -join ' '
}

foreach ($p in $patches) {
    $addr = [IntPtr]($dllBase.ToInt64() + $p['rva'])
    $cur = ReadAt $addr $p['patched'].Length
    L ("  {0} @RVA 0x{1:X6} addr=0x{2:X8}: live=[{3}]" -f $p['desc'], $p['rva'], $addr, (HexOf $cur))
    if ($null -eq $cur) { continue }

    if ($mode -eq 'revert') { $target = $p['orig']; $tlabel = 'REVERT' }
    else { $target = $p['patched']; $tlabel = 'APPLY' }

    $same = $true
    for ($k = 0; $k -lt $target.Length; $k++) { if ($cur[$k] -ne $target[$k]) { $same = $false } }
    if ($same) {
        L ("    sudah kondisi $tlabel - skip")
        continue
    }

    $oldProt = 0
    if (-not [PT8]::VirtualProtectEx($h, $addr, [IntPtr]$target.Length, 0x40, [ref]$oldProt)) {
        L ("    VirtualProtectEx FAIL err={0}" -f [Runtime.InteropServices.Marshal]::GetLastWin32Error())
        continue
    }
    $written = 0
    if (-not [PT8]::WriteProcessMemory($h, $addr, $target, $target.Length, [ref]$written)) {
        L ("    WriteProcessMemory FAIL err={0}" -f [Runtime.InteropServices.Marshal]::GetLastWin32Error())
        continue
    }
    $dummy = 0
    [PT8]::VirtualProtectEx($h, $addr, [IntPtr]$target.Length, $oldProt, [ref]$dummy) | Out-Null

    $after = ReadAt $addr $target.Length
    L ("    $tlabel OK -> [{0}]" -f (HexOf $after))
}

# --- RUNTIME WRITE: paksa menu-visible (RVA 0xB2498) = 1 ---
if ($mode -ne 'revert') {
    $mvAddr = [IntPtr]($dllBase.ToInt64() + 0xB2498)
    $mvCur = ReadAt $mvAddr 4
    L ("  menu-visible @RVA 0xB2498: live=0x{0:X8}" -f [BitConverter]::ToUInt32($mvCur, 0))
    $one = [byte[]]@(0x01, 0x00, 0x00, 0x00)
    $oldProt2 = 0
    if ([PT8]::VirtualProtectEx($h, $mvAddr, [IntPtr]4, 0x40, [ref]$oldProt2)) {
        $w2 = 0
        if ([PT8]::WriteProcessMemory($h, $mvAddr, $one, 4, [ref]$w2)) {
            $mvAfter = ReadAt $mvAddr 4
            L ("    WRITE menu-visible=1 OK -> 0x{0:X8}" -f [BitConverter]::ToUInt32($mvAfter, 0))
        } else {
            L ("    WRITE menu-visible FAIL err={0}" -f [Runtime.InteropServices.Marshal]::GetLastWin32Error())
        }
        $dummy2 = 0
        [PT8]::VirtualProtectEx($h, $mvAddr, [IntPtr]4, $oldProt2, [ref]$dummy2) | Out-Null
    } else {
        L ("    VirtualProtectEx(menu-visible) FAIL err={0}" -f [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    }
}

[PT8]::CloseHandle($h) | Out-Null
L ""
if ($mode -eq 'revert') {
    L "REVERT DONE - byte asli udah dikembalikan"
} else {
    L "PATCH APPLIED (A+B+C+D+E+F) + menu-visible=1!"
    L ">>> CEK LAYAR PB SEKARANG - menu cheat harusnya MUNCUL!"
    L ">>> INSERT tetap bisa dipakai buat toggle menu (handler VK 0x2E gak disentuh)"
    L ">>> kalau error/crash, balikin: powershell -ExecutionPolicy Bypass -File D:\pbct\force_menu.ps1 revert"
}
$log | Out-File 'D:\pbct\logs\force_out.txt' -Encoding utf8
L "SELESAI - D:\pbct\logs\force_out.txt"