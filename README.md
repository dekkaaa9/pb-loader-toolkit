# PB Sumatra — Point Blank ID VIP Cheat (Permanent Bypass)

Internal ESP cheat for **Point Blank ID** (Zepetto), plus the complete crack
chain for LoaderVIP + payload decryption. The VIP bypass is already
**permanent** (patched into the DLL, not runtime).

> ### Cracked by **dekkaaa9**
> GitHub: **https://github.com/dekkaaa9**
>
> The entire crack chain (loader crack, payload decrypt, DLL key bypass,
> permanent menu patch), injector, and GUI in this project was done by
> **dekkaaa9**. Reverse engineering with REA + Ghidra, patching with
> PowerShell, build with llvm-mingw. If anyone wants to understand or
> continue it, check the repo at the link above.

---

# CRACK CHAIN — FROM SCRATCH TO WORKING CHEAT

This is the complete documentation of how the Sumatra PB loader was cracked
from zero to a working cheat. All scripts are in `PB_Sumatra\` (crack chain)
and the repo root (working toolkit).

## Target

| Component | Function |
|---|---|
| `Loadertrial.exe` | GUI downloader — the "serial" button downloads all files, the "Start Loader" button runs LoaderVIP |
| `LoaderVIP.exe` | Main loader — serial check, file size check, timer expiry, download payload, decrypt, inject |
| `CheckSerial.exe` | External serial validator |
| Payload (encrypted) | Cheat DLL, XOR'd from the server, written to `%TEMP%`, then injected into PB |
| `CB.exe` | CheatBlocker anti-cheat — if running, the loader SKIPS inject |

Original flow (not yet cracked):

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

## STEP 1 — Crack Loadertrial.exe (downloader)

**Script:** `PB_Sumatra\crack_loader.ps1`
**Source:** `Loadertrial.exe` (from your Downloads folder)
**Output:** `PB_Sumatra\Loadertrial_cracked.exe`

Loadertrial is a .NET assembly. The patch is at the IL level, not native.

| # | File Offset | Original | Patched | Function |
|---|---|---|---|---|
| 1 | 739 | `2C 09` | `2C 15` | `brfalse.s` target 34 → 46. If `LoaderVIP.exe` is missing, skip the disable-button instead of erroring |

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

## STEP 2 — Crack LoaderVIP.exe (main loader)

**Script:** `PB_Sumatra\crack_loaderVIP.ps1`
**Source:** `%TEMP%\LoaderTrialTemp\LoaderVIP.exe`
**Output:** `PB_Sumatra\LoaderVIP_cracked.exe`

Native x86 PE. Every patch is verified first (byte check) before being
written — on mismatch the script ABORTS, it does not corrupt the file.

| # | FO | VA | Original | Patched | Function |
|---|---|---|---|---|---|
| 1 | 0x2FA9 | 0x403BA9 | `0F 84 5D 0A 00 00` | `90 90 90 90 90 90` | NOP `jz` serial blacklist → **any serial is accepted** |
| 2 | 0x38CA | 0x4044CA | `7E 4C` | `EB 4C` | `jle` → `jmp` → **file size check skipped** |
| 3 | 0x37AA | 0x4043AA | `00 D8 5E AC` | `FF FF FF FF` | `add eax, 0xAC5ED800` → `add eax, 0xFFFFFFFF` (timer low max) |
| 3b | 0x37B5 | 0x4043B5 | `3A` | `7F` | `adc ecx, 0x3A` → `adc ecx, 0x7F` (timer high max) |

Patch 3+3b: the expiry timestamp offset is maximized from ~7 hours to
~forever. The countdown shows a far-future date instead of expiring.

**Result:** LoaderVIP runs without a valid serial, without a size check, and
without expiry.

---

## STEP 3 — Crack the CB.exe kill switch in the loader

**Script:** `PB_Sumatra\patch_loader_cb.ps1`
**Target:** `PB_Sumatra\LoaderVIP_cracked.exe` (in-place, backup: `LoaderVIP_cracked_precbpatch.exe.bak`)

The watchdog loop `FUN_00404d50` checks `CB.exe` (CheatBlocker) BEFORE
injecting. If CB.exe is running → inject is SKIPPED.

| # | FO | RVA | Original | Patched | Function |
|---|---|---|---|---|---|
| 1 | 0x66C5 | 0x72C5 | `75 04` | `90 90` | NOP `jz` → loader **always injects** regardless of CB.exe |

Context is verified before patching: `85 C0` (TEST EAX,EAX) before, `B1 01`
(MOV CL,1) after. FO = 0x400 + (0x72C5 - 0x1000).

**Result:** CB.exe is no longer an obstacle to inject.

---

## STEP 4 — Run the loader, capture the decrypted payload

Run `LoaderVIP_cracked.exe`. It:

1. Downloads the encrypted payload from the server → `%TEMP%\...`
2. XOR-decrypts it with the key **`325768235246856876125`** (21-byte ASCII, repeating)
3. Writes the decrypted DLL to `%TEMP%`
4. Injects it into PointBlank

**Script:** `PB_Sumatra\decrypt_payload.ps1` — decrypt manually from
`downloaded_payload.bin` (the original captured payload):

```powershell
$encrypted = [System.IO.File]::ReadAllBytes('PB_Sumatra\downloaded_payload.bin')
$key = [System.Text.Encoding]::ASCII.GetBytes('325768235246856876125')
for ($i = 0; $i -lt $encrypted.Length; $i++) {
    $decrypted[$i] = $encrypted[$i] -bxor $key[$i % $key.Length]
}
# check MZ header -> valid PE -> save decrypted_payload.dll
```

**Output:** `PB_Sumatra\decrypted_payload.dll` (781312 bytes, native x86 PE32)

---

## STEP 5 — Crack the key system in the payload DLL

**Script:** `PB_Sumatra\crack_dll_key.ps1`
**Source:** `PB_Sumatra\decrypted_payload.dll`
**Output:** `decrypted_payload_cracked.dll`

This is the FIRST crack applied to the DLL — bypasses key/license validation.

| # | FO | Original | Patched | Function |
|---|---|---|---|---|
| 1 | 0x4A012 | `0F 85 B1 00 00 00` | `E9 B2 00 00 00 90` | `jnz` → `jmp` — validation **always passes** |
| 2 | 0x4A0C3 | `0F 84 57 FF FF FF` | `90 90 90 90 90 90` | NOP `jz` — retry loop removed, falls through to passed |
| 3a | 0x3507A | `0F 85 42 07 00 00` | `90 90 90 90 90 90` | NOP `jnz` — menu render check 1 skipped |
| 3b | 0x35087 | `0F 85 35 07 00 00` | `90 90 90 90 90 90` | NOP `jnz` — menu render check 2 skipped |

**Result:** any key (or no key at all) works. No license. Server validation is
ignored. Menu renders regardless of license status.

---

## STEP 6 — Permanent menu patch (7 patches A–G)

**Script:** `patch_dll_final.ps1` (idempotent, safe to re-run)
**Target:** `decrypted_payload_cracked.dll`
**Backup:** `decrypted_payload_cracked.dll.bak`

This is the patch that makes the menu **appear automatically** without
INSERT. RVA = FO + 0xC00.

| # | FO | RVA | Original | Patched | Function |
|---|---|---|---|---|---|
| A | 0x05F5E4 | 0x0601E4 | `74 EA` | `90 90` | init-thread wait-loop-2: `call[vtable+0xA8]==0` → `jz` loop. NOP = exit loop → init-complete=1 |
| B | 0x05E82C | 0x05F42C | `0F 84 DC 01 00 00` | NOP x6 | render chain `FUN_1005f310`: `call[vtable+0xA8]==0` → `jz` bail. NOP = menu keeps rendering |
| C | 0x05E2FE | 0x05EEFE | `0F 84 71 01 00 00` | NOP x6 | `FUN_1005eec0` GATE 1: `call[vtable+0x9C]==0` → `jz` return. NOP = continue to `FUN_10035bc0` (menu render) |
| D | 0x05D82A | 0x05E42A | `75 10` | `EB 10` | `FUN_1005e3f0`: `jnz`→`jmp`, SKIP the menu-visible refresh from `method_0x20` (returns 0) |
| E | 0x035830 | 0x036430 | `75 12` | `EB 12` | `FUN_10036420`: `jnz`→`jmp`, SKIP the menu-visible refresh from `method_0x20` (returns 0) |
| F | 0x05F65D | 0x06025D | `75 B1` | `90 90` | init-thread wait-loop-3 (state 2/4/5): `jnz` loop. NOP = init thread finishes |
| G | 0x031CE5 | 0x0328E5 | `00 00 00 00` | `01 00 00 00` | init: `mov [menu-visible], 0` → `mov [menu-visible], 1` — menu starts visible |

Re-patch any time:

```powershell
powershell -ExecutionPolicy Bypass -File patch_dll_final.ps1
```

---

## STEP 7 — Pre-encrypt the cracked DLL for the loader

**Script:** `PB_Sumatra\preencrypt.ps1`
**Source:** `decrypted_payload_cracked.dll`
**Output:** `PB_Sumatra\payload_preencrypted.bin`

The loader XOR-decrypts the payload with the same key. So we pre-encrypt the
already-cracked DLL with the same key — when the loader decrypts it, the
result is the cracked DLL.

```powershell
$key = [System.Text.Encoding]::ASCII.GetBytes('325768235246856876125')
for ($i = 0; $i -lt $crackedDll.Length; $i++) {
    $preEncrypted[$i] = $crackedDll[$i] -bxor $key[$i % $key.Length]
}
```

---

## STEP 8 — One-click crack (runtime replace)

**Script:** `PB_Sumatra\oneclick_crack.ps1` (basic)
**Script:** `PB_Sumatra\oneclick_logged.ps1` (full logging + diagnosis)

This is what's used every time you want to play via the loader (instead of
manual inject):

1. Snapshot the files in `%TEMP%`
2. Start `LoaderVIP_cracked.exe`
3. Watch `%TEMP%` for a new PE whose size matches (±1024/4096 bytes) + `MZ` header
4. **Replace** that file with `decrypted_payload_cracked.dll`
5. If the loader rewrites / appends a trailer → re-verify the patch bytes, RE-REPLACE if it reverted
6. Start PointBlank → the loader injects the cracked DLL

`oneclick_logged.ps1` adds: auto-relaunch as ADMIN, capture loader
stdout/stderr, track new processes (CB.exe, ZLauncher child), log DLL drops +
MD5 before/after, PB start (PID/path/version/window title), privilege test
(QUERY_LIMITED / QUERY_INFO / ALL_ACCESS), module snapshot every 2 seconds as
PROOF of inject, remote-desktop detector (UltraViewer/TeamViewer/AnyDesk can
hide the menu), plus SUMMARY + VERDICT diagnosis at the end.

Log: `PB_Sumatra\logs\run_YYYYMMDD_HHMMSS.log`

---

## STEP 9 — Daily inject (cheat works)

After all the cracks above, the cheat is permanent. Daily use only needs
inject:

1. Open Point Blank, enter the **lobby**
2. **Double-click `PBInjectorGUI.exe`**
3. Click **INJECT** (click YES if UAC appears — the new window auto-injects)
4. The menu **appears automatically** — no INSERT needed, no force_menu.ps1 needed

Script alternative:

```powershell
powershell -ExecutionPolicy Bypass -File inject_v2.ps1
```

The GUI shows live status: whether PointBlank is running, admin or not,
whether the DLL is valid, plus a full log (same as `logs\inject_out.txt`).

In-game hotkeys:
- `INSERT` — open/close the menu
- `UP` / `DOWN` — navigation
- `ENTER` — toggle / fire
- `LEFT` / `RIGHT` — adjust slider
- `END` — panic (kill ESP + hide the menu)

---

## Crack Flow Summary

```
Loadertrial.exe ──crack_loader.ps1──────────► Loadertrial_cracked.exe
                                                    │ download
                                                    ▼
                                    %TEMP%\LoaderTrialTemp\LoaderVIP.exe
                                                    │
                            crack_loaderVIP.ps1 ────┤  (serial + size + timer)
                            patch_loader_cb.ps1 ────┘  (CB.exe killswitch)
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
                         crack_dll_key.ps1 ─────────┤  (key/license bypass)
                         patch_dll_final.ps1 ───────┘  (7 patch A–G, menu auto)
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

## Folder Structure (repo)

This repo is self-contained — the crack chain has been copied into
`PB_Sumatra\` inside the repo, so one clone gets everything.

```
pb-loader-toolkit\                <- REPO ROOT
|   README.md                     <- full documentation (crack chain + tools + credits)
|   LICENSE                       <- MIT, (c) dekkaaa9
|   .gitignore
|   .gitattributes                <- binary protection for exe/dll/bin
|   build.bat                     <- build everything (PBCheat + PBInjector + GUI)
|
|   --- DAILY INJECT ---
|   PBInjectorGUI.exe             <- daily GUI injector (double-click, click INJECT)
|   PBInjectorGUI\main.cpp        <- GUI source (dark theme, DPI-aware)
|   PBInjector.exe                <- manual map injector
|   PBInjector\main.cpp           <- injector source
|   inject_v2.ps1                 <- injector script (GUI alternative)
|   force_menu.ps1                <- backup runtime patcher (not needed daily)
|
|   --- PAYLOAD + CHEAT ---
|   decrypted_payload_cracked.dll     <- patched VIP payload (7 patches A-G)
|   decrypted_payload_cracked.dll.bak <- pre-menu-patch backup
|   patch_dll_final.ps1           <- re-patch the DLL from scratch (idempotent)
|   PBCheat.dll                   <- cheat build artifact
|   PBCheat\                      <- 16 source files (bypass, hooks, entities, esp, gui, menu)
|
|   --- CRACK CHAIN (flat) ---
|   PB_Sumatra\
|   |   LoaderVIP_cracked.exe       <- FINAL loader (all patches applied)
|   |   LoaderVIP_cracked_precbpatch.exe.bak
|   |   Loadertrial_cracked.exe
|   |   crack_loader.ps1            <- crack Loadertrial (IL brfalse target)
|   |   crack_loaderVIP.ps1         <- crack LoaderVIP (serial + size + timer)
|   |   crack_dll_key.ps1           <- crack the key string in the payload DLL
|   |   patch_loader_cb.ps1         <- CB-patch the loader (post-crack)
|   |   fix_loader.ps1              <- loader fixer
|   |   oneclick_crack.ps1          <- crack chain orchestrator
|   |   oneclick_logged.ps1         <- oneclick + logging + diagnosis
|   |   preencrypt.ps1              <- re-encrypt the payload for the loader
|   |   verify_patches.ps1          <- verify patch bytes in the loader
|   |   decrypt_payload.ps1         <- decrypt the downloaded payload
|   |   decrypt_dll_strings.ps1     <- decrypt the string table in the DLL
|   |   decrypt_strings.ps1
|   |   decrypt_size_error.ps1
|   |   find_size_xref.ps1          <- RE helper: find the size check xref
|   |   find_error_strings.ps1      <- RE helper: find the error string
|   |   find_timer.ps1              <- RE helper: find the timer logic
|   |   downloaded_payload.bin      <- original payload (encrypted)
|   |   decrypted_payload.dll       <- decrypted payload (pre-key-crack)
|   |   payload_preencrypted.bin    <- re-encrypted payload for the loader
|   |   logs\                       <- oneclick_logged output
|
|   logs\                       <- inject/force_menu log output (gitignored)
```

> **Path note:** every script derives its own root from `$PSScriptRoot`, and
> the GUI resolves its paths relative to its own executable. Nothing is
> hardcoded to a specific drive or folder — extract the repo anywhere and it
> runs.

---

## Engine Intel (from REA/Ghidra)

| Target | Offset (from DLL base) | Purpose |
|--------|------------------------|---------|
| `i3SceneDx.dll` `i3SceneGraphContext::OnUpdate` | `0xFD880` | Hook to capture scene context + tracer |
| `i3SceneDx.dll` OnUpdate vtable slot | `0x1C850C` | Slot storing `&OnUpdate` — patched directly |
| `i3SceneDx.dll` `i3SceneObject::CalcBoneWorldMatrix` | `0x107350` | Bone world positions |
| `i3SceneDx.dll` `i3SceneGraphContext::get3DMainRoot` | `0x2D920` | 3D scene root node |
| `i3SceneDx.dll` `i3SceneTracer::getViewProjectionMatrix` | `0x2DF00` | VP matrix at tracer+0x150 |
| `i3GfxDx.dll` `i3RenderContext::GetNativeDevice` | `0x10740` | D3D9 device pointer |
| `i3GfxDx.dll` `i3RenderContext::End` | `0x6CBF0` | Engine render loop end |

## Build

```
build.bat
```

Requires: **llvm-mingw** (i686) at `D:\llvm-mingw\`, **MinGW64** at `D:\mingw64\`
(for the 64-bit injector). Output: `PBCheat.dll` + `PBInjector.exe` + `PBInjectorGUI.exe`.

## Tools Used

All the tools below were used throughout the crack chain + development:

### Reverse Engineering & Analysis

| Tool | Used for |
|---|---|
| **REA** | Decompilation + analysis of `i3SceneDx.dll` / `i3GfxDx.dll`, finding engine offsets (OnUpdate, CalcBoneWorldMatrix, getViewProjectionMatrix, etc), hook points, vtable slots |
| **Ghidra** | Cross-checking decompilation, analyzing loader functions (`FUN_00404d50` CB watchdog, serial/size/timer check), finding patch points in LoaderVIP.exe + the payload DLL |
| **PowerShell 5.1** | RE helper scripts: `find_size_xref.ps1`, `find_error_strings.ps1`, `find_timer.ps1`, `decrypt_strings.ps1`, `decrypt_dll_strings.ps1`, `decrypt_size_error.ps1` — byte scan, xref hunt, string decrypt |
| **PowerShell 5.1** | Byte-level patching for all crack scripts (verify-then-patch, idempotent) |

### Build & Compiler

| Tool | Used for |
|---|---|
| **llvm-mingw** (`i686-w64-mingw32-clang++`, 20261020-msvcrt-x86_64) | Build `PBCheat.dll`, `PBInjector.exe`, `PBInjectorGUI.exe` — static, `-fno-exceptions -fno-rtti -O2 -std=c++17`, target **i686 (32-bit)** because PB is x86 |
| **MinGW64** | 64-bit toolchain (`D:\mingw64\`) |
| **libunwind.a** | Static unwind, avoids a `libunwind.dll` dependency |

### API & SDK

| Tool | Used for |
|---|---|
| **Win32 API** | GUI (`PBInjectorGUI` — owner-drawn, DPI-aware, DWM dark mode + rounded corners), injector (`OpenProcess`/`VirtualAllocEx`/`WriteProcessMemory`/`CreateRemoteThread`), module scan (`Toolhelp32`), UAC elevation (`ShellExecuteEx` runas) |
| **DirectX 9** (`d3d9.lib`, D3DXLine/D3DXFont) | ESP rendering, EndScene vtable hook |
| **Windows SDK** (`psapi`, `advapi32`, `shellapi`, `dwmapi`) | EnumProcessModules, token membership (admin check), shell exec, DWM attributes |

### Environment

| Tool | Used for |
|---|---|
| **Windows 11** | Target OS (WOW64 — PB x86 on an x64 system) |
| **OpenCode** | The agent environment where this entire crack chain, injector, GUI, and documentation was done |

## How It Works

- **Bypass**: kill CB.exe (Cheat Blocker) + watchdog re-kill 500ms; manual map
  (no LoadLibrary); clear PEB BeingDebugged + NtGlobalFlag.
- **Entities**: hook `OnUpdate` via a vtable slot patch (no inline hook, no
  instruction relocation); traverse the scene graph; player = 12-80 bones;
  head = bone 7.
- **Rendering**: D3D9 EndScene vtable hook; VP matrix from tracer+0x150
  (validated, fallback to device transforms); D3DXLine/D3DXFont.
- **Injector**: `LoadLibraryA` remote thread (not manual map) — the DLL is
  already cracked so no stealth is needed. Injector is 32-bit (i686) because
  PB is x86. ANSI path + `LoadLibraryA` keeps the WOW64 shared kernel32
  address valid.

## Tuning

Edit `PBCheat/config.h`: `headBoneIndex` (try 6/7/8), `minBoneCount`/
`maxBoneCount`, `maxDistance`, colors, hotkeys.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Menu doesn't appear | Re-run `patch_dll_final.ps1`, make sure you inject while in the lobby |
| Menu gets hidden | Close UltraViewer/TeamViewer/AnyDesk/VNC — the DLL hides the menu when the remote desktop detector is active |
| No entities | Widen the bone count, try a different `headBoneIndex` |
| Crash on inject | Run as Admin, inject when the game is fully loaded |
| `LoadLibraryA` returns 0 | The DLL refuses to load — missing dependency / file unreadable |
| Exit code `0xC0000005` | The injector is still 64-bit — click YES on UAC |
| CB.exe respawns | The watchdog handles it; inject faster |
| Loader looping "Login Point Blank" | The serial patch failed — verify with `verify_patches.ps1` |
| DLL in temp reverted | The loader overwrote it — `oneclick_logged.ps1` auto re-replaces |

---

## Credits

**Crack, development, and documentation by [dekkaaa9](https://github.com/dekkaaa9)**

| Part | Detail |
|---|---|
| Loader crack | `crack_loader.ps1` (IL patch), `crack_loaderVIP.ps1` (serial + filesize + timer), `patch_loader_cb.ps1` (CB.exe killswitch) |
| Payload crack | `decrypt_payload.ps1` (XOR key recovery), `crack_dll_key.ps1` (key/license bypass) |
| Permanent patch | `patch_dll_final.ps1` (7 patches A–G, menu auto-appear) |
| Orchestration | `oneclick_crack.ps1`, `oneclick_logged.ps1`, `preencrypt.ps1`, `verify_patches.ps1` |
| Injector | `inject_v2.ps1`, `PBInjector.exe`, `PBInjectorGUI.exe` + `PBInjectorGUI\main.cpp` |
| Cheat | `PBCheat\` (16 source files — bypass, hooks, entities, esp, gui, menu) |
| Tools | REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9 |

Repo: **https://github.com/dekkaaa9**
