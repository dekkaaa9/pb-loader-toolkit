// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PBC_SDK_H
#define PBC_SDK_H

#include <windows.h>
#include <d3d9.h>

// ============================================================
//  Point Blank ID — i3Framework Engine SDK
//  Reverse-engineered offsets from i3SceneDx.dll / i3GfxDx.dll
// ============================================================

namespace PB {

// ---- i3SceneGraphContext (i3SceneDx.dll, base 0x10000000) ----
constexpr DWORD OFF_SGCTX_3D_ROOT      = 0x28;   // i3Node* get3DMainRoot()
constexpr DWORD OFF_SGCTX_LIGHT_LIST   = 0x08;
constexpr DWORD OFF_SGCTX_CAMERA_LIST  = 0x10;
constexpr DWORD OFF_SGCTX_FRAME_CNT    = 0xA8;

// ---- i3Node ----
constexpr DWORD OFF_NODE_CHILD_START   = 0x10;   // i3Node** child array begin
constexpr DWORD OFF_NODE_CHILD_END     = 0x14;   // i3Node** child array end
// GetChild(i) = *(i3Node**)(*(DWORD*)(node + 0x10) + i * 4)
// GetChildCount() = (*(DWORD*)(node + 0x14) - *(DWORD*)(node + 0x10)) >> 2

// ---- i3SceneObject (extends i3Node) ----
constexpr DWORD OFF_SCENEOBJ_BODY      = 0x6C;   // i3Body*
constexpr DWORD OFF_SCENEOBJ_WORLD_SRC = 0x70;   // i3Transform* world space source
constexpr DWORD OFF_SCENEOBJ_BONELIST  = 0x64;   // i3BoneRef** bone list
// Bone count = (boneList[1] - boneList[0]) >> 2

// ---- i3SceneTracer ----
constexpr DWORD OFF_TRACER_VP_MATRIX   = 0x150;  // MATRIX (64 bytes)

// ---- i3RenderContext (i3GfxDx.dll) ----
constexpr DWORD OFF_RCTX_GET_VIEW_MTX  = 0x10A70; // i3RenderContext::GetViewMatrix
constexpr DWORD OFF_RCTX_GET_PROJ_MTX  = 0x10860; // i3RenderContext::GetProjectMatrix
constexpr DWORD OFF_RCTX_GET_NATIVE_DEV= 0x10740; // i3RenderContext::GetNativeDevice
constexpr DWORD OFF_RCTX_END           = 0x6CBF0; // i3RenderContext::End

// ---- i3SceneGraphContext::OnUpdate (hook target) ----
constexpr DWORD OFF_SGCTX_ONUPDATE     = 0xFD880; // void OnUpdate(i3SceneTracer*)
// VTable slot (RVA) that STORES &i3SceneGraphContext::OnUpdate.
// Verified against the shipped i3SceneDx.dll: OnUpdate's VA (0x100FD880)
// appears exactly once in the whole binary, inside the i3SceneGraphContext
// vtable (45/48 neighbouring entries are valid code pointers). We patch this
// single slot — no inline hook, no instruction relocation.
constexpr DWORD OFF_SGCTX_VT_ONUPDATE  = 0x1C850C;

// ---- i3SceneObject::CalcBoneWorldMatrix (hook target) ----
constexpr DWORD OFF_SCENEOBJ_CALC_BONE = 0x107350; // void CalcBoneWorldMatrix(int, MATRIX*, int)

// ---- i3SceneObject::GetWorldSpaceSourceMatrix ----
constexpr DWORD OFF_SCENEOBJ_GET_WORLD_MTX = 0x107800;

// ---- D3D9 Device vtable indices ----
constexpr int  VTIDX_RESET              = 16;
constexpr int  VTIDX_PRESENT            = 17;
constexpr int  VTIDX_ENDSCENE           = 42;
constexpr int  VTIDX_DRAW_INDEXED_PRIM  = 82;

// ---- MATRIX (row-major 4x4 float, 64 bytes) ----
// Layout matches D3DXMATRIX:
//   [0..3]   = _11 _12 _13 _14
//   [4..7]   = _21 _22 _23 _24
//   [8..11]  = _31 _32 _33 _34
//   [12..15] = _41 _42 _43 _44
struct MATRIX {
    float m[16];
    float& operator[](int i) { return m[i]; }
    const float& operator[](int i) const { return m[i]; }
};

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float _x, float _y) : x(_x), y(_y) {}
};

// Bone indices (tunable — may vary per model)
constexpr int BONE_ROOT = 0;   // usually feet/root
constexpr int BONE_HEAD = 7;   // usually head — configurable

// Entity filter defaults
constexpr int  MIN_BONE_COUNT = 12;
constexpr int  MAX_BONE_COUNT = 80;
constexpr float MAX_ESP_DIST  = 200.0f;  // meters
constexpr int  MAX_TRAVERSE_DEPTH = 24;
constexpr int  MAX_TRAVERSE_NODES = 8192;

} // namespace PB

#endif // PBC_SDK_H