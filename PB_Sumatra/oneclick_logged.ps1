<#
====================================================================
 ONE-CLICK CRACK + FULL RUN LOG - Sumatra PB Cheat (cracked loader)
====================================================================
 Tujuan: tiap run menghasilkan log lengkap di:
   D:\PB_Sumatra\logs\run_YYYYMMDD_HHMMSS.log

 Log merekam:
   1. Status patch loader (CB.exe killswitch, serial, filesize)
   2. Hash + patch-bytes DLL cracked
   3. Elevation status (auto-relaunch sebagai ADMIN, log yg sama)
   4. Loader start/exit + capture stdout/stderr (mungkin kosong: GUI subsys)
   5. SEMUA proses baru (CB.exe, ZLauncher child, .ini PE kalau di-run)
   6. DLL drop di temp (path/size/hash) + REPLACE (before/after MD5)
   7. Trailer append / rewrite oleh loader (size tracking + isi trailer)
   8. PB start: PID, path, versi, window title
   9. Privilege test: OpenProcess(QUERY_LIMITED / QUERY_INFO / ALL_ACCESS)
  10. MODULE SNAPSHOT PB -> BUKTI INJECT atau TIDAK (definitif)
  11. Remote-desktop detector (UltraViewer/TeamViewer/AnyDesk)
  12. SUMMARY + VERDIK diagnosis di akhir log

 Cara pakai:
   powershell -ExecutionPolicy Bypass -File D:\PB_Sumatra\oneclick_logged.ps1
   (script auto-relaunch sebagai ADMIN kalau belum)
   Lalu start PointBlank. Biarkan sampai selesai / timeout.
====================================================================
#>

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
param(
    [string]$LoaderPath     = 'D:\PB_Sumatra\LoaderVIP_cracked.exe',
    [string]$CrackedDllPath = 'D:\pbct\decrypted_payload_cracked.dll',
    [int]$WatchSeconds      = 900,
    [string]$LogFile        = ''
)

$ErrorActionPreference = 'Continue'

# ================= LOGGING =================
if ([string]::IsNullOrEmpty($LogFile)) {
    $logDir = 'D:\PB_Sumatra\logs'
    if (-not (Test-Path -LiteralPath $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
    $LogFile = Join-Path $logDir ("run_{0:yyyyMMdd_HHmmss}.log" -f (Get-Date))
}

function Write-Log {
    param(
        [Parameter(Mandatory=$true)][string]$Message,
        [ValidateSet('INFO','OK','WARN','ERROR','KEY')][string]$Level = 'INFO'
    )
    $ts = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
    $line = "[$ts] [$($Level.PadRight(5))] $Message"
    switch ($Level) {
        'ERROR' { Write-Host $line -ForegroundColor Red }
        'WARN'  { Write-Host $line -ForegroundColor Yellow }
        'OK'    { Write-Host $line -ForegroundColor Green }
        'KEY'   { Write-Host $line -ForegroundColor Cyan }
        default { Write-Host $line }
    }
    try { Add-Content -LiteralPath $LogFile -Value $line -Force } catch {}
}

# ================= P/INVOKE =================
$pinvokeSrc = @'
using System;
using System.Runtime.InteropServices;
public static class PT {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr CreateToolhelp32Snapshot(uint dwFlags, uint th32ProcessID);
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern bool Module32First(IntPtr hSnapshot, ref MODULEENTRY32 lpme);
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern bool Module32Next(IntPtr hSnapshot, ref MODULEENTRY32 lpme);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool CloseHandle(IntPtr hObject);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, uint dwProcessId);
    [DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr FindWindowA(string lpClassName, string lpWindowName);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Auto)]
    public struct MODULEENTRY32 {
        public uint dwSize;
        public uint th32ModuleID;
        public uint th32ProcessID;
        public uint GlblcntUsage;
        public uint ProccntUsage;
        public IntPtr modBaseAddr;
        public uint modBaseSize;
        public IntPtr hModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)]
        public string szModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=260)]
        public string szExePath;
    }
    public const uint TH32CS_SNAPMODULE   = 0x00000008;
    public const uint TH32CS_SNAPMODULE32 = 0x00000010;
    public const uint PROCESS_QUERY_LIMITED_INFORMATION = 0x00001000;
    public const uint PROCESS_QUERY_INFORMATION        = 0x00000400;
    public const uint PROCESS_ALL_ACCESS               = 0x001F0FFF;
}
'@
try { Add-Type -TypeDefinition $pinvokeSrc } catch { Write-Log "Add-Type GAGAL: $_" 'ERROR' }

function Get-ProcModules {
    param([uint32]$procId)
    $snap = [PT]::CreateToolhelp32Snapshot([PT]::TH32CS_SNAPMODULE -bor [PT]::TH32CS_SNAPMODULE32, $procId)
    if ($snap -eq [IntPtr]::Zero -or $snap -eq -1) {
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "CreateToolhelp32Snapshot failed (Win32 err=$err)"
    }
    try {
        $me = New-Object PT+MODULEENTRY32
        $me.dwSize = [Runtime.InteropServices.Marshal]::SizeOf([PT+MODULEENTRY32])
        $mods = New-Object System.Collections.Generic.List[object]
        if ([PT]::Module32First($snap, [ref]$me)) {
            do {
                $mods.Add([pscustomobject]@{
                    Name = $me.szModule
                    Path = $me.szExePath
                    Base = $me.modBaseAddr
                    Size = $me.modBaseSize
                })
            } while ([PT]::Module32Next($snap, [ref]$me))
        } else {
            $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            if ($err -ne 0 -and $err -ne 18) { Write-Log "Module32First err=$err (18=ERROR_NO_MORE_FILES normal di akhir)" 'WARN' }
        }
        return $mods
    } finally {
        [PT]::CloseHandle($snap) | Out-Null
    }
}

function Test-OpenProc {
    param([uint32]$procId, [uint32]$Access)
    $h = [PT]::OpenProcess($Access, $false, $procId)
    if ($h -eq [IntPtr]::Zero) { return $false }
    [PT]::CloseHandle($h) | Out-Null
    return $true
}

function Test-OpenProcE {
    param([uint32]$procId, [uint32]$Access, [string]$Name)
    $h = [PT]::OpenProcess($Access, $false, $procId)
    if ($h -eq [IntPtr]::Zero) {
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Write-Log ("  OpenProcess {0} = DENIED (Win32 err={1})" -f $Name, $err) 'KEY'
        return $false
    }
    [PT]::CloseHandle($h) | Out-Null
    Write-Log ("  OpenProcess {0} = OK" -f $Name) 'KEY'
    return $true
}

function Test-WindowTitle {
    param([string]$Title)
    return ([PT]::FindWindowA($null, $Title) -ne [IntPtr]::Zero)
}

# ================= HELPERS =================
function Read-FileBytes {
    param([string]$Path, [int]$Offset, [int]$Count)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $fs.Seek($Offset, 'Begin') | Out-Null
        $buf = New-Object byte[] $Count
        $read = $fs.Read($buf, 0, $Count)
        if ($read -lt $Count) { return $buf[0..($read-1)] }
        return $buf
    } finally { $fs.Close() }
}

function Fmt-Hex { param([byte[]]$Bytes) return (($Bytes | ForEach-Object { $_.ToString('X2') }) -join ' ') }

function Test-DllCracked([byte[]]$b) {
    if ($null -eq $b -or $b.Length -lt 0x5EBC9) { return $false }
    return ($b[0x4A012] -eq 0xE9 -and $b[0x4A013] -eq 0xB2 -and $b[0x5EBC7] -eq 0x90 -and $b[0x5EBC8] -eq 0x90)
}

function Get-MD5Safe([string]$Path) {
    try { return (Get-FileHash -LiteralPath $Path -Algorithm MD5).Hash } catch { return '(locked/err)' }
}

# ================= ELEVATION =================
$wid = [Security.Principal.WindowsIdentity]::GetCurrent()
$prp = New-Object Security.Principal.WindowsPrincipal($wid)
$isAdmin = $prp.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-Log '=====================================================' 'KEY'
Write-Log '  SUMATRA PB CHEAT - LOGGED RUN' 'KEY'
Write-Log "  LogFile : $LogFile" 'KEY'
Write-Log "  User    : $($wid.Name)" 'KEY'
Write-Log "  Elevated: $isAdmin" 'KEY'
Write-Log '=====================================================' 'KEY'

if (-not $isAdmin) {
    Write-Log 'BELUM ADMIN. CB.exe (CheatBlocker) biasanya bikin PB elevated -> loader butuh ADMIN buat OpenProcess. Relaunch sebagai ADMIN (klik YES di UAC)...' 'WARN'
    $sp = $PSCommandPath
    if ([string]::IsNullOrEmpty($sp)) { $sp = $MyInvocation.MyCommand.Path }
    try {
        Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList @(
            '-ExecutionPolicy','Bypass','-NoProfile','-File',$sp,
            '-LogFile',$LogFile,
            '-LoaderPath',$LoaderPath,
            '-CrackedDllPath',$CrackedDllPath,
            '-WatchSeconds',"$WatchSeconds"
        )
        Write-Log 'Instance ADMIN sudah di-relaunch (log lanjut ke file yg sama). Window ini bisa ditutup.' 'OK'
    } catch {
        Write-Log "Relaunch admin GAGAL: $_" 'ERROR'
    }
    exit 0
}

# ================= STATIC CHECKS =================
Write-Log '--- STATIC CHECKS ---' 'KEY'

if (-not (Test-Path -LiteralPath $LoaderPath)) {
    Write-Log "Loader TIDAK ADA: $LoaderPath" 'ERROR'
    exit 1
}
if (-not (Test-Path -LiteralPath $CrackedDllPath)) {
    Write-Log "Cracked DLL TIDAK ADA: $CrackedDllPath" 'ERROR'
    exit 1
}

$lb = [System.IO.File]::ReadAllBytes($LoaderPath)
$cbBytes  = Fmt-Hex ($lb[0x66C5..0x66C6])
$serBytes = Fmt-Hex ($lb[0x2FA9..0x2FAE])
$fszBytes = Fmt-Hex ($lb[0x38CA..0x38CB])
Write-Log ("Loader MD5={0} size={1}" -f (Get-MD5Safe $LoaderPath), $lb.Length)
Write-Log ("  CB.exe killswitch FO 0x66C5 : {0}  (harusnya '90 90')" -f $cbBytes)  $(if ($cbBytes -eq '90 90') { 'OK' } else { 'WARN' })
Write-Log ("  Serial bypass   FO 0x2FA9 : {0}  (harusnya 90 x6)" -f $serBytes)     $(if ($serBytes -eq '90 90 90 90 90 90') { 'OK' } else { 'WARN' })
Write-Log ("  Filesize check  FO 0x38CA : {0}  (harusnya '90 90')" -f $fszBytes)   $(if ($fszBytes -eq '90 90') { 'OK' } else { 'WARN' })

$crackedDll = [System.IO.File]::ReadAllBytes($CrackedDllPath)
$crackedSize = $crackedDll.Length
Write-Log ("CrackedDLL MD5={0} size={1} patchOK={2}" -f (Get-MD5Safe $CrackedDllPath), $crackedSize, (Test-DllCracked $crackedDll))

# baseline self-test: validasi P/Invoke OpenProcess (self harusnya selalu OK)
$selfOk = Test-OpenProc ([uint32]$PID) ([uint32]0x001F0FFF)
Write-Log "Self-test OpenProcess(ALL_ACCESS) self pid=$PID : $(if($selfOk){'OK - P/Invoke valid'}else{'GAGAL - P/Invoke BUG, hasil test PB ga bisa dipercaya!'})" 'KEY'

$pbExisting = Get-Process -Name 'PointBlank' -ErrorAction SilentlyContinue
if ($pbExisting) {
    Write-Log ("PointBlank SUDAH jalan (PID {0}). TUTUP PB dulu, baru run script ini lagi." -f $pbExisting.Id) 'ERROR'
    exit 1
}

$temp = $env:TEMP
Write-Log "Watch temp: $temp (timeout ${WatchSeconds}s)"

# snapshot temp files
$existing = @{}
Get-ChildItem -LiteralPath $temp -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $existing[$_.FullName] = $_.Length }
Write-Log "Temp snapshot: $($existing.Count) file"

# ================= START LOADER =================
$stamp = Get-Date -Format 'HHmmss'
$loaderOut = Join-Path $env:TEMP "loader_stdout_$stamp.txt"
$loaderErr = Join-Path $env:TEMP "loader_stderr_$stamp.txt"
Write-Log "Starting loader: $LoaderPath"
try {
    $loader = Start-Process -FilePath $LoaderPath -PassThru -RedirectStandardOutput $loaderOut -RedirectStandardError $loaderErr
    Write-Log "Loader PID=$($loader.Id) (stdout->$loaderOut)" 'OK'
} catch {
    Write-Log "Start-Process loader GAGAL: $_ - coba tanpa redirect" 'WARN'
    $loader = Start-Process -FilePath $LoaderPath -PassThru
    Write-Log "Loader PID=$($loader.Id) (tanpa redirect)" 'OK'
}

# ================= WATCH STATE =================
$replaced    = @{}   # path -> $true
$lastSize    = @{}   # path -> last known size
$seenPE      = @{}   # path -> $true (sudah dilaporkan drop)
$knownProc   = @{}   # pid  -> name
Get-Process | ForEach-Object { $knownProc[$_.Id] = $_.ProcessName }

$pbSeen = $false; $pbPid = 0; $pbPath = ''; $pbTitle = ''
$injectConfirmed = $false
$loaderExited = $false
$outPos = 0; $errPos = 0
$modSnapFails = 0
$lastModSnap = -99
$rdWarned = $false
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lastBeat = -1

function Read-NewLines {
    param([string]$Path, [int]$Pos, [string]$Tag)
    if (-not (Test-Path -LiteralPath $Path)) { return $Pos }
    $all = @(Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue)
    if ($all.Count -gt $Pos) {
        $all[$Pos..($all.Count-1)] | ForEach-Object { Write-Log "$Tag> $_" 'KEY' }
        return $all.Count
    }
    return $Pos
}

# ================= MAIN LOOP =================
try {
    while ($sw.Elapsed.TotalSeconds -lt $WatchSeconds) {
        Start-Sleep -Milliseconds 700
        try {

        # --- loader stdout/stderr (mungkin kosong: GUI subsystem) ---
        $outPos = Read-NewLines $loaderOut $outPos 'LOADER-OUT'
        $errPos = Read-NewLines $loaderErr $errPos 'LOADER-ERR'

        # --- loader exit ---
        if (-not $loaderExited) {
            try { $loader.Refresh() } catch {}
            if ($loader.HasExited) {
                $loaderExited = $true
                Write-Log "LOADER EXITED code=$($loader.ExitCode) (kalau PB belum start, injection TIDAK akan terjadi)" 'KEY'
                $outPos = Read-NewLines $loaderOut $outPos 'LOADER-OUT'
                $errPos = Read-NewLines $loaderErr $errPos 'LOADER-ERR'
            }
        }

        # --- proses baru (CB.exe, helper .ini PE, dll) + exit tracking ---
        $liveIds = @{}
        Get-Process | ForEach-Object {
            $liveIds[$_.Id] = $true
            if (-not $knownProc.ContainsKey($_.Id)) {
                $knownProc[$_.Id] = $_.ProcessName
                $pp = '(denied)'
                try { $pp = $_.Path } catch {}
                Write-Log "NEW PROCESS: $($_.ProcessName) pid=$($_.Id) path=$pp" 'KEY'
                if ($_.ProcessName -eq 'CB') {
                    $w = Get-CimInstance Win32_Process -Filter "ProcessId=$($_.Id)" -ErrorAction SilentlyContinue
                    if ($w) {
                        $par = Get-CimInstance Win32_Process -Filter "ProcessId=$($w.ParentProcessId)" -ErrorAction SilentlyContinue
                        Write-Log "  CB.exe parent: pid=$($w.ParentProcessId) name=$(if($par){$par.Name}else{'?'})" 'KEY'
                    }
                }
            }
        }
        foreach ($k in @($knownProc.Keys)) {
            if (-not $liveIds.ContainsKey($k)) {
                Write-Log "PROCESS EXIT: $($knownProc[$k]) pid=$k" 'KEY'
                $knownProc.Remove($k)
            }
        }

        # --- CB.exe presence ---
        $cb = Get-Process -Name 'CB' -ErrorAction SilentlyContinue
        if ($cb -and -not $script:cbLogged) {
            $script:cbLogged = $true
            Write-Log "CB.exe RUNNING pid=$($cb.Id) - CheatBlocker anti-cheat (loader CB-check SUDAH di-patch, jadi bukan penghalang)" 'WARN'
        }

        # --- temp: cari PE baru ---
        Get-ChildItem -LiteralPath $temp -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
            $f = $_
            if ($existing.ContainsKey($f.FullName)) { return }
            if ($f.Length -lt 4096) { return }
            try {
                $hdr = Read-FileBytes $f.FullName 0 2
                if ($hdr.Count -lt 2 -or $hdr[0] -ne 0x4D -or $hdr[1] -ne 0x5A) { return }
            } catch { return }

            if (-not $seenPE.ContainsKey($f.FullName)) {
                $seenPE[$f.FullName] = $true
                $hasSumatra = $false
                try {
                    $raw = [System.IO.File]::ReadAllBytes($f.FullName)
                    $txt = [System.Text.Encoding]::ASCII.GetString($raw)
                    $hasSumatra = $txt.Contains('Sumatra')
                } catch {}
                Write-Log "PE DROP: $($f.FullName) size=$($f.Length) write=$($f.LastWriteTime.ToString('HH:mm:ss.fff')) md5=$(Get-MD5Safe $f.FullName) containsSumatra=$hasSumatra" 'KEY'
            }

            # replace kalau kandidat DLL
            if ([math]::Abs($f.Length - $crackedSize) -le 4096) {
                try {
                    $cur = [System.IO.File]::ReadAllBytes($f.FullName)
                    if (-not (Test-DllCracked $cur)) {
                        $before = Get-MD5Safe $f.FullName
                        [System.IO.File]::WriteAllBytes($f.FullName, $crackedDll)
                        $after = Get-MD5Safe $f.FullName
                        Write-Log "REPLACED: $($f.FullName) ($($f.Length) -> $crackedSize bytes) md5 $before -> $after" 'OK'
                    }
                    $replaced[$f.FullName] = $true
                    $lastSize[$f.FullName] = (Get-Item -LiteralPath $f.FullName).Length
                } catch {
                    Write-Log "Replace/read gagal (file locked?): $($f.FullName) - $_" 'WARN'
                }
            }
        }

        # --- size change / trailer append pada file yg sudah di-replace ---
        foreach ($p in @($replaced.Keys)) {
            if (-not (Test-Path -LiteralPath $p)) { continue }
            try { $len = (Get-Item -LiteralPath $p).Length } catch { continue }
            if ($lastSize[$p] -ne $len) {
                $delta = $len - $lastSize[$p]
                Write-Log "SIZE CHANGE: $p $($lastSize[$p]) -> $len (delta=$delta) - loader nulis ulang / append" 'WARN'
                if ($len -gt $lastSize[$p]) {
                    try {
                        $tail = Read-FileBytes $p $lastSize[$p] ([math]::Min(64, $delta))
                        $tailTxt = -join ($tail | ForEach-Object { [char]$_ })
                        Write-Log "  trailer head hex : $(Fmt-Hex $tail)"
                        Write-Log "  trailer head ascii: $tailTxt"
                    } catch {}
                }
                $lastSize[$p] = $len
                # re-verify crack state
                try {
                    $cur = [System.IO.File]::ReadAllBytes($p)
                    if (-not (Test-DllCracked $cur)) {
                        Write-Log "  !! PATCH BYTES KEMBALI KE ASLI - loader overwrite. RE-REPLACE..." 'WARN'
                        [System.IO.File]::WriteAllBytes($p, $crackedDll)
                        Write-Log "  RE-REPLACED ok, md5=$(Get-MD5Safe $p)" 'OK'
                    }
                } catch {}
            }
        }

        # --- PB start ---
        $pbNow = Get-Process -Name 'PointBlank' -ErrorAction SilentlyContinue
        if ($pbNow -and -not $pbSeen) {
            $pbSeen = $true
            $pbPid = $pbNow.Id
            try { $pbPath = $pbNow.Path } catch { $pbPath = '(access denied)' }
            try { $pbTitle = $pbNow.MainWindowTitle } catch { $pbTitle = '' }
            Write-Log "POINTBLANK STARTED pid=$pbPid path=$pbPath title='$pbTitle'" 'KEY'
            if (Test-Path -LiteralPath $pbPath) {
                $fi = Get-Item -LiteralPath $pbPath
                Write-Log "  PB exe: size=$($fi.Length) lastwrite=$($fi.LastWriteTime) filever=$($fi.VersionInfo.FileVersion)"
            }
            $q1 = Test-OpenProcE $pbPid ([uint32]0x1000) "QUERY_LIMITED"
            $q2 = Test-OpenProcE $pbPid ([uint32]0x0400) "QUERY_INFO"
            $q3 = Test-OpenProcE $pbPid ([uint32]0x43A) "INJECT-MINIMAL(0x43A)"
            $q4 = Test-OpenProcE $pbPid ([uint32]0x001F0FFF) "ALL_ACCESS"
            if (-not $q4) {
                Write-Log "  >>> ALL_ACCESS ditolak. TAPI loader BISA tetap sukses (mask kecil / native API) - cek loader stdout di summary: '=> OKS' = SUKSES inject." 'ERROR'
            }
        }

        # --- PB window title update ---
        if ($pbSeen) {
            try {
                $t = (Get-Process -Id $pbPid -ErrorAction SilentlyContinue).MainWindowTitle
                if ($t -and $t -ne $pbTitle) { $pbTitle = $t; Write-Log "PB window title: '$t'" 'INFO' }
            } catch {}
        }

        # --- injection verification via module snapshot (tiap ~2 detik) ---
        if ($pbSeen -and -not $injectConfirmed -and (($sw.Elapsed.TotalSeconds - $lastModSnap) -ge 2)) {
            $lastModSnap = $sw.Elapsed.TotalSeconds
            try {
                $mods = Get-ProcModules $pbPid
                $hit = $mods | Where-Object {
                    $replaced.ContainsKey($_.Path) -or
                    ($_.Name -match '^\d+\.dll$' -and $_.Size -ge 700000)
                }
                if ($hit) {
                    $injectConfirmed = $true
                    $names = ($hit | ForEach-Object { "$($_.Name) ($($_.Size)B) @ $($_.Path)" }) -join ' | '
                    Write-Log ">>> INJECTION CONFIRMED: $names" 'OK'
                    Write-Log ">>> total modules PB: $($mods.Count). Menu harusnya muncul ~5-10s setelah inject (DLL Sleep 5000ms)." 'KEY'
                } else {
                    $big = $mods | Where-Object { $_.Size -ge 700000 } | ForEach-Object { $_.Name }
                    Write-Log "PB modules=$($mods.Count) - DLL cracked BELUM ter-load. big modules: $($big -join ',')" 'WARN'
                }
            } catch {
                $modSnapFails++
                if ($modSnapFails -le 3) { Write-Log "Module snapshot gagal: $_" 'WARN' }
            }
        }

        # --- remote desktop detector (menu hider di DLL) ---
        if (-not $rdWarned) {
            $rdProc = Get-Process | Where-Object { $_.ProcessName -match 'UltraViewer|TeamViewer|AnyDesk|vnc|rustdesk' }
            $rdWin  = (Test-WindowTitle 'UltraViewer') -or (Test-WindowTitle 'TeamViewer') -or (Test-WindowTitle 'AnyDesk')
            if ($rdProc -or $rdWin) {
                $rdWarned = $true
                Write-Log "REMOTE DESKTOP TERDETEKSI ($($rdProc.ProcessName -join ',')) - DLL bisa hide menu kalau detector aktif. TUTUP dulu." 'WARN'
            }
        }

        # --- heartbeat ---
        $el = [int]$sw.Elapsed.TotalSeconds
        if ($el -ne $lastBeat -and ($el % 30) -eq 0) {
            $lastBeat = $el
            $state = @()
            $state += "replaced=$($replaced.Count)"
            $state += "pb=$pbSeen"
            $state += "injected=$injectConfirmed"
            $state += "loaderExited=$loaderExited"
            Write-Log ("heartbeat {0}s | {1}" -f $el, ($state -join ' | '))
        }
        } catch {
            Write-Log "LOOP ERROR (loop dilanjutkan): $_" 'ERROR'
        }
    }
} finally {
    # ================= SUMMARY =================
    Write-Log '==================== SUMMARY ====================' 'KEY'
    Write-Log "Elevated        : $isAdmin"
    Write-Log "Loader exited   : $loaderExited (code=$(if($loaderExited){$loader.ExitCode}else{'n/a'}))"
    Write-Log "PB seen         : $pbSeen (pid=$pbPid)"
    Write-Log "Injection OK    : $injectConfirmed"

    if ($injectConfirmed) {
        Write-Log 'VERDIK: DLL SUKSES MASUK ke PB. Menu ga muncul = MASALAH DLL-SIDE, bukan loader/oneclick.' 'KEY'
        Write-Log 'Kemungkinan: (a) hook/render gagal karena versi game (PBNC XVII update 26.OCT.2026) - pattern scan engine gagal; (b) menu di-hide (DAT_100b2498=0 / screenshot detector); (c) perlu tekan INSERT.' 'KEY'
        Write-Log 'Tes lanjutan: 1) tekan INSERT di lobby; 2) tutup UltraViewer/TeamViewer/AnyDesk; 3) inject decrypted_payload_cracked.dll MANUAL pakai injector lo - kalau hasilnya sama (ga muncul), 100% DLL butuh update buat versi game baru.' 'KEY'
    } elseif ($pbSeen) {
        $q3 = Test-OpenProc $pbPid ([uint32]0x001F0FFF)
        if (-not $q3) {
            Write-Log 'VERDIK: ALL_ACCESS ditolak TAPI loader bisa tetap SUKSES (pakai mask kecil / native API). Cek loader stdout: "=> OKS" = injection SUKSES (masalah pindah ke DLL-side); loop "Proc Eror" = loader gagal inject.' 'KEY'
        } else {
            Write-Log 'VERDIK: PB bisa di-open tapi DLL TIDAK ter-load. Cek: window loader (=> OKS / Proc Eror / looping Login Point Blank), CB.exe, dan pastikan oneclick replace DLL SEBELUM PB start.' 'KEY'
        }
    } else {
        Write-Log 'VERDIK: PB ga pernah start selama watch window. Jalankan PB setelah loader jalan (jangan tutup window loader).' 'KEY'
    }

    if (Test-Path -LiteralPath $loaderOut) {
        $c = @(Get-Content -LiteralPath $loaderOut -ErrorAction SilentlyContinue)
        if ($c.Count -gt 0) {
            Write-Log "--- LOADER STDOUT ($($c.Count) baris) ---"
            $c | Where-Object { $_ -notmatch 'Downloaded:' } | ForEach-Object { Write-Log "LOADER> $_" }
        } else {
            Write-Log '--- LOADER STDOUT: KOSONG (loader subsystem=GUI; output ada di window loader sendiri) ---'
        }
    }
    if (Test-Path -LiteralPath $loaderErr) {
        $c = @(Get-Content -LiteralPath $loaderErr -ErrorAction SilentlyContinue)
        if ($c.Count -gt 0) {
            Write-Log "--- LOADER STDERR ($($c.Count) baris) ---"
            $c | ForEach-Object { Write-Log "LOADER-ERR> $_" }
        }
    }

    foreach ($p in @($replaced.Keys)) {
        if (Test-Path -LiteralPath $p) {
            try {
                $fi = Get-Item -LiteralPath $p
                $b = [System.IO.File]::ReadAllBytes($p)
                Write-Log "FINAL FILE: $p size=$($fi.Length) cracked=$(Test-DllCracked $b) md5=$(Get-MD5Safe $p)"
            } catch {
                Write-Log "FINAL FILE: $p (read gagal: $_)" 'WARN'
            }
        } else {
            Write-Log "FINAL FILE: $p SUDAH HILANG (dihapus loader/cleaner)" 'WARN'
        }
    }

    Write-Log "Log lengkap: $LogFile" 'KEY'
    Write-Log '====================== END ======================' 'KEY'
}