// dear imgui: Platform Backend for the Wii U
// Copyright (C) 2023 GaryOderNichts
// Copyright (C) 2026 Daniel K. O. (dkosmari)

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

#include <gx2/enum.h>
#include <gx2/surface.h>        // Color buffer for drawing swkbd.
#include <vpad/input.h>         // GamePad Input
#include <padscore/kpad.h>      // Wiimote Input

struct ImGui_ImplWiiU_ControllerInput
{
    VPADStatus* vpad = nullptr;
    KPADStatus* kpad[4] = { nullptr };
};

enum ImGui_ImplWiiU_KeyboardOverlayType
{
    //! Draw for the DRC
    ImGui_KeyboardOverlay_DRC,
    //! Draw for the TV
    ImGui_KeyboardOverlay_TV,
    //! Draw for the controller which requested the keyboard
    ImGui_KeyboardOverlay_Auto
};

IMGUI_IMPL_API bool     ImGui_ImplWiiU_Init();
IMGUI_IMPL_API void     ImGui_ImplWiiU_Shutdown();
IMGUI_IMPL_API bool     ImGui_ImplWiiU_ProcessInput(ImGui_ImplWiiU_ControllerInput* input);

// TODO: add options/functions to control SWKBD region and language.
IMGUI_IMPL_API void     ImGui_ImplWiiU_DrawKeyboardOverlay(ImGui_ImplWiiU_KeyboardOverlayType type = ImGui_KeyboardOverlay_Auto);
IMGUI_IMPL_API void     ImGui_ImplWiiU_NewFrame(GX2ColorBuffer* cb, GX2RenderTarget target = GX2_RENDER_TARGET_0);
