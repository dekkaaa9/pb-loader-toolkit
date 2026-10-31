// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#include "esp.h"
#include "config.h"
#include "math.h"
#include "entities.h"
#include "gui.h"

#include <cstdio>

// ============================================================
//  ESP Renderer — raw D3D9, no D3DX dependency
//  Uses DrawPrimitiveUP with pre-transformed screen-space vertices
// ============================================================

namespace PB {
namespace ESP {

    // ---- Screen-space line vertex (already transformed) ----
    struct LineVertex {
        float    x, y, z, rhw;
        D3DCOLOR color;
    };
    static const DWORD LINE_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

    // ---- Draw a single line ----
    static void DrawLine(IDirect3DDevice9* dev, float x1, float y1,
                         float x2, float y2, D3DCOLOR color)
    {
        LineVertex verts[2] = {
            { x1, y1, 0.0f, 1.0f, color },
            { x2, y2, 0.0f, 1.0f, color }
        };
        dev->SetFVF(LINE_FVF);
        dev->DrawPrimitiveUP(D3DPT_LINELIST, 1, verts, sizeof(LineVertex));
    }

    // ---- Draw a rectangle outline (4 lines in one batch) ----
    static void DrawRect(IDirect3DDevice9* dev, float x, float y,
                         float w, float h, D3DCOLOR color)
    {
        LineVertex verts[8] = {
            // Top
            { x,     y,     0.0f, 1.0f, color },
            { x + w, y,     0.0f, 1.0f, color },
            // Right
            { x + w, y,     0.0f, 1.0f, color },
            { x + w, y + h, 0.0f, 1.0f, color },
            // Bottom
            { x + w, y + h, 0.0f, 1.0f, color },
            { x,     y + h, 0.0f, 1.0f, color },
            // Left
            { x,     y + h, 0.0f, 1.0f, color },
            { x,     y,     0.0f, 1.0f, color },
        };
        dev->SetFVF(LINE_FVF);
        dev->DrawPrimitiveUP(D3DPT_LINELIST, 4, verts, sizeof(LineVertex));
    }

    // ---- Main render entry (called from EndScene hook) ----
    void Render(IDirect3DDevice9* dev, const MATRIX& vp, int scrW, int scrH)
    {
        if (!dev) return;

        const auto& entities = Entities::GetEntities();
        if (entities.empty()) return;

        // ---- Save current render + texture-stage states ----
        DWORD oldLighting, oldZEnable, oldAlphaBlend, oldSrcBlend, oldDestBlend, oldFVF;
        DWORD oldColorOp, oldColorArg1, oldColorArg2;
        DWORD oldAlphaOp, oldAlphaArg1, oldAlphaArg2;
        dev->GetRenderState(D3DRS_LIGHTING,       &oldLighting);
        dev->GetRenderState(D3DRS_ZENABLE,        &oldZEnable);
        dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
        dev->GetRenderState(D3DRS_SRCBLEND,       &oldSrcBlend);
        dev->GetRenderState(D3DRS_DESTBLEND,      &oldDestBlend);
        dev->GetFVF(&oldFVF);
        dev->GetTextureStageState(0, D3DTSS_COLOROP,   &oldColorOp);
        dev->GetTextureStageState(0, D3DTSS_COLORARG1, &oldColorArg1);
        dev->GetTextureStageState(0, D3DTSS_COLORARG2, &oldColorArg2);
        dev->GetTextureStageState(0, D3DTSS_ALPHAOP,   &oldAlphaOp);
        dev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &oldAlphaArg1);
        dev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &oldAlphaArg2);

        // ---- Set states for 2D overlay ----
        dev->SetRenderState(D3DRS_LIGHTING,         FALSE);
        dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);
        dev->SetFVF(LINE_FVF);

        // Texture-stage states so GUI::Text blends with alpha correctly
        dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

        float cx = scrW * 0.5f;
        float cy = (float)scrH;

        for (const auto& e : entities) {
            Vec2 screenFeet, screenHead;
            if (!WorldToScreen(e.posFeet, screenFeet, vp, scrW, scrH)) continue;
            if (!WorldToScreen(e.posHead, screenHead, vp, scrW, scrH)) continue;

            float boxH = screenFeet.y - screenHead.y;
            if (boxH < 8.0f) continue;
            float boxW = boxH * 0.35f;
            float boxX = screenHead.x - boxW * 0.5f;
            float boxY = screenHead.y;

            D3DCOLOR col = e.visible ? (D3DCOLOR)g_cfg.colEnemyVisible
                                     : (D3DCOLOR)g_cfg.colEnemyHidden;

            // Box
            if (g_cfg.boxESP)
                DrawRect(dev, boxX, boxY, boxW, boxH, col);

            // Snapline from bottom-center of screen
            if (g_cfg.snaplineESP)
                DrawLine(dev, cx, cy, screenFeet.x, screenFeet.y,
                         (D3DCOLOR)g_cfg.colSnapline);

            // Head marker
            if (g_cfg.boneESP) {
                DrawLine(dev, screenHead.x - 3, screenHead.y,
                              screenHead.x + 3, screenHead.y, col);
                DrawLine(dev, screenHead.x, screenHead.y - 3,
                              screenHead.x, screenHead.y + 3, col);
            }

            // Distance text
            if (g_cfg.distanceText) {
                char buf[24];
                snprintf(buf, sizeof(buf), "%.0fm", e.distance);
                GUI::Text(buf, screenFeet.x - 10.f, screenFeet.y + 4.f,
                          (D3DCOLOR)g_cfg.colText, true);
            }
        }

        // ---- Restore render + texture-stage states ----
        dev->SetRenderState(D3DRS_LIGHTING,         oldLighting);
        dev->SetRenderState(D3DRS_ZENABLE,          oldZEnable);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
        dev->SetRenderState(D3DRS_SRCBLEND,         oldSrcBlend);
        dev->SetRenderState(D3DRS_DESTBLEND,        oldDestBlend);
        dev->SetFVF(oldFVF);
        dev->SetTextureStageState(0, D3DTSS_COLOROP,   oldColorOp);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, oldColorArg1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG2, oldColorArg2);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   oldAlphaOp);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, oldAlphaArg1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, oldAlphaArg2);
    }

} // namespace ESP
} // namespace PB