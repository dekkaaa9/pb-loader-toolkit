// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PB_MENU_H
#define PB_MENU_H

#include <d3d9.h>

namespace PB {
namespace Menu {

    void Toggle();
    bool IsVisible();

    // Polls arrow/enter keys and updates selection + values.
    // Call once per frame from the render hook.
    void Update();

    // Called from EndScene hook after ESP render
    void Render(IDirect3DDevice9* dev);

} // namespace Menu
} // namespace PB

#endif