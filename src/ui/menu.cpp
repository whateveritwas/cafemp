#include <gx2/registers.h>
#include <gx2/swap.h>
#include <imgui.h>
#include <imgui_impl_wiiu.h>
#include <imgui_impl_gx2.h>
#include <whb/gfx.h>

#include "input/input_actions.hpp"
#include "utils/app_state.hpp"
#include "ui/scene.hpp"
#include "ui/scenes/scene_main_menu.hpp"
#include "utils/font.hpp"

#include "ui/menu.hpp"

static ImGuiIO *io {};
const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.00f, 1.00f);
static InputState input {};

void ui_init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    io->LogFilename = nullptr; // don't save log
    io->IniFilename = nullptr; // don't save ini

    io->ConfigDragScroll = true;
    io->ConfigWindowsMoveFromTitleBarOnly = true;
    io->MouseDragThreshold = 25;
    io->ConfigInputTrickleEventQueue = false;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(2.0f);

    font_load_all();

    ImGui_ImplWiiU_Init();
    ImGui_ImplGX2_Init();

    ui_scene_register(STATE_MENU, {[]() {}, [](InputState &input) {}, []() { scene_main_menu_render(); }, []() {}});

    ui_scene_set(app_state_get());
}

void ui_render() {
    input_poll(input);
    
    GX2ColorBuffer* cb = WHBGfxGetTVColourBuffer();

    ImGui_ImplWiiU_NewFrame(cb);
    ImGui_ImplGX2_NewFrame();
    ImGui::NewFrame();

    {
	ui_scene_input(input);
	ui_scene_render();
    }

    ImGui::EndFrame();
    

    ImGui::Render();
    WHBGfxBeginRender();

    WHBGfxBeginRenderTV();
    GX2SetViewport(0, 0, io->DisplaySize.x, io->DisplaySize.y, 0.0f, 1.0f);
    WHBGfxClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

    ImGui_ImplGX2_RenderDrawData(ImGui::GetDrawData());

    ImGui_ImplWiiU_DrawKeyboardOverlay(ImGui_KeyboardOverlay_Auto);

    WHBGfxFinishRenderTV();

    GX2CopyColorBufferToScanBuffer(WHBGfxGetTVColourBuffer(), GX2_SCAN_TARGET_DRC);

    WHBGfxFinishRender();
}

void ui_shutdown() {
    ImGui_ImplGX2_Shutdown();
    ImGui_ImplWiiU_Shutdown();
    
    ImGui::DestroyContext();
}
