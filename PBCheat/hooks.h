// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PBC_HOOKS_H
#define PBC_HOOKS_H

#include <d3d9.h>
#include "sdk.h"

namespace PB {
namespace Hooks {

    bool Initialize();
    void Shutdown();

    IDirect3DDevice9*  GetDevice();
    void*              GetSceneContext();
    void*              GetSceneTracer();
    MATRIX             GetVPMatrix();
    Vec3               GetLocalPos();
    int                GetScreenWidth();
    int                GetScreenHeight();
    bool               IsEndSceneHooked();
    bool               IsOnUpdateHooked();

} // namespace Hooks
} // namespace PB

#endif