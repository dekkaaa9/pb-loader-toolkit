// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#include "menu.h"
#include "gui.h"
#include "config.h"
#include "hooks.h"
#include "entities.h"

#include <windows.h>
#include <cstdio>

// ============================================================
//  Mod Menu — keyboard-driven (arrow keys + ENTER), no mouse.
//  INSERT opens/closes.  UP/DOWN move the selection.
//  ENTER toggles a checkbox / fires a button.
//  LEFT/RIGHT nudge the selected slider.  END = panic.
// ============================================================

namespace PB {
namespace Menu {

    static bool  s_visible = false;
    static float s_x = 80.f;
    static float s_y = 80.f;
    static int   s_sel = 0;   // index into s_items

    // ---- Layout constants ----
    static const float WIN_W   = 320.f;
    static const float TITLE_H = 28.f;
    static const float PAD     = 14.f;
    static const float SEC_H   = 24.f;    // section header advance
    static const float ROW_CB  = 22.f;    // checkbox row height
    static const float ROW_SL  = 40.f;    // slider row height
    static const float ROW_ST  = 20.f;    // status/info row height
    static const float BTN_H   = 26.f;
    static const float GAP     = 10.f;
    static const float BOT_PAD = 8.f;

    // Section heights (shared by the measure pass and the draw pass)
    static const float H_ESP   = SEC_H + 4 * ROW_CB + GAP;
    static const float H_FILT  = SEC_H + 3 * ROW_SL + GAP;
    static const float H_BTN   = BTN_H + GAP;
    static const float H_DEBUG = SEC_H + 7 * ROW_ST + 12;
    static const float H_FOOT  = ROW_ST + BOT_PAD;

    // ---- Colors ----
    static const D3DCOLOR COL_BG      = 0xF00A0A12;
    static const D3DCOLOR COL_BORDER  = 0xFF2A2A38;
    static const D3DCOLOR COL_ACCENT  = 0xFF00AAFF;
    static const D3DCOLOR COL_TEXT    = 0xFFE8E8EC;
    static const D3DCOLOR COL_DIM     = 0xFF8888A0;
    static const D3DCOLOR COL_GREEN   = 0xFF00CC44;
    static const D3DCOLOR COL_RED     = 0xFFFF3333;
    static const D3DCOLOR COL_ORANGE  = 0xFFFFAA33;
    static const D3DCOLOR COL_SELBG   = 0x3300AAFF;

    // ---- Interactive items ----
    enum Kind { K_TOGGLE, K_SLIDER, K_BUTTON };

    static void ActionUnload();

    struct Item {
        Kind        kind;
        const char* label;
        bool*       bval;      // toggle target
        float*      fval;      // slider target
        float       fmin, fmax;
        void      (*action)(); // button callback
    };

    static Item s_items[] = {
        { K_TOGGLE, "Box ESP",       &g_cfg.boxESP,       nullptr, 0.f,  0.f,  nullptr },
        { K_TOGGLE, "Snaplines",     &g_cfg.snaplineESP,  nullptr, 0.f,  0.f,  nullptr },
        { K_TOGGLE, "Head Marker",   &g_cfg.boneESP,      nullptr, 0.f,  0.f,  nullptr },
        { K_TOGGLE, "Distance Text", &g_cfg.distanceText, nullptr, 0.f,  0.f,  nullptr },
        { K_SLIDER, "Max Distance",  nullptr, &g_cfg.maxDistance,  10.f, 500.f, nullptr },
        { K_SLIDER, "Min Bones",     nullptr, &g_cfg.minBoneCount,  1.f,  50.f, nullptr },
        { K_SLIDER, "Max Bones",     nullptr, &g_cfg.maxBoneCount, 10.f, 200.f, nullptr },
        { K_BUTTON, "Unload",        nullptr, nullptr, 0.f, 0.f, &ActionUnload },
    };
    static const int ITEM_COUNT = (int)(sizeof(s_items) / sizeof(s_items[0]));

    void Toggle()    { s_visible = !s_visible; }
    bool IsVisible() { return s_visible; }

    void ActionUnload()
    {
        g_cfg.espEnabled = false;
        Hooks::Shutdown();
        s_visible = false;
    }

    // ---- Keyboard input (edge-triggered), called once per frame ----
    void Update()
    {
        static bool pUp = false, pDown = false, pLeft = false,
                    pRight = false, pEnter = false;

        bool up    = (GetAsyncKeyState(VK_UP)     & 0x8000) != 0;
        bool down  = (GetAsyncKeyState(VK_DOWN)   & 0x8000) != 0;
        bool left  = (GetAsyncKeyState(VK_LEFT)   & 0x8000) != 0;
        bool right = (GetAsyncKeyState(VK_RIGHT)  & 0x8000) != 0;
        bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;

        if (s_visible) {
            if (up   && !pUp)   { if (--s_sel < 0)            s_sel = ITEM_COUNT - 1; }
            if (down && !pDown) { if (++s_sel >= ITEM_COUNT)  s_sel = 0; }

            Item& it = s_items[s_sel];

            if (enter && !pEnter) {
                if (it.kind == K_TOGGLE && it.bval)         *it.bval = !(*it.bval);
                else if (it.kind == K_BUTTON && it.action)  it.action();
            }

            if (it.kind == K_SLIDER && it.fval) {
                float step = (it.fmax - it.fmin) * 0.05f;
                if (left  && !pLeft)  { *it.fval -= step; if (*it.fval < it.fmin) *it.fval = it.fmin; }
                if (right && !pRight) { *it.fval += step; if (*it.fval > it.fmax) *it.fval = it.fmax; }
            }
        }

        pUp = up; pDown = down; pLeft = left; pRight = right; pEnter = enter;
    }

    // ---- Draw one interactive row ----
    static void DrawItem(int idx, float x, float y, float rowH)
    {
        Item& it      = s_items[idx];
        bool  sel     = (idx == s_sel);
        float contentW = WIN_W - 2 * PAD;

        if (sel)
            GUI::FilledRect(x - 5, y - 3, contentW + 10, rowH, COL_SELBG);

        if (it.kind == K_TOGGLE) {
            float bx = x + 8;
            GUI::FilledRect(bx, y, 13, 13, 0xFF0C0C14);
            GUI::OutlinedRect(bx, y, 13, 13, sel ? COL_ACCENT : 0xFF3A3A4A);
            if (it.bval && *it.bval)
                GUI::FilledRect(bx + 2, y + 2, 9, 9, COL_GREEN);
            GUI::Text(it.label, x + 28, y + 1, sel ? COL_TEXT : 0xFFCCCCDD, true);
        }
        else if (it.kind == K_SLIDER) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%s  %.0f", it.label, it.fval ? *it.fval : 0.f);
            GUI::Text(buf, x + 8, y, sel ? COL_TEXT : 0xFFCCCCDD, true);

            float trackY = y + 24;
            float trackW = contentW - 16;
            float norm = 0.f;
            if (it.fval && it.fmax > it.fmin)
                norm = (*it.fval - it.fmin) / (it.fmax - it.fmin);
            if (norm < 0.f) norm = 0.f;
            if (norm > 1.f) norm = 1.f;

            GUI::FilledRect(x + 8, trackY, trackW, 5, 0xFF16161F);
            if (norm * trackW > 0.f)
                GUI::FilledRect(x + 8, trackY, norm * trackW, 5,
                                sel ? COL_ACCENT : 0xFF0088CC);
            float hx = x + 8 + norm * (trackW - 6);
            GUI::FilledRect(hx, trackY - 3, 6, 11, sel ? COL_ACCENT : 0xFF0099DD);
        }
        else if (it.kind == K_BUTTON) {
            float bw = 110.f, bh = 22.f;
            float bx = x + 8;
            GUI::FilledRect(bx, y, bw, bh, sel ? 0xFF2A2A3A : 0xFF1A1A26);
            GUI::OutlinedRect(bx, y, bw, bh, sel ? COL_ACCENT : 0xFF3A3A4A);
            float tw = GUI::TextWidth(it.label);
            GUI::Text(it.label, bx + (bw - tw) * 0.5f, y + 4,
                      sel ? COL_TEXT : 0xFFCCCCDD, true);
        }
    }

    void Render(IDirect3DDevice9* dev)
    {
        if (!s_visible) return;

        GUI::BeginFrame(dev);

        float x = s_x, y = s_y;
        float h = TITLE_H + 12 + H_ESP + H_FILT + H_BTN + H_DEBUG + H_FOOT;

        // ---- Panel ----
        GUI::FilledRect(x, y, WIN_W, h, COL_BG);
        GUI::OutlinedRect(x, y, WIN_W, h, COL_BORDER);

        // ---- Title bar ----
        GUI::GradientRect(x, y, WIN_W, TITLE_H, 0xFF16161F, 0xFF0C0C14);
        GUI::FilledRect(x, y + TITLE_H - 1, WIN_W, 1, COL_BORDER);
        GUI::FilledRect(x, y, 3, TITLE_H, COL_ACCENT);
        GUI::Text("PBCheat v1.0", x + 12, y + 7, COL_TEXT, true);

        float cx = x + PAD;
        float cy = y + TITLE_H + 12;

        auto sectionHeader = [&](const char* label) {
            GUI::FilledRect(cx, cy + 3, 3, 13, COL_ACCENT);
            GUI::Text(label, cx + 9, cy + 2, COL_ACCENT, true);
            cy += SEC_H;
        };
        auto statusLine = [&](const char* label, bool ok) {
            D3DCOLOR col = ok ? COL_GREEN : COL_RED;
            GUI::FilledRect(cx + 4, cy + 6, 6, 6, col);
            char buf[80];
            snprintf(buf, sizeof(buf), "%s: %s", label, ok ? "OK" : "FAIL");
            GUI::Text(buf, cx + 17, cy + 2, col, true);
            cy += ROW_ST;
        };
        auto infoLine = [&](const char* text, D3DCOLOR col) {
            GUI::Text(text, cx + 17, cy + 2, col, true);
            cy += ROW_ST;
        };

        // ---- ESP toggles (items 0-3) ----
        sectionHeader("ESP");
        for (int i = 0; i < 4; i++) { DrawItem(i, cx, cy, ROW_CB); cy += ROW_CB; }
        cy += GAP;

        // ---- Filters (items 4-6) ----
        sectionHeader("FILTERS");
        for (int i = 4; i < 7; i++) { DrawItem(i, cx, cy, ROW_SL); cy += ROW_SL; }
        cy += GAP;

        // ---- Unload button (item 7) ----
        DrawItem(7, cx, cy, BTN_H);
        cy += BTN_H + GAP;

        // ---- Debug ----
        sectionHeader("DEBUG");
        statusLine("EndScene", Hooks::IsEndSceneHooked());
        statusLine("OnUpdate",  Hooks::IsOnUpdateHooked());
        {
            char buf[80];
            const auto& ents = Entities::GetEntities();
            snprintf(buf, sizeof(buf), "Entities: %d", (int)ents.size());
            infoLine(buf, ents.empty() ? COL_ORANGE : COL_DIM);
        }
        {
            MATRIX vp  = Hooks::GetVPMatrix();
            bool   vpOk = (vp[0] != 0.f || vp[5] != 0.f || vp[10] != 0.f);
            statusLine("VP Matrix", vpOk);
        }
        {
            char buf[80];
            snprintf(buf, sizeof(buf), "Screen: %dx%d",
                     Hooks::GetScreenWidth(), Hooks::GetScreenHeight());
            infoLine(buf, COL_DIM);
        }
        {
            char buf[80];
            snprintf(buf, sizeof(buf), "Device: 0x%p", Hooks::GetDevice());
            infoLine(buf, COL_DIM);
        }
        {
            char buf[80];
            snprintf(buf, sizeof(buf), "Text: ok=%d fail=%d",
                     GUI::GetTexOk(), GUI::GetTexFail());
            infoLine(buf, GUI::GetTexFail() > 0 ? COL_ORANGE : COL_DIM);
        }
        cy += 12;

        // ---- Footer hint ----
        GUI::Text("ARROWS navigate   ENTER select   END panic",
                  cx, cy, COL_DIM, true);

        GUI::EndFrame();
    }

} // namespace Menu
} // namespace PB