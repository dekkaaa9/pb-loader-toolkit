// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PBC_MATH_H
#define PBC_MATH_H

#include "sdk.h"
#include <math.h>

namespace PB {

inline bool WorldToScreen(const Vec3& world, Vec2& screen,
                          const MATRIX& vp, int scrW, int scrH)
{
    float clipX = world.x * vp[0] + world.y * vp[4] + world.z * vp[8]  + vp[12];
    float clipY = world.x * vp[1] + world.y * vp[5] + world.z * vp[9]  + vp[13];
    float clipW = world.x * vp[3] + world.y * vp[7] + world.z * vp[11] + vp[15];

    if (clipW < 0.01f)
        return false;

    float invW = 1.0f / clipW;
    float ndcX = clipX * invW;
    float ndcY = clipY * invW;

    screen.x = (ndcX + 1.0f) * 0.5f * (float)scrW;
    screen.y = (1.0f - ndcY) * 0.5f * (float)scrH;
    return true;
}

inline float Distance3D(const Vec3& a, const Vec3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
}

inline Vec3 MatrixPosition(const MATRIX& m)
{
    return Vec3(m[12], m[13], m[14]);
}

// Safe memory readability check (replaces __try/__except for portability)
inline bool IsReadable(const void* addr, size_t size)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) != sizeof(mbi))
        return false;
    if (mbi.State != MEM_COMMIT)
        return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return false;
    uintptr_t end = (uintptr_t)addr + size;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return end <= regionEnd;
}

} // namespace PB

#endif