// ------------------------------------------------------------
// Cracked by dekkaaa9  |  https://github.com/dekkaaa9
// Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
// ------------------------------------------------------------
#pragma once
#ifndef PBC_CONFIG_H
#define PBC_CONFIG_H

namespace PB {

struct Config {
    // Master toggle
    bool espEnabled      = true;

    // Visual features
    bool boxESP          = true;
    bool snaplineESP     = true;
    bool boneESP         = false;   // skeleton overlay
    bool distanceText    = true;
    bool healthBar       = false;   // needs HP offset (future)

    // Filters
    float maxDistance    = 200.0f;
    float minBoneCount   = 12.0f;
    float maxBoneCount   = 80.0f;
    float headBoneIndex  = 7.0f;

    // Colors (D3DCOLOR ARGB)
    unsigned int colEnemyVisible = 0xFFFF0000;  // red
    unsigned int colEnemyHidden  = 0xFF00FF00;  // green
    unsigned int colTeam         = 0xFF0080FF;  // blue
    unsigned int colSnapline     = 0x80FFFFFF;  // translucent white
    unsigned int colText         = 0xFFFFFFFF;  // white
    unsigned int colTextShadow   = 0xFF000000;  // black

    // Hotkeys (VK codes)
    int toggleESPKey     = VK_INSERT;
    int toggleBoxKey     = VK_HOME;
    int toggleLineKey    = VK_DELETE;
    int panicKey         = VK_END;    // emergency disable + unload
};

extern Config g_cfg;

} // namespace PB

#endif