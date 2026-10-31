# pb-loader-toolkit

Windows PE loader analysis & patching toolkit — Point Blank ID

> ### Cracked by **dekkaaa9**
> GitHub: **https://github.com/dekkaaa9**
>
> Everything in this repository — the full crack chain (loader crack, payload
> decryption, license bypass, permanent menu patch), the injectors, and the
> cheat source — was done by **dekkaaa9**. Reverse engineering with REA +
> Ghidra, patching with PowerShell, built with llvm-mingw.

---

## Table of Contents

- [What This Is](#what-this-is)
- [Quick Start (Daily Use)](#quick-start-daily-use)
- [Repository Layout](#repository-layout)
- [Crack Chain — Start to Finish](#crack-chain--start-to-finish)
- [Patch Tables](#patch-tables)
- [Engine Intel](#engine-intel)
- [Tools Used](#tools-used)
- [Build](#build)
- [How It Works](#how-it-works)
- [Tuning](#tuning)
- [Troubleshooting](#troubleshooting)
- [Credits](#credits)
- [License](#license)

---

## What This Is

A complete toolkit for analyzing, patching, and working with the Sumatra PB
loader and its encrypted payload for **Point Blank ID**.

It contains:

| Part | What it does |
|---|---|
| **Crack chain** (`PB_Sumatra/`) | 17 PowerShell scripts that crack the loader, decrypt the payload, bypass the license check, and permanently patch the menu |
| **Injectors** | `PBInjectorGUI.exe` (native Win32 GUI) + `inject_v2.ps1` (PowerShell) |
| **Cheat source** (`PBCheat/`) | 16 C++ files — bypass, hooks, entity traversal, ESP rendering, menu |
| **Documentation** | This README — full walkthrough of every patch with file offsets |

All paths in this repo are **relative**. Extract it anywhere and the scripts
and GUI resolve their own location automatically.

---

## Quick Start (Daily Use)

Once the crack chain has been run once (see below), the bypass is permanent.
Daily use is just injection:

1. Launch Point Blank and wait until you're in the **lobby**
2. Double-click **`PBInjectorGUI.exe`**
3. Click **INJECT** (click YES if the UAC prompt appears — the elevated
   instance auto-injects)
4. The menu appears automatically — no INSERT key needed

The GUI shows live status: whether Point Blank is running, whether you're
admin, whether the payload DLL is valid, plus a full log.

Script alternative:

```powershell
powershell -ExecutionPolicy Bypass -File .\inject_v2.ps1
```

### In-game hotkeys

| Key | Action |
|---|---|
| `INSERT` | Toggle menu |
| `UP` / `DOWN` | Navigate |
| `ENTER` | Toggle / fire |
| `LEFT` / `RIGHT` | Adjust slider |
| `END` | Panic (disable ESP + hide menu) |

---

## Repository Layout

```
pb-loader-toolkit\
|
|-- README.md                  This file
|-- LICENSE                    MIT, (c) dekkaaa9
|-- .gitignore
|-- .gitattributes             Binary protection for exe/dll/bin
|-- build.bat                  Build everything (cheat + injectors + GUI)
|
|-- ---- DAILY INJECT ----
|-- PBInjectorGUI.exe          Daily GUI injector (double-click, click INJECT)
|-- PBInjectorGUI\main.cpp     GUI source (dark theme, DPI-aware, owner-drawn)
|-- PBInjector.exe             Manual-map injector
|-- PBInjector\main.cpp        Injector source
|-- inject_v2.ps1              PowerShell injector (GUI alternative)
|-- force_menu.ps1             Legacy runtime patcher (not needed daily)
|
|-- ---- CHEAT ----
|-- PBCheat\                   16 source files (bypass, hooks, entities, esp, gui, menu)
|-- patch_dll_final.ps1        Re-apply the 7 permanent patches (idempotent)
|
|-- ---- CRACK CHAIN (flat) ----
|-- PB_Sumatra\
|   |-- LoaderVIP_cracked.exe          Final loader (all patches applied)
|   |-- LoaderVIP_cracked_precbpatch.exe.bak
|   |-- Loadertrial_cracked.exe
|   |-- crack_loader.ps1               Crack Loadertrial (IL brfalse target)
|   |-- crack_loaderVIP.ps1            Crack LoaderVIP (serial + size + timer)
|   |-- crack_dll_key.ps1              Crack the license key check in the payload
|   |-- patch_loader_cb.ps1            Remove the CB.exe kill switch
|   |-- fix_loader.ps1                 Loader fixer
|   |-- oneclick_crack.ps1             Crack chain orchestrator
|   |-- oneclick_logged.ps1            Orchestrator + full logging + diagnosis
|   |-- preencrypt.ps1                 Re-encrypt the patched payload for the loader
|   |-- verify_patches.ps1             Verify patch bytes in the loader
|   |-- decrypt_payload.ps1            Decrypt the downloaded payload
|   |-- decrypt_dll_strings.ps1        Decrypt the DLL string table
|   |-- decrypt_strings.ps1
|   |-- decrypt_size_error.ps1
|   |-- find_size_xref.ps1             RE helper: find the size-check xref
|   |-- find_error_strings.ps1         RE helper: find error strings
|   |-- find_timer.ps1                 RE helper: find the timer logic
|   |-- downloaded_payload.bin         Original encrypted payload
|   |-- decrypted_payload.dll          Decrypted payload (pre-license-crack)
|   |-- payload_preencrypted.bin       Re-encrypted payload for the loader
|   |-- logs\                          Output of oneclick_logged
|
|-- logs\                       Inject / force_menu output
```

> **Path handling:** every script derives its own root from `$PSScriptRoot`,
> and the GUI resolves its paths relative to its own executable. Nothing is
> hardcoded to a specific drive or folder.

---

## Crack Chain — Start to Finish

Full walkthrough of how the loader was cracked, from the original downloader
to a working cheat.

### Target

| Component | Role |
|---|---|
| `Loadertrial.exe` | GUI downloader — the "serial" button downloads everything, "Start Loader" runs LoaderVIP |
| `LoaderVIP.exe` | Main loader — serial check, file size check, expiry timer, downloads payload, decrypts, injects |
| `CheckSerial.exe` | External serial validator |
| Payload (encrypted) | The cheat DLL, XOR-encrypted from the server, written to `%TEMP%`, then injected |
| `CB.exe` | CheatBlocker anti-cheat — if running, the loader skips injection |

Original (un-cracked) flow:

```
Loadertrial.exe --download--> LoaderVIP.exe + CheckSerial.exe + DLLs
                                          |
LoaderVIP.exe --serial check--> fail? STOP
             --file size check--> fail? STOP
             --timer check--> expired? STOP
             --download payload--> XOR decrypt --> write to %TEMP%
             --CB.exe running?--> STOP (skip inject)
             --inject DLL into PointBlank
```

---

### Step 1 — Crack Loadertrial.exe (the downloader)

**Script:** `PB_Sumatra\crack_loader.ps1`
**Source:** `Loadertrial.exe` (from your Downloads folder)
**Output:** `PB_Sumatra\Loadertrial_cracked.exe`

Loadertrial is a .NET assembly, so the patch is at the IL level, not native.

| # | File Offset | Original | Patched | Effect |
|---|---|---|---|---|
| 1 | 739 | `2C 09` | `2C 15` | `brfalse.s` target 34 → 46. If `LoaderVIP.exe` is missing, skip the button-disable instead of erroring |

Patched token: `0x06000004` (method `start_Click`), RVA 8384, fat header 12
bytes, IL starts at file offset 716.

**Result:** the trial loader no longer refuses to run when files are missing.
The "serial" button downloads all requirements to `%TEMP%\LoaderTrialTemp\`.

Manual download URLs (if the loader fails):

```
LoaderVIP.exe   : https://www.sumatra.biz.id/DOwNload/versi1/LoaderVIP.exe
CheckSerial.exe : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/CheckSerial.exe
msvcp100.dll    : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcp100.dll
msvcp90.dll     : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcp90.dll
msvcr100.dll    : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcr100.dll
msvcr100d.dll   : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcr100d.dll
msvcr90.dll     : https://github.com/Aniro21/SuMatraUI/raw/refs/heads/main/1/msvcr90.dll
```

---

### Step 2 — Crack LoaderVIP.exe (the main loader)

**Script:** `PB_Sumatra\crack_loaderVIP.ps1`
**Source:** `%TEMP%\LoaderTrialTemp\LoaderVIP.exe`
**Output:** `PB_Sumatra\LoaderVIP_cracked.exe`

Native x86 PE. Every patch is byte-verified before being written — if the
bytes don't match, the script aborts instead of corrupting the file.

| # | FO | VA | Original | Patched | Effect |
|---|---|---|---|---|---|
| 1 | 0x2FA9 | 0x403BA9 | `0F 84 5D 0A 00 00` | `90 90 90 90 90 90` | NOP the `jz` serial blacklist — **any serial is accepted** |
| 2 | 0x38CA | 0x4044CA | `7E 4C` | `EB 4C` | `jle` → `jmp` — **file size check skipped** |
| 3 | 0x37AA | 0x4043AA | `00 D8 5E AC` | `FF FF FF FF` | `add eax, 0xAC5ED800` → `add eax, 0xFFFFFFFF` (timer low maxed) |
| 3b | 0x37B5 | 0x4043B5 | `3A` | `7F` | `adc ecx, 0x3A` → `adc ecx, 0x7F` (timer high maxed) |

Patches 3 + 3b maximize the expiry timestamp offset from ~7 hours to
effectively forever. The countdown shows a far-future date instead of
expiring.

**Result:** LoaderVIP runs without a valid serial, without a size check, and
without expiring.

---

### Step 3 — Remove the CB.exe kill switch

**Script:** `PB_Sumatra\patch_loader_cb.ps1`
**Target:** `PB_Sumatra\LoaderVIP_cracked.exe` (in-place, backup: `LoaderVIP_cracked_precbpatch.exe.bak`)

The watchdog loop `FUN_00404d50` checks for `CB.exe` (CheatBlocker) *before*
injecting. If CB.exe is running, injection is skipped.

| # | FO | RVA | Original | Patched | Effect |
|---|---|---|---|---|---|
| 1 | 0x66C5 | 0x72C5 | `75 04` | `90 90` | NOP the `jz` — loader **always injects** regardless of CB.exe |

Context is verified before patching: `85 C0` (TEST EAX,EAX) before, `B1 01`
(MOV CL,1) after. FO = 0x400 + (0x72C5 - 0x1000).

**Result:** CB.exe is no longer an obstacle to injection.

---

### Step 4 — Run the loader, capture the decrypted payload

Run `LoaderVIP_cracked.exe`. It:

1. Downloads the encrypted payload from the server into `%TEMP%`
2. XOR-decrypts it with the key **`325768235246856876125`** (21-byte ASCII, repeating)
3. Writes the decrypted DLL to `%TEMP%`
4. Injects it into PointBlank

**Script:** `PB_Sumatra\decrypt_payload.ps1` — decrypt manually from
`downloaded_payload.bin` (the captured original payload):

```powershell
$encrypted = [System.IO.File]::ReadAllBytes('downloaded_payload.bin')
$key = [System.Text.Encoding]::ASCII.GetBytes('325768235246856876125')
for ($i = 0; $i -lt $encrypted.Length; $i++) {
    $decrypted[$i] = $encrypted[$i] -bxor $key[$i % $key.Length]
}
# check for the MZ header -> valid PE -> save decrypted_payload.dll
```

**Output:** `PB_Sumatra\decrypted_payload.dll` (781312 bytes, native x86 PE32)

---

### Step 5 — Crack the license key system in the payload

**Script:** `PB_Sumatra\crack_dll_key.ps1`
**Source:** `PB_Sumatra\decrypted_payload.dll`
**Output:** `decrypted_payload_cracked.dll`

This is the first crack applied to the DLL — it bypasses key/license
validation.

| # | FO | Original | Patched | Effect |
|---|---|---|---|---|
| 1 | 0x4A012 | `0F 85 B1 00 00 00` | `E9 B2 00 00 00 90` | `jnz` → `jmp` — validation **always passes** |
| 2 | 0x4A0C3 | `0F 84 57 FF FF FF` | `90 90 90 90 90 90` | NOP the `jz` — retry loop removed, falls through to passed |
| 3a | 0x3507A | `0F 85 42 07 00 00` | `90 90 90 90 90 90` | NOP the `jnz` — menu render check 1 bypassed |
| 3b | 0x35087 | `0F 85 35 07 00 00` | `90 90 90 90 90 90` | NOP the `jnz` — menu render check 2 bypassed |

**Result:** any key (or no key at all) works. No license needed. Server
validation is ignored. The menu renders regardless of license status.

---

### Step 6 — Permanent menu patch (7 patches, A–G)

**Script:** `patch_dll_final.ps1` (idempotent, safe to re-run)
**Target:** `decrypted_payload_cracked.dll`
**Backup:** `decrypted_payload_cracked.dll.bak`

This is what makes the menu appear automatically without pressing INSERT.
RVA = FO + 0xC00.

| # | FO | RVA | Original | Patched | Effect |
|---|---|---|---|---|---|
| A | 0x05F5E4 | 0x0601E4 | `74 EA` | `90 90` | init-thread wait-loop-2: `call[vtable+0xA8]==0` → `jz` loop. NOP = exit loop → init-complete=1 |
| B | 0x05E82C | 0x05F42C | `0F 84 DC 01 00 00` | NOP x6 | render chain `FUN_1005f310`: `call[vtable+0xA8]==0` → `jz` bail. NOP = menu keeps rendering |
| C | 0x05E2FE | 0x05EEFE | `0F 84 71 01 00 00` | NOP x6 | `FUN_1005eec0` GATE 1: `call[vtable+0x9C]==0` → `jz` return. NOP = continue to `FUN_10035bc0` (menu render) |
| D | 0x05D82A | 0x05E42A | `75 10` | `EB 10` | `FUN_1005e3f0`: `jnz`→`jmp`, skip the menu-visible refresh from `method_0x20` (returns 0) |
| E | 0x035830 | 0x036430 | `75 12` | `EB 12` | `FUN_10036420`: `jnz`→`jmp`, skip the menu-visible refresh from `method_0x20` (returns 0) |
| F | 0x05F65D | 0x06025D | `75 B1` | `90 90` | init-thread wait-loop-3 (state 2/4/5): `jnz` loop. NOP = init thread finishes |
| G | 0x031CE5 | 0x0328E5 | `00 00 00 00` | `01 00 00 00` | init: `mov [menu-visible], 0` → `mov [menu-visible], 1` — menu starts visible |

Re-apply at any time:

```powershell
powershell -ExecutionPolicy Bypass -File .\patch_dll_final.ps1
```

---

### Step 7 — Pre-encrypt the patched DLL for the loader

**Script:** `PB_Sumatra\preencrypt.ps1`
**Source:** `decrypted_payload_cracked.dll`
**Output:** `PB_Sumatra\payload_preencrypted.bin`

The loader XOR-decrypts the payload with the same key, so we pre-encrypt the
already-patched DLL with that key — when the loader decrypts our file, the
result is the cracked DLL.

```powershell
$key = [System.Text.Encoding]::ASCII.GetBytes('325768235246856876125')
for ($i = 0; $i -lt $crackedDll.Length; $i++) {
    $preEncrypted[$i] = $crackedDll[$i] -bxor $key[$i % $key.Length]
}
```

---

### Step 8 — One-click crack (runtime replacement)

**Script:** `PB_Sumatra\oneclick_crack.ps1` (basic)
**Script:** `PB_Sumatra\oneclick_logged.ps1` (full logging + diagnosis)

This is what you run when you want to play via the loader instead of manual
injection:

1. Snapshot the files in `%TEMP%`
2. Start `LoaderVIP_cracked.exe`
3. Watch `%TEMP%` for a new PE whose size matches (±1024/4096 bytes) and whose header is `MZ`
4. **Replace** that file with `decrypted_payload_cracked.dll`
5. If the loader rewrites or appends a trailer, re-verify the patch bytes and RE-REPLACE if they reverted
6. Start PointBlank — the loader injects the cracked DLL

`oneclick_logged.ps1` adds: auto-relaunch as ADMIN, loader stdout/stderr
capture, new-process tracking (CB.exe, ZLauncher children), DLL drop logging
with MD5 before/after, PB start details (PID/path/version/window title),
privilege testing (QUERY_LIMITED / QUERY_INFO / ALL_ACCESS), a module
snapshot every 2 seconds as proof of injection, a remote-desktop detector
(UltraViewer/TeamViewer/AnyDesk can hide the menu), and a SUMMARY + verdict
at the end.

Log: `PB_Sumatra\logs\run_YYYYMMDD_HHMMSS.log`

---

### Step 9 — Daily injection (cheat works)

After the chain above, the bypass is permanent. Daily use is just injection:

1. Launch Point Blank, enter the **lobby**
2. Double-click **`PBInjectorGUI.exe`**
3. Click **INJECT** (click YES if UAC appears — the new window auto-injects)
4. The menu appears automatically — no INSERT, no force_menu.ps1

Script alternative:

```powershell
powershell -ExecutionPolicy Bypass -File .\inject_v2.ps1
```

---

### Crack Flow Overview

```
Loadertrial.exe ──crack_loader.ps1──────────► Loadertrial_cracked.exe
                                                    │ download
                                                    ▼
                                    %TEMP%\LoaderTrialTemp\LoaderVIP.exe
                                                    │
                            crack_loaderVIP.ps1 ────┤  (serial + size + timer)
                            patch_loader_cb.ps1 ────┘  (CB.exe kill switch)
                                                    ▼
                                          LoaderVIP_cracked.exe  ◄── FINAL LOADER
                                                    │ run
                                                    ▼
                                    %TEMP%\<decrypted DLL>
                                                    │
                         decrypt_payload.ps1 ──────┤  (XOR key 325768235246856876125)
                                                    ▼
                                          decrypted_payload.dll
                                                    │
                         crack_dll_key.ps1 ─────────┤  (license bypass)
                         patch_dll_final.ps1 ───────┘  (7 patches A–G, menu auto)
                                                    ▼
                                   decrypted_payload_cracked.dll  ◄── FINAL DLL
                                                    │
                    ┌───────────────────────────────┴───────────────────────────┐
                    ▼                                                           ▼
        preencrypt.ps1 → payload_preencrypted.bin              inject_v2.ps1 / PBInjectorGUI.exe
                    │                                                           │
                    ▼                                                           ▼
        oneclick_crack.ps1 (loader inject)                        DIRECT INJECT (daily)
                    │                                                           │
                    └───────────────────────┬───────────────────────────────────┘
                                            ▼
                              POINT BLANK + VIP MENU APPEARS ✓
```

---

## Patch Tables

### Loader patches (LoaderVIP_cracked.exe)

| Patch | FO | VA | Original | Patched | Purpose |
|---|---|---|---|---|---|
| Serial bypass | 0x2FA9 | 0x403BA9 | `0F 84 5D 0A 00 00` | `90` x6 | NOP `jz` — any serial accepted |
| File size bypass | 0x38CA | 0x4044CA | `7E 4C` | `EB 4C` | `jle` → `jmp` — size check skipped |
| Timer low | 0x37AA | 0x4043AA | `00 D8 5E AC` | `FF FF FF FF` | `add eax` maxed |
| Timer high | 0x37B5 | 0x4043B5 | `3A` | `7F` | `adc ecx` maxed |
| CB kill switch | 0x66C5 | 0x72C5 | `75 04` | `90 90` | NOP `jz` — always inject |

### Payload license patches (crack_dll_key.ps1)

| Patch | FO | Original | Patched | Purpose |
|---|---|---|---|---|
| Call site 1 | 0x4A012 | `0F 85 B1 00 00 00` | `E9 B2 00 00 00 90` | `jnz` → `jmp` — validation passes |
| Call site 2 | 0x4A0C3 | `0F 84 57 FF FF FF` | `90` x6 | NOP `jz` — no retry loop |
| Menu check 1 | 0x3507A | `0F 85 42 07 00 00` | `90` x6 | NOP `jnz` — menu renders |
| Menu check 2 | 0x35087 | `0F 85 35 07 00 00` | `90` x6 | NOP `jnz` — menu renders |

### Permanent menu patches (patch_dll_final.ps1, A–G)

See [Step 6](#step-6--permanent-menu-patch-7-patches-ag) for the full table
with RVA and per-patch effect.

---

## Engine Intel

Reverse-engineered from `i3SceneDx.dll` and `i3GfxDx.dll` using REA + Ghidra.

| Target | Offset (from DLL base) | Purpose |
|--------|------------------------|---------|
| `i3SceneDx.dll` `i3SceneGraphContext::OnUpdate` | `0xFD880` | Hook to capture the scene context + tracer |
| `i3SceneDx.dll` OnUpdate vtable slot | `0x1C850C` | Slot storing `&OnUpdate` — patched directly |
| `i3SceneDx.dll` `i3SceneObject::CalcBoneWorldMatrix` | `0x107350` | Bone world positions |
| `i3SceneDx.dll` `i3SceneGraphContext::get3DMainRoot` | `0x2D920` | 3D scene root node |
| `i3SceneDx.dll` `i3SceneTracer::getViewProjectionMatrix` | `0x2DF00` | VP matrix at tracer+0x150 |
| `i3GfxDx.dll` `i3RenderContext::GetNativeDevice` | `0x10740` | D3D9 device pointer |
| `i3GfxDx.dll` `i3RenderContext::End` | `0x6CBF0` | Engine render loop end |

---

## Tools Used

Everything in this repo was built with these tools.

### Reverse Engineering & Analysis

| Tool | Used for |
|---|---|
| **REA** | Decompiling and analyzing `i3SceneDx.dll` / `i3GfxDx.dll`, locating engine offsets (OnUpdate, CalcBoneWorldMatrix, getViewProjectionMatrix), finding hook points and vtable slots |
| **Ghidra** | Cross-checking decompilation, analyzing loader functions (`FUN_00404d50` CB watchdog, serial/size/timer checks), locating patch points in LoaderVIP.exe and the payload DLL |
| **PowerShell 5.1** | RE helper scripts: `find_size_xref.ps1`, `find_error_strings.ps1`, `find_timer.ps1`, `decrypt_strings.ps1`, `decrypt_dll_strings.ps1`, `decrypt_size_error.ps1` — byte scanning, xref hunting, string decryption |
| **PowerShell 5.1** | Byte-level patching for every crack script (verify-then-patch, idempotent) |

### Build & Compiler

| Tool | Used for |
|---|---|
| **llvm-mingw** (`i686-w64-mingw32-clang++`, 20261020-msvcrt-x86_64) | Building `PBCheat.dll`, `PBInjector.exe`, `PBInjectorGUI.exe` — static, `-fno-exceptions -fno-rtti -O2 -std=c++17`, target **i686 (32-bit)** because PB is x86 |
| **MinGW64** | 64-bit toolchain |
| **libunwind.a** | Static unwind, avoids a `libunwind.dll` runtime dependency |

### APIs & SDKs

| Tool | Used for |
|---|---|
| **Win32 API** | The GUI (owner-drawn, DPI-aware, DWM dark mode + rounded corners), the injectors (`OpenProcess`/`VirtualAllocEx`/`WriteProcessMemory`/`CreateRemoteThread`), module scanning (Toolhelp32), UAC elevation (`ShellExecuteEx` runas) |
| **DirectX 9** (`d3d9.lib`, D3DXLine/D3DXFont) | ESP rendering, EndScene vtable hook |
| **Windows SDK** (`psapi`, `advapi32`, `shellapi`, `dwmapi`) | EnumProcessModules, token membership (admin check), shell exec, DWM attributes |

### Environment

| Tool | Used for |
|---|---|
| **Windows 11** | Target OS (WOW64 — x86 PB on an x64 system) |
| **OpenCode** | The agent environment where the entire crack chain, injectors, GUI, and documentation were built |

---

## Build

```
build.bat
```

Requires **llvm-mingw** (i686) and **MinGW64**. Outputs `PBCheat.dll`,
`PBInjector.exe`, and `PBInjectorGUI.exe`.

The GUI is a native Win32 C++ application — no runtime dependencies, no
framework, static-linked. It is built 32-bit (i686) because Point Blank is a
32-bit process and the injector must match.

---

## How It Works

- **Bypass**: kills CB.exe (Cheat Blocker) with a watchdog re-kill every 500ms; manual map (no LoadLibrary); clears PEB BeingDebugged + NtGlobalFlag.
- **Entities**: hooks `OnUpdate` via a vtable slot patch (no inline hook, no instruction relocation); traverses the scene graph; players are 12–80 bones; head is bone 7.
- **Rendering**: D3D9 EndScene vtable hook; VP matrix from tracer+0x150 (validated, with a fallback to device transforms); D3DXLine/D3DXFont.
- **Injector**: `LoadLibraryA` via a remote thread (not manual map) — the DLL is already patched so stealth isn't needed. The injector is 32-bit (i686) to match PB. ANSI path + `LoadLibraryA` keeps the WOW64-shared kernel32 address valid.
- **GUI**: resolves its payload path and log directory relative to its own executable, so the repo works from any location.

---

## Tuning

Edit `PBCheat\config.h`: `headBoneIndex` (try 6/7/8), `minBoneCount` /
`maxBoneCount`, `maxDistance`, colors, hotkeys.

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Menu doesn't appear | Re-run `patch_dll_final.ps1`, make sure you inject while in the lobby |
| Menu gets hidden | Close UltraViewer/TeamViewer/AnyDesk/VNC — the DLL hides the menu when a remote-desktop detector is active |
| No entities | Widen the bone count, try a different `headBoneIndex` |
| Crash on inject | Run as Admin, inject after the game is fully loaded |
| `LoadLibraryA` returns 0 | The DLL refused to load — missing dependency or unreadable file |
| Exit code `0xC0000005` | The injector is still 64-bit — click YES on the UAC prompt |
| CB.exe respawns | The watchdog handles it; inject faster |
| Loader loops on "Login Point Blank" | The serial patch failed — verify with `verify_patches.ps1` |
| DLL in temp reverted | The loader overwrote it — `oneclick_logged.ps1` auto-replaces |

---

## Credits

**Crack, development, and documentation by [dekkaaa9](https://github.com/dekkaaa9)**

| Part | Files |
|---|---|
| Loader crack | `crack_loader.ps1` (IL patch), `crack_loaderVIP.ps1` (serial + size + timer), `patch_loader_cb.ps1` (CB.exe kill switch) |
| Payload crack | `decrypt_payload.ps1` (XOR key recovery), `crack_dll_key.ps1` (license bypass) |
| Permanent patch | `patch_dll_final.ps1` (7 patches A–G, menu auto-appear) |
| Orchestration | `oneclick_crack.ps1`, `oneclick_logged.ps1`, `preencrypt.ps1`, `verify_patches.ps1` |
| Injectors | `inject_v2.ps1`, `PBInjector.exe`, `PBInjectorGUI.exe` + `PBInjectorGUI\main.cpp` |
| Cheat | `PBCheat\` (16 source files — bypass, hooks, entities, esp, gui, menu) |
| Tools | REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9 |

Repository: **https://github.com/dekkaaa9/pb-loader-toolkit**

---

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 dekkaaa9.
