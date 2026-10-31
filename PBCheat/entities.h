// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PBC_ENTITIES_H
#define PBC_ENTITIES_H

#include "sdk.h"
#include <vector>

namespace PB {
namespace Entities {

    struct Entity {
        Vec3  posFeet;
        Vec3  posHead;
        float distance;    // from local player / camera
        int   boneCount;
        bool  visible;     // placeholder — needs trace check
        void* sceneObj;    // raw pointer for dedup
    };

    // Called each frame from the OnUpdate hook after the original runs.
    // Traverses the scene graph and captures player-like entities.
    void CaptureFrame(void* sceneCtx);

    // Get the captured entity list (thread-safe snapshot)
    const std::vector<Entity>& GetEntities();

    // Clear the entity list
    void Clear();

} // namespace Entities
} // namespace PB

#endif