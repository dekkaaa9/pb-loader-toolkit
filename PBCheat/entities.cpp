// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#include "entities.h"
#include "hooks.h"
#include "config.h"
#include "math.h"

#include <mutex>

namespace PB {
namespace Entities {

    static std::vector<Entity> g_entities;
    static std::mutex           g_mutex;
    static Vec3                 g_localPos;  // camera position for distance calc

    // ---- Safe memory read helpers ----
    template <typename T>
    static bool SafeRead(void* addr, T& out)
    {
        if (!IsReadable(addr, sizeof(T)))
            return false;
        out = *(T*)addr;
        return true;
    }

    // Fast absolute value (avoids fabsf / cmath header issues)
    static inline float fast_abs(float v) { return v < 0.f ? -v : v; }

    // ---- Get bone world matrix via engine function ----
    // i3SceneObject::CalcBoneWorldMatrix(int boneIdx, MATRIX* out, int flags)
    typedef void (__thiscall* fnCalcBoneWorldMatrix)(void*, int, MATRIX*, int);

    static fnCalcBoneWorldMatrix GetCalcBoneFunc()
    {
        static fnCalcBoneWorldMatrix fn = nullptr;
        if (!fn) {
            HMODULE h = GetModuleHandleA("i3SceneDx.dll");
            if (h) fn = (fnCalcBoneWorldMatrix)((BYTE*)h + OFF_SCENEOBJ_CALC_BONE);
        }
        return fn;
    }

    // ---- Get bone count from i3SceneObject ----
    static int GetBoneCount(void* sceneObj)
    {
        DWORD* boneList = nullptr;
        if (!SafeRead((BYTE*)sceneObj + OFF_SCENEOBJ_BONELIST, boneList))
            return 0;
        if (!boneList) return 0;

        DWORD start = 0, end = 0;
        if (!SafeRead(boneList, start)) return 0;
        if (!SafeRead(boneList + 1, end)) return 0;
        if (end < start) return 0;

        return (int)((end - start) >> 2);
    }

    // ---- Get bone position ----
    static bool GetBonePos(void* sceneObj, int boneIdx, Vec3& out)
    {
        fnCalcBoneWorldMatrix fn = GetCalcBoneFunc();
        if (!fn) return false;
        if (!IsReadable(sceneObj, 0x100)) return false;

        MATRIX m = {};
        fn(sceneObj, boneIdx, &m, 0);

        // Sanity check — reject NaN / insane values
        if (m[12] != m[12] || m[13] != m[13] || m[14] != m[14])
            return false;
        if (fast_abs(m[12]) > 100000.f || fast_abs(m[13]) > 100000.f || fast_abs(m[14]) > 100000.f)
            return false;

        out = Vec3(m[12], m[13], m[14]);
        return true;
    }

    // ---- Recursive scene graph traversal ----
    static int g_visitedNodes = 0;

    static void TraverseNode(void* node, int depth)
    {
        if (!node || depth > PB::MAX_TRAVERSE_DEPTH) return;
        if (++g_visitedNodes > PB::MAX_TRAVERSE_NODES) return;

        // Check if this node is an i3SceneObject with bones
        int boneCount = GetBoneCount(node);
        if (boneCount >= (int)g_cfg.minBoneCount && boneCount <= (int)g_cfg.maxBoneCount) {
            // Get feet and head positions
            Vec3 feet, head;
            bool hasFeet = GetBonePos(node, PB::BONE_ROOT, feet);
            bool hasHead = GetBonePos(node, (int)g_cfg.headBoneIndex, head);

            if (hasFeet && hasHead) {
                // Distance from local/camera position. If no camera reference
                // is available yet, keep the entity (filter skipped).
                float dist = Distance3D(feet, g_localPos);
                bool haveCam = (g_localPos.x != 0.f || g_localPos.y != 0.f ||
                                g_localPos.z != 0.f);
                if (!haveCam || dist <= g_cfg.maxDistance) {
                    Entity e;
                    e.posFeet = feet;
                    e.posHead = head;
                    e.distance = dist;
                    e.boneCount = boneCount;
                    e.visible = true;  // TODO: implement visibility trace
                    e.sceneObj = node;
                    g_entities.push_back(e);
                }
            }
        }

        // Recurse into children
        DWORD childStart = 0, childEnd = 0;
        if (!SafeRead((BYTE*)node + OFF_NODE_CHILD_START, childStart)) return;
        if (!SafeRead((BYTE*)node + OFF_NODE_CHILD_END, childEnd)) return;
        if (childEnd < childStart) return;

        int childCount = (int)((childEnd - childStart) >> 2);
        if (childCount <= 0 || childCount > 256) return;

        for (int i = 0; i < childCount; i++) {
            void* child = nullptr;
            if (!SafeRead((DWORD*)(childStart + i * 4), child)) continue;
            if (child) TraverseNode(child, depth + 1);
        }
    }

    void CaptureFrame(void* sceneCtx)
    {
        if (!sceneCtx) return;

        std::lock_guard<std::mutex> lock(g_mutex);
        g_entities.clear();
        g_visitedNodes = 0;

        // Get the 3D root node
        void* root = nullptr;
        if (!SafeRead((BYTE*)sceneCtx + OFF_SGCTX_3D_ROOT, root)) return;
        if (!root) return;

        // Local reference = camera world position (derived from the view
        // matrix in the EndScene hook). If the camera is shader-driven this
        // stays origin, and the distance filter below is skipped so entities
        // still populate.
        g_localPos = Hooks::GetLocalPos();

        // Traverse
        TraverseNode(root, 0);
    }

    const std::vector<Entity>& GetEntities()
    {
        return g_entities;
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_entities.clear();
    }

} // namespace Entities
} // namespace PB