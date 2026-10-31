// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PBC_ESP_H
#define PBC_ESP_H

#include <d3d9.h>
#include "sdk.h"

namespace PB {
namespace ESP {

    // Render ESP overlay. Called from EndScene hook.
    void Render(IDirect3DDevice9* dev, const MATRIX& vp, int scrW, int scrH);

} // namespace ESP
} // namespace PB

#endif