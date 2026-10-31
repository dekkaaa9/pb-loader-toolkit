// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#include "gui.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <map>
#include <string>

// ============================================================
//  Immediate-mode GUI — raw D3D9, no ImGui/D3DX dependency
//  Text rendered via GDI -> D3D9 texture (cached per string)
//  Keyboard-driven menu: no mouse state is tracked here.
// ============================================================

namespace PB {
namespace GUI {

    static IDirect3DDevice9* s_dev = nullptr;

    // ---- Text diagnostics (separate log to avoid clashing with hooks.cpp) ----
    static FILE* g_guiLog  = nullptr;
    static int   s_texOk   = 0;
    static int   s_texFail = 0;
    static void GLog(const char* fmt, ...) {
        if (!g_guiLog) g_guiLog = fopen("D:\\pbct\\gui_log.txt", "w");
        if (!g_guiLog) return;
        va_list ap; va_start(ap, fmt); vfprintf(g_guiLog, fmt, ap); va_end(ap);
        fflush(g_guiLog);
    }

    // ---- Text cache ----
    struct CachedText {
        IDirect3DTexture9* tex;
        int w, h;
    };
    static std::map<std::string, CachedText> s_textCache;

    // ---- Saved render states (restored in EndFrame) ----
    static DWORD s_oldLighting, s_oldZEnable, s_oldAlphaBlend;
    static DWORD s_oldSrcBlend, s_oldDestBlend, s_oldFVF;
    static DWORD s_oldColorOp, s_oldColorArg1, s_oldColorArg2;
    static DWORD s_oldAlphaOp, s_oldAlphaArg1, s_oldAlphaArg2;
    static DWORD s_oldMinFilter, s_oldMagFilter;
    static DWORD s_oldAddrU, s_oldAddrV;
    static DWORD s_oldTexTransform;

    // ---- Vertex formats ----
    struct LineVtx { float x, y, z, rhw; D3DCOLOR c; };
    struct TexVtx  { float x, y, z, rhw; D3DCOLOR c; float u, v; };

    static const DWORD FVF_LINE = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
    static const DWORD FVF_TEX  = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

    // ============================================================
    //  Text texture creation via GDI
    // ============================================================
    static IDirect3DTexture9* MakeTextTexture(const char* text, int fontSize,
                                               int* outW, int* outH)
    {
        *outW = *outH = 0;

        HDC hdcScreen = GetDC(nullptr);
        if (!hdcScreen) { GLog("[gui] GetDC(null) failed\n"); s_texFail++; return nullptr; }
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        if (!hdcMem) { ReleaseDC(nullptr, hdcScreen); GLog("[gui] CreateCompatibleDC failed\n"); s_texFail++; return nullptr; }

        // Try a sequence of font faces; use the first that measures non-zero.
        static const char* kFonts[] = { "Segoe UI", "Tahoma", "Arial", "MS Sans Serif" };
        HFONT hFont = nullptr;
        bool  fontCreated = false;
        SIZE  sz = { 0, 0 };
        for (const char* face : kFonts) {
            HFONT f = CreateFontA(fontSize, 0, 0, 0, FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
            if (!f) continue;
            HFONT old = (HFONT)SelectObject(hdcMem, f);
            SIZE s = { 0, 0 };
            GetTextExtentPoint32A(hdcMem, text, (int)strlen(text), &s);
            SelectObject(hdcMem, old);
            if (s.cx > 0 && s.cy > 0) { hFont = f; fontCreated = true; sz = s; break; }
            DeleteObject(f);
        }
        if (!hFont) {
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);   // stock — must NOT be deleted
            if (hFont) {
                HFONT old = (HFONT)SelectObject(hdcMem, hFont);
                GetTextExtentPoint32A(hdcMem, text, (int)strlen(text), &sz);
                SelectObject(hdcMem, old);
            }
        }
        if (!hFont || sz.cx < 1 || sz.cy < 1) {
            if (fontCreated) DeleteObject(hFont);
            DeleteDC(hdcMem); ReleaseDC(nullptr, hdcScreen);
            GLog("[gui] no usable font for '%s'\n", text);
            s_texFail++; return nullptr;
        }
        // Sanity guard — reject pathological sizes before allocating
        if (sz.cx > 2048 || sz.cy > 128) {
            if (fontCreated) DeleteObject(hFont);
            DeleteDC(hdcMem); ReleaseDC(nullptr, hdcScreen);
            GLog("[gui] size guard reject %dx%d\n", sz.cx, sz.cy);
            s_texFail++; return nullptr;
        }

        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = sz.cx;
        bmi.bmiHeader.biHeight      = -sz.cy;   // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hBmp || !bits) {
            if (fontCreated) DeleteObject(hFont);
            DeleteDC(hdcMem); ReleaseDC(nullptr, hdcScreen);
            GLog("[gui] CreateDIBSection failed\n");
            s_texFail++; return nullptr;
        }

        HFONT   hOldFont = (HFONT)SelectObject(hdcMem, hFont);
        HBITMAP hOldBmp  = (HBITMAP)SelectObject(hdcMem, hBmp);
        SetTextColor(hdcMem, RGB(255, 255, 255));
        SetBkColor(hdcMem, RGB(0, 0, 0));
        SetBkMode(hdcMem, OPAQUE);
        TextOutA(hdcMem, 0, 0, text, (int)strlen(text));

        // Verify the glyph bitmap actually lit up (catches silent font failures)
        int lit = 0;
        {
            DWORD* s0 = (DWORD*)bits;
            for (int i = 0; i < sz.cx * sz.cy; i++)
                if (((s0[i] >> 16) & 0xFF) > 8) lit++;
        }

        IDirect3DTexture9* tex = nullptr;
        if (lit == 0) {
            GLog("[gui] empty glyph bitmap for '%s'\n", text);
            s_texFail++;
        }
        else {
            HRESULT hr = s_dev->CreateTexture(sz.cx, sz.cy, 1, 0,
                D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr);
            if (FAILED(hr) || !tex) {
                GLog("[gui] CreateTexture hr=0x%08lX %dx%d\n", (unsigned long)hr, sz.cx, sz.cy);
                tex = nullptr; s_texFail++;
            } else {
                D3DLOCKED_RECT lr;
                HRESULT lhr = tex->LockRect(0, &lr, nullptr, 0);
                if (FAILED(lhr)) {
                    GLog("[gui] LockRect hr=0x%08lX\n", (unsigned long)lhr);
                    tex->Release(); tex = nullptr; s_texFail++;
                } else {
                    DWORD* src = (DWORD*)bits;
                    DWORD* dst = (DWORD*)lr.pBits;
                    int pitch = lr.Pitch / 4;
                    for (int yy = 0; yy < sz.cy; yy++) {
                        for (int xx = 0; xx < sz.cx; xx++) {
                            DWORD px = src[yy * sz.cx + xx];
                            BYTE a = (BYTE)((px >> 16) & 0xFF);   // red = intensity
                            dst[yy * pitch + xx] = ((DWORD)a << 24) | 0x00FFFFFF;
                        }
                    }
                    tex->UnlockRect(0);
                    s_texOk++;
                    GLog("[gui] tex ok '%s' %dx%d lit=%d\n", text, sz.cx, sz.cy, lit);
                }
            }
        }

        SelectObject(hdcMem, hOldBmp);
        SelectObject(hdcMem, hOldFont);
        DeleteObject(hBmp);
        if (fontCreated) DeleteObject(hFont);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);

        *outW = sz.cx;
        *outH = sz.cy;
        return tex;
    }

    // ---- Draw a cached text quad ----
    static void DrawTextQuad(IDirect3DTexture9* tex, float x, float y,
                             int w, int h, D3DCOLOR color)
    {
        TexVtx v[4] = {
            { x,     y,     0, 1, color, 0.f, 0.f },
            { x + w, y,     0, 1, color, 1.f, 0.f },
            { x,     y + h, 0, 1, color, 0.f, 1.f },
            { x + w, y + h, 0, 1, color, 1.f, 1.f },
        };
        s_dev->SetTexture(0, tex);
        s_dev->SetFVF(FVF_TEX);
        s_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(TexVtx));
        s_dev->SetTexture(0, nullptr);
    }

    // ============================================================
    //  Init / Frame
    // ============================================================
    void Init(IDirect3DDevice9* dev)
    {
        s_dev = dev;
    }

    void BeginFrame(IDirect3DDevice9* dev)
    {
        s_dev = dev;

        // ---- Save render states ----
        dev->GetRenderState(D3DRS_LIGHTING,        &s_oldLighting);
        dev->GetRenderState(D3DRS_ZENABLE,         &s_oldZEnable);
        dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&s_oldAlphaBlend);
        dev->GetRenderState(D3DRS_SRCBLEND,        &s_oldSrcBlend);
        dev->GetRenderState(D3DRS_DESTBLEND,       &s_oldDestBlend);
        dev->GetFVF(&s_oldFVF);
        dev->GetTextureStageState(0, D3DTSS_COLOROP,   &s_oldColorOp);
        dev->GetTextureStageState(0, D3DTSS_COLORARG1, &s_oldColorArg1);
        dev->GetTextureStageState(0, D3DTSS_COLORARG2, &s_oldColorArg2);
        dev->GetTextureStageState(0, D3DTSS_ALPHAOP,   &s_oldAlphaOp);
        dev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &s_oldAlphaArg1);
        dev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &s_oldAlphaArg2);
        dev->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &s_oldTexTransform);
        dev->GetSamplerState(0, D3DSAMP_MINFILTER, &s_oldMinFilter);
        dev->GetSamplerState(0, D3DSAMP_MAGFILTER, &s_oldMagFilter);
        dev->GetSamplerState(0, D3DSAMP_ADDRESSU,  &s_oldAddrU);
        dev->GetSamplerState(0, D3DSAMP_ADDRESSV,  &s_oldAddrV);

        // ---- Set 2: overlay states ----
        dev->SetRenderState(D3DRS_LIGHTING,         FALSE);
        dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);

        dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
    }

    void EndFrame()
    {
        if (!s_dev) return;
        s_dev->SetRenderState(D3DRS_LIGHTING,         s_oldLighting);
        s_dev->SetRenderState(D3DRS_ZENABLE,          s_oldZEnable);
        s_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, s_oldAlphaBlend);
        s_dev->SetRenderState(D3DRS_SRCBLEND,         s_oldSrcBlend);
        s_dev->SetRenderState(D3DRS_DESTBLEND,        s_oldDestBlend);
        s_dev->SetFVF(s_oldFVF);
        s_dev->SetTextureStageState(0, D3DTSS_COLOROP,   s_oldColorOp);
        s_dev->SetTextureStageState(0, D3DTSS_COLORARG1, s_oldColorArg1);
        s_dev->SetTextureStageState(0, D3DTSS_COLORARG2, s_oldColorArg2);
        s_dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   s_oldAlphaOp);
        s_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, s_oldAlphaArg1);
        s_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, s_oldAlphaArg2);
        s_dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, s_oldTexTransform);
        s_dev->SetSamplerState(0, D3DSAMP_MINFILTER, s_oldMinFilter);
        s_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, s_oldMagFilter);
        s_dev->SetSamplerState(0, D3DSAMP_ADDRESSU,  s_oldAddrU);
        s_dev->SetSamplerState(0, D3DSAMP_ADDRESSV,  s_oldAddrV);
        s_dev->SetTexture(0, nullptr);
    }

    // ============================================================
    //  Primitives (each sets its own FVF so they are order-safe)
    // ============================================================
    void FilledRect(float x, float y, float w, float h, D3DCOLOR color)
    {
        LineVtx v[4] = {
            { x,     y,     0, 1, color },
            { x + w, y,     0, 1, color },
            { x,     y + h, 0, 1, color },
            { x + w, y + h, 0, 1, color },
        };
        s_dev->SetFVF(FVF_LINE);
        s_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(LineVtx));
    }

    void GradientRect(float x, float y, float w, float h,
                      D3DCOLOR colTop, D3DCOLOR colBot)
    {
        LineVtx v[4] = {
            { x,     y,     0, 1, colTop },
            { x + w, y,     0, 1, colTop },
            { x,     y + h, 0, 1, colBot },
            { x + w, y + h, 0, 1, colBot },
        };
        s_dev->SetFVF(FVF_LINE);
        s_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(LineVtx));
    }

    void OutlinedRect(float x, float y, float w, float h, D3DCOLOR color)
    {
        LineVtx v[8] = {
            { x,     y,     0, 1, color }, { x + w, y,     0, 1, color },
            { x + w, y,     0, 1, color }, { x + w, y + h, 0, 1, color },
            { x + w, y + h, 0, 1, color }, { x,     y + h, 0, 1, color },
            { x,     y + h, 0, 1, color }, { x,     y,     0, 1, color },
        };
        s_dev->SetFVF(FVF_LINE);
        s_dev->DrawPrimitiveUP(D3DPT_LINELIST, 4, v, sizeof(LineVtx));
    }

    void Line(float x1, float y1, float x2, float y2, D3DCOLOR color)
    {
        LineVtx v[2] = {
            { x1, y1, 0, 1, color },
            { x2, y2, 0, 1, color },
        };
        s_dev->SetFVF(FVF_LINE);
        s_dev->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(LineVtx));
    }

    void Text(const char* str, float x, float y, D3DCOLOR color, bool shadow)
    {
        if (!s_dev || !str || !*str) return;

        auto it = s_textCache.find(str);
        if (it == s_textCache.end()) {
            int w, h;
            IDirect3DTexture9* tex = MakeTextTexture(str, 13, &w, &h);
            CachedText ct = { tex, w, h };
            // Cache the result even when tex is null, so a failure doesn't
            // re-run the GDI path (and re-log) every single frame.
            it = s_textCache.emplace(str, ct).first;
        }

        CachedText& ct = it->second;
        if (!ct.tex) return;

        if (shadow)
            DrawTextQuad(ct.tex, x + 1, y + 1, ct.w, ct.h, 0xFF000000);
        DrawTextQuad(ct.tex, x, y, ct.w, ct.h, color);
    }

    float TextWidth(const char* str)
    {
        if (!str || !*str) return 0.f;
        auto it = s_textCache.find(str);
        if (it != s_textCache.end()) return (float)it->second.w;

        // Measure without caching
        HDC hdc = GetDC(nullptr);
        HFONT hFont = CreateFontA(13, 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);
        SIZE sz;
        GetTextExtentPoint32A(hdc, str, (int)strlen(str), &sz);
        SelectObject(hdc, hOld);
        DeleteObject(hFont);
        ReleaseDC(nullptr, hdc);
        return (float)sz.cx;
    }

    // ---- Text diagnostics accessors ----
    int GetTexOk()   { return s_texOk; }
    int GetTexFail() { return s_texFail; }

} // namespace GUI
} // namespace PB