// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#include "hooks.h"
#include "sdk.h"
#include "config.h"
#include "math.h"
#include "entities.h"
#include "esp.h"
#include "menu.h"
#include "gui.h"
#include "bypass.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdio>
#include <cstdarg>

namespace PB {

// ---- DLL-side logger (writes to file since we can't see game console) ----
static FILE* g_dllLog = nullptr;
static void DLog(const char* fmt, ...) {
    if (!g_dllLog) {
        g_dllLog = fopen("D:\\pbct\\cheat_log.txt", "w");
    }
    if (!g_dllLog) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_dllLog, fmt, args);
    va_end(args);
    fflush(g_dllLog);
}

// ---- Dump all loaded modules in the current process ----
static void DumpModules()
{
    DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) {
        DLog("[-] Module snapshot failed: %d\n", (int)GetLastError());
        return;
    }
    MODULEENTRY32W me = { sizeof(me) };
    if (Module32FirstW(snap, &me)) {
        do {
            DLog("    MOD: %ls (0x%p)\n", me.szModule, me.modBaseAddr);
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
}

// ---- Global state ----
static IDirect3DDevice9*   g_device        = nullptr;
static void*               g_pSceneCtx     = nullptr;  // i3SceneGraphContext*
static void*               g_pTracer       = nullptr;  // i3SceneTracer*
static MATRIX              g_vpMatrix      = {};
static Vec3                g_localPos      = {};   // camera world position
static int                 g_scrW          = 0;
static int                 g_scrH          = 0;
static bool                g_esHooked      = false;
static bool                g_ouHooked      = false;

// Original function pointers
typedef HRESULT (WINAPI* fnEndScene)(IDirect3DDevice9*);
// __fastcall with dummy EDX matches __thiscall layout for the trampoline call
typedef void   (__fastcall* fnOnUpdate)(void* ctx, void* edx, void* tracer);

static fnEndScene  g_origEndScene  = nullptr;
static fnOnUpdate  g_origOnUpdate  = nullptr;
static void*       g_pEndSceneTramp = nullptr;  // trampoline for engine hook

// ---- D3D9 vtable hook helpers ----
static void** GetDeviceVTable(bool tryEx)
{
    void** vt = nullptr;

    if (tryEx) {
        // Try D3D9Ex first (different vtable layout)
        typedef HRESULT (WINAPI *fnD3DCreate9Ex)(UINT, IDirect3D9Ex**);
        HMODULE hD3d9 = GetModuleHandleA("d3d9.dll");
        if (hD3d9) {
            auto pCreateEx = (fnD3DCreate9Ex)GetProcAddress(hD3d9, "Direct3DCreate9Ex");
            if (pCreateEx) {
                IDirect3D9Ex* d3dEx = nullptr;
                if (SUCCEEDED(pCreateEx(D3D_SDK_VERSION, &d3dEx)) && d3dEx) {
                    D3DPRESENT_PARAMETERS pp = {};
                    pp.Windowed = TRUE;
                    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
                    pp.hDeviceWindow = GetDesktopWindow();
                    IDirect3DDevice9Ex* devEx = nullptr;
                    HRESULT hr = d3dEx->CreateDeviceEx(
                        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                        pp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                        &pp, nullptr, &devEx);
                    if (SUCCEEDED(hr) && devEx) {
                        vt = *(void***)devEx;
                        DLog("[*] D3D9Ex device vtable: 0x%p\n", vt);
                        devEx->Release();
                    } else {
                        DLog("[!] D3D9Ex CreateDeviceEx failed: 0x%X\n", hr);
                    }
                    d3dEx->Release();
                }
            }
        }
        if (vt) return vt;
        DLog("[*] D3D9Ex not available, falling back to D3D9\n");
    }

    // Standard D3D9
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return nullptr;

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetDesktopWindow();

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
        pp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);

    if (SUCCEEDED(hr) && dev) {
        vt = *(void***)dev;
        DLog("[*] D3D9 device vtable: 0x%p\n", vt);
        dev->Release();
    } else {
        DLog("[!] D3D9 CreateDevice failed: 0x%X\n", hr);
    }
    d3d->Release();
    return vt;
}

// Forward declaration (defined below)
static HRESULT WINAPI hkEndScene(IDirect3DDevice9* dev);

// Hook EndScene by scanning for the FUNCTION POINTER (not vtable ptr).
// This works even when D3D9 runtime creates per-device vtables.
static int HookByFunctionPointer()
{
    // Create dummy D3D9 device to obtain the EndScene function address
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { DLog("[-] Direct3DCreate9 failed\n"); return 0; }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetDesktopWindow();

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
        pp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    if (FAILED(hr) || !dev) {
        DLog("[-] Dummy CreateDevice failed: 0x%X\n", hr);
        d3d->Release();
        return 0;
    }

    void** vt = *(void***)dev;
    void* esFunc    = vt[PB::VTIDX_ENDSCENE];
    void* presFunc  = vt[17];
    void* beginFunc = vt[41];
    DLog("[*] fnEndScene=0x%p fnPresent=0x%p fnBeginScene=0x%p\n",
         esFunc, presFunc, beginFunc);

    dev->Release();
    d3d->Release();

    // d3d9.dll range for verification
    HMODULE hD3d9 = GetModuleHandleA("d3d9.dll");
    MODULEINFO mi = {};
    GetModuleInformation(GetCurrentProcess(), hD3d9, &mi, sizeof(mi));
    BYTE* d3d9Start = (BYTE*)hD3d9;
    BYTE* d3d9End   = d3d9Start + mi.SizeOfImage;

    auto isValidFn = [&](void* p) -> bool {
        BYTE* b = (BYTE*)p;
        return b >= d3d9Start && b < d3d9End;
    };

    // Scan ALL committed readable regions for the EndScene function pointer
    MEMORY_BASIC_INFORMATION mbi;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    BYTE* addr = (BYTE*)si.lpMinimumApplicationAddress;
    BYTE* end  = (BYTE*)si.lpMaximumApplicationAddress;
    HANDLE hSelf = GetCurrentProcess();
    int hookCount = 0;

    while (addr < end) {
        if (VirtualQuery(addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_COMMIT &&
                (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE |
                                PAGE_READONLY  | PAGE_EXECUTE_READ |
                                PAGE_WRITECOPY)) &&
                !(mbi.Protect & PAGE_GUARD)) {

                BYTE* regionEnd = addr + mbi.RegionSize;
                BYTE  chunk[4096];
                for (BYTE* p = addr; p < regionEnd; p += sizeof(chunk) - 8) {
                    SIZE_T toRead = regionEnd - p;
                    if (toRead > sizeof(chunk)) toRead = sizeof(chunk);
                    SIZE_T bytesRead = 0;
                    if (!ReadProcessMemory(hSelf, p, chunk, toRead, &bytesRead))
                        break;
                    for (SIZE_T i = 4; i + 4 <= bytesRead; i += 4) {
                        if (*(void**)(chunk + i) != esFunc) continue;

                        // Verify neighbours look like vtable entries
                        void* prev = *(void**)(chunk + i - 4);
                        void* next = (i + 8 <= bytesRead)
                            ? *(void**)(chunk + i + 4) : nullptr;
                        if (!isValidFn(prev)) continue;
                        if (next && !isValidFn(next)) continue;

                        BYTE* esAddr = p + i;
                        DLog("[*] Found EndScene vtable entry at 0x%p\n", esAddr);

                        // Hook it
                        DWORD oldProtect;
                        if (VirtualProtect(esAddr, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
                            if (!g_origEndScene)
                                g_origEndScene = (fnEndScene)esFunc;
                            *(void**)esAddr = (void*)&hkEndScene;
                            VirtualProtect(esAddr, sizeof(void*), oldProtect, &oldProtect);
                            hookCount++;
                            DLog("[+] Hooked EndScene #%d at 0x%p\n", hookCount, esAddr);
                        }
                    }
                }
            }
            addr += mbi.RegionSize;
        } else break;
    }
    DLog("[*] HookByFunctionPointer: %d hooks installed\n", hookCount);
    return hookCount;
}

// ---- EndScene hook ----
static HRESULT WINAPI hkEndScene(IDirect3DDevice9* dev)
{
    static int  callCount = 0;
    static bool guiInited = false;
    if (callCount < 10) {
        DLog("[*] hkEndScene CALLED! dev=0x%p count=%d\n", dev, callCount);
        callCount++;
    }

    if (!g_device) g_device = dev;
    if (!guiInited) { GUI::Init(dev); guiInited = true; }

    // Update screen dimensions
    D3DDEVICE_CREATION_PARAMETERS cp;
    if (SUCCEEDED(dev->GetCreationParameters(&cp))) {
        RECT rc;
        GetClientRect(cp.hFocusWindow, &rc);
        g_scrW = rc.right - rc.left;
        g_scrH = rc.bottom - rc.top;
    }

    // ---- Camera world position (from the device view matrix, best effort) ----
    // Feeds the entity distance filter/readout. If the camera is driven
    // through shaders this stays origin and the filter is skipped.
    {
        D3DMATRIX vm;
        if (SUCCEEDED(dev->GetTransform(D3DTS_VIEW, &vm))) {
            const float* V = (const float*)&vm;
            float cx = -(V[12]*V[0] + V[13]*V[1] + V[14]*V[2]);
            float cy = -(V[12]*V[4] + V[13]*V[5] + V[14]*V[6]);
            float cz = -(V[12]*V[8] + V[13]*V[9] + V[14]*V[10]);
            if (cx == cx && cy == cy && cz == cz &&
                cx > -1e6f && cx < 1e6f && cy > -1e6f && cy < 1e6f &&
                cz > -1e6f && cz < 1e6f) {
                g_localPos = Vec3(cx, cy, cz);
            } else {
                g_localPos = Vec3(0.f, 0.f, 0.f);
            }
        }
    }

    // ---- VP matrix fallback ----
    // The authoritative VP comes from the OnUpdate hook (tracer + 0x150).
    // Only rebuild it from device transforms if that hasn't produced a valid
    // matrix yet (e.g. before the engine hook fires, or if the offset is off).
    {
        bool haveTracerVP = (g_vpMatrix.m[0] != 0.f || g_vpMatrix.m[5] != 0.f ||
                             g_vpMatrix.m[10] != 0.f);
        if (!haveTracerVP) {
            D3DMATRIX vm, pm;
            if (SUCCEEDED(dev->GetTransform(D3DTS_VIEW, &vm)) &&
                SUCCEEDED(dev->GetTransform(D3DTS_PROJECTION, &pm))) {
                const float* V = (const float*)&vm;
                const float* P = (const float*)&pm;
                float* o = g_vpMatrix.m;
                for (int r = 0; r < 4; r++)
                    for (int c = 0; c < 4; c++) {
                        float s = 0.f;
                        for (int k = 0; k < 4; k++)
                            s += V[r * 4 + k] * P[k * 4 + c];
                        o[r * 4 + c] = s;
                    }
            }
        }
    }

    // Handle hotkeys
    static bool prevInsert = false;
    bool curInsert = (GetAsyncKeyState(g_cfg.toggleESPKey) & 0x8000) != 0;
    if (curInsert && !prevInsert) {
        DLog("[*] INSERT pressed! Toggling menu\n");
        Menu::Toggle();
    }
    prevInsert = curInsert;

    static bool prevEnd = false;
    bool curEnd = (GetAsyncKeyState(g_cfg.panicKey) & 0x8000) != 0;
    if (curEnd && !prevEnd) {
        g_cfg.espEnabled = false;
        if (Menu::IsVisible()) Menu::Toggle();
    }
    prevEnd = curEnd;

    // ---- Menu keyboard navigation (arrow keys / enter) ----
    Menu::Update();

    // Render ESP overlay (world-space)
    if (g_cfg.espEnabled && g_scrW > 0 && g_scrH > 0) {
        ESP::Render(dev, g_vpMatrix, g_scrW, g_scrH);
    }

    // Render mod menu (screen-space, on top)
    Menu::Render(dev);

    // ---- Periodic ESP-state log ----
    // The in-menu DEBUG text may be unreadable while the text renderer is
    // being fixed, so mirror the key stats into the log every ~3s.
    {
        static int s_frame = 0;
        if (++s_frame >= 180) {
            s_frame = 0;
            DLog("[esp] OnUpdate=%d Entities=%d VP=%d Cam=(%.0f,%.0f,%.0f)\n",
                 g_ouHooked ? 1 : 0,
                 (int)Entities::GetEntities().size(),
                 (g_vpMatrix.m[0] != 0.f || g_vpMatrix.m[5] != 0.f ||
                  g_vpMatrix.m[10] != 0.f) ? 1 : 0,
                 g_localPos.x, g_localPos.y, g_localPos.z);
        }
    }

    return g_origEndScene(dev);
}

// ---- Present hook (index 17, always called every frame) ----
typedef HRESULT (WINAPI* fnPresent)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
static fnPresent g_origPresent = nullptr;

static HRESULT WINAPI hkPresent(IDirect3DDevice9* dev, const RECT* src, const RECT* dst,
    HWND hwnd, const RGNDATA* dirty)
{
    static int callCount = 0;
    if (callCount < 10) {
        DLog("[*] hkPresent CALLED! dev=0x%p count=%d\n", dev, callCount);
        callCount++;
    }

    if (!g_device) g_device = dev;

    // Handle hotkeys
    static bool prevInsert = false;
    bool curInsert = (GetAsyncKeyState(g_cfg.toggleESPKey) & 0x8000) != 0;
    if (curInsert && !prevInsert) {
        DLog("[*] INSERT pressed! Toggling menu\n");
        Menu::Toggle();
    }
    prevInsert = curInsert;

    static bool prevEnd = false;
    bool curEnd = (GetAsyncKeyState(g_cfg.panicKey) & 0x8000) != 0;
    if (curEnd && !prevEnd) {
        g_cfg.espEnabled = false;
        if (Menu::IsVisible()) Menu::Toggle();
    }
    prevEnd = curEnd;

    // Render ESP + menu
    D3DDEVICE_CREATION_PARAMETERS cp;
    if (SUCCEEDED(dev->GetCreationParameters(&cp))) {
        RECT rc;
        GetClientRect(cp.hFocusWindow, &rc);
        g_scrW = rc.right - rc.left;
        g_scrH = rc.bottom - rc.top;
    }
    if (g_cfg.espEnabled && g_scrW > 0 && g_scrH > 0) {
        ESP::Render(dev, g_vpMatrix, g_scrW, g_scrH);
    }
    Menu::Render(dev);

    return g_origPresent(dev, src, dst, hwnd, dirty);
}

// ---- Engine OnUpdate hook ----
// Captures i3SceneGraphContext* and i3SceneTracer* each frame,
// then traverses the scene graph to capture entity positions.
static void __fastcall hkOnUpdate(void* ctx, void* /*edx*/, void* tracer)
{
    g_pSceneCtx = ctx;
    g_pTracer   = tracer;

    // Read VP matrix from tracer + 0x150. Validate before accepting so a wrong
    // offset can't poison g_vpMatrix — if rejected, the device-transform
    // fallback in EndScene keeps WorldToScreen working.
    if (IsReadable((BYTE*)tracer + PB::OFF_TRACER_VP_MATRIX, sizeof(MATRIX))) {
        MATRIX* vp = (MATRIX*)((BYTE*)tracer + PB::OFF_TRACER_VP_MATRIX);
        if (vp->m[15] > 0.9f && vp->m[15] < 1.1f &&
            (vp->m[0] != 0.f || vp->m[5] != 0.f) &&
            vp->m[0] == vp->m[0] && vp->m[5] == vp->m[5]) {
            memcpy(&g_vpMatrix, vp, sizeof(MATRIX));
        }
    }

    // Call original
    g_origOnUpdate(ctx, nullptr, tracer);

    // After original update, traverse scene graph to capture entities
    if (g_pSceneCtx) {
        Entities::CaptureFrame(g_pSceneCtx);
    }
}

// ---- Trampoline hook (for engine functions) ----
static void* CreateTrampoline(void* target, void* detour, int len)
{
    // Allocate executable memory near the target
    BYTE* tramp = (BYTE*)VirtualAlloc(nullptr, len + 5,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return nullptr;

    // Copy original bytes
    memcpy(tramp, target, len);

    // Append jump back to original + len
    tramp[len] = 0xE9;  // JMP rel32
    *(DWORD*)(tramp + len + 1) = (DWORD)((BYTE*)target + len - (tramp + len + 5));

    // Write detour jump at target
    DWORD oldProtect;
    VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(BYTE*)target = 0xE9;
    *(DWORD*)((BYTE*)target + 1) = (DWORD)((BYTE*)detour - (BYTE*)target - 5);
    VirtualProtect(target, 5, oldProtect, &oldProtect);

    return tramp;
}

// ---- VTable hook ----
static bool HookVTable(void** vt, int index, void* detour, void** original)
{
    DWORD oldProtect;
    if (!VirtualProtect(&vt[index], sizeof(void*), PAGE_READWRITE, &oldProtect))
        return false;
    *original = vt[index];
    vt[index] = detour;
    VirtualProtect(&vt[index], sizeof(void*), oldProtect, &oldProtect);
    return true;
}

// ---- Install the i3SceneGraphContext::OnUpdate vtable hook ----
// Patches the single vtable slot that stores &OnUpdate. No inline hook, no
// instruction relocation — just swap one pointer. Safe because the slot was
// verified to sit inside the i3SceneGraphContext vtable.
static void InstallOnUpdateHook()
{
    HMODULE hScene = GetModuleHandleA("i3SceneDx.dll");
    if (!hScene) { DLog("[-] i3SceneDx.dll not loaded yet\n"); return; }

    void** slot = (void**)((BYTE*)hScene + PB::OFF_SGCTX_VT_ONUPDATE);
    if (!IsReadable(slot, sizeof(void*))) {
        DLog("[-] OnUpdate vtable slot unreadable (0x%p)\n", (void*)slot);
        return;
    }

    void* orig = *slot;
    if (!orig) { DLog("[-] OnUpdate vtable slot empty\n"); return; }
    g_origOnUpdate = (fnOnUpdate)orig;

    DWORD oldProt;
    if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProt)) {
        *slot = (void*)&hkOnUpdate;
        VirtualProtect(slot, sizeof(void*), oldProt, &oldProt);
        g_ouHooked = true;
        DLog("[+] OnUpdate vtable hook installed: slot=0x%p orig=0x%p\n",
             (void*)slot, orig);
    } else {
        DLog("[-] VirtualProtect(OnUpdate slot) failed: %d\n", (int)GetLastError());
    }
}

// ---- Initialization ----
static DWORD WINAPI InitThread(LPVOID)
{
    DLog("[*] InitThread started\n");
    DLog("[*] Dumping loaded modules...\n");
    DumpModules();

    HMODULE hD3d9 = nullptr;
    for (int i = 0; i < 30; i++) {
        hD3d9 = GetModuleHandleA("d3d9.dll");
        if (hD3d9) break;
        Sleep(500);
    }
    if (!hD3d9) {
        DLog("[-] d3d9.dll NOT FOUND\n");
        return 0;
    }
    DLog("[+] d3d9.dll found at 0x%p\n", hD3d9);

    // --- Hook D3D9 EndScene by scanning for the function pointer ---
    // This is more reliable than vtable-ptr scanning because D3D9 on modern
    // Windows creates per-device vtables — the dummy device's vtable differs
    // from the game's.  But the EndScene FUNCTION address is identical.
    for (int attempt = 0; attempt < 10 && !g_esHooked; attempt++) {
        DLog("[*] Attempt %d: function-pointer scan...\n", attempt);
        int hooks = HookByFunctionPointer();
        if (hooks > 0) {
            g_esHooked = true;
            DLog("[+] EndScene hook active (%d vtable entries patched)\n", hooks);
            break;
        }
        Sleep(1000);
    }
    if (!g_esHooked)
        DLog("[-] Function-pointer scan FAILED after 10 attempts\n");

    // --- Hook i3SceneGraphContext::OnUpdate (vtable slot) ---
    // i3SceneDx.dll is a core engine DLL but can load slightly after d3d9;
    // poll for it, then patch the single vtable slot that holds &OnUpdate.
    for (int i = 0; i < 300 && !g_ouHooked; i++) {   // up to ~30s
        if (GetModuleHandleA("i3SceneDx.dll")) {
            InstallOnUpdateHook();
            if (g_ouHooked) break;
        }
        Sleep(100);
    }
    if (g_ouHooked)
        DLog("[+] OnUpdate hook active — entity capture online\n");
    else
        DLog("[-] OnUpdate hook NOT installed (i3SceneDx.dll never loaded?)\n");

    DLog("[*] InitThread done (esHooked=%d ouHooked=%d)\n", g_esHooked, g_ouHooked);
    return 0;
}


bool Hooks::Initialize()
{
    // Run bypass first
    Bypass::Initialize();

    // Spawn init thread
    HANDLE h = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
    return h != nullptr;
}

void Hooks::Shutdown()
{
    // Restore EndScene
    if (g_device && g_origEndScene) {
        void** vt = *(void***)g_device;
        DWORD old;
        VirtualProtect(&vt[PB::VTIDX_ENDSCENE], sizeof(void*), PAGE_READWRITE, &old);
        vt[PB::VTIDX_ENDSCENE] = (void*)g_origEndScene;
        VirtualProtect(&vt[PB::VTIDX_ENDSCENE], sizeof(void*), old, &old);
    }
    // Restore OnUpdate vtable slot
    if (g_ouHooked && g_origOnUpdate) {
        HMODULE hScene = GetModuleHandleA("i3SceneDx.dll");
        if (hScene) {
            void** slot = (void**)((BYTE*)hScene + PB::OFF_SGCTX_VT_ONUPDATE);
            DWORD old;
            if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
                *slot = (void*)g_origOnUpdate;
                VirtualProtect(slot, sizeof(void*), old, &old);
            }
        }
        g_ouHooked = false;
    }
    // Free trampoline
    if (g_pEndSceneTramp) {
        VirtualFree(g_pEndSceneTramp, 0, MEM_RELEASE);
    }
}

// ---- Accessors ----
IDirect3DDevice9* Hooks::GetDevice() { return g_device; }
void* Hooks::GetSceneContext() { return g_pSceneCtx; }
void* Hooks::GetSceneTracer() { return g_pTracer; }
MATRIX Hooks::GetVPMatrix() { return g_vpMatrix; }
Vec3   Hooks::GetLocalPos() { return g_localPos; }
int Hooks::GetScreenWidth() { return g_scrW; }
int Hooks::GetScreenHeight() { return g_scrH; }
bool Hooks::IsEndSceneHooked() { return g_esHooked; }
bool Hooks::IsOnUpdateHooked() { return g_ouHooked; }

} // namespace PB