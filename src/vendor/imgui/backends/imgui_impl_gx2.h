// dear imgui: Renderer Backend for the Nintendo Wii U using GX2
// Copyright (c) 2023 GaryOderNichts

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

// Backend API
IMGUI_IMPL_API bool     ImGui_ImplGX2_Init();
IMGUI_IMPL_API void     ImGui_ImplGX2_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplGX2_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplGX2_RenderDrawData(ImDrawData* draw_data);

// (Optional) Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API void     ImGui_ImplGX2_HandleTexture(ImTextureData* tex);
IMGUI_IMPL_API bool     ImGui_ImplGX2_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplGX2_DestroyDeviceObjects();
