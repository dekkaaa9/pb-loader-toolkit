// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PB_GUI_H
#define PB_GUI_H

#include <d3d9.h>

namespace PB {
namespace GUI {

    // Call once when the device is first available
    void Init(IDirect3DDevice9* dev);

    // Call at start of overlay rendering (saves + sets 2D render states)
    void BeginFrame(IDirect3DDevice9* dev);

    // Call at end of overlay rendering (restores render states)
    void EndFrame();

    // ---- Primitives ----
    void FilledRect(float x, float y, float w, float h, D3DCOLOR color);
    void OutlinedRect(float x, float y, float w, float h, D3DCOLOR color);
    void GradientRect(float x, float y, float w, float h,
                      D3DCOLOR colTop, D3DCOLOR colBot);
    void Line(float x1, float y1, float x2, float y2, D3DCOLOR color);
    void Text(const char* str, float x, float y, D3DCOLOR color,
              bool shadow = true);
    float TextWidth(const char* str);

    // Text diagnostics (texture creation success/failure counts)
    int GetTexOk();
    int GetTexFail();

} // namespace GUI
} // namespace PB

#endif