#include "ui/menu.hpp"

#include "input/input_actions.hpp"
#include "main.hpp"
#include "player/media_player.hpp"
#include "ui/scene.hpp"
#include "ui/scenes/scene_file_browser.hpp"
#include "ui/scenes/scene_main_menu.hpp"
#include "ui/scenes/scene_media_player.hpp"
#include "ui/scenes/scene_pdf_viewer.hpp"
#include "ui/scenes/scene_photo_viewer.hpp"
#include "ui/widgets/widget_cursor.hpp"
#include "utils/app_state.hpp"
#include "utils/display.hpp"
#include "utils/font.hpp"
#include "utils/media_info.hpp"

#include <gx2/registers.h>
#include <gx2/swap.h>
#include <imgui/backends/imgui_impl_gx2.h>
#include <imgui/backends/imgui_impl_wiiu.h>
#include <imgui/imgui.h>
#include <memory>
#include <whb/gfx.h>

static ImGuiIO *io{};

static bool ambiance_playing = false;
static bool background_music_enabled = true;

void ui_handle_ambiance(bool new_state) {
    /*
    ambiance_playing = new_state;

    if (ambiance_playing && background_music_enabled) {
        media_player_init(AMBIANCE_PATH);
        media_player_play(true);
        ambiance_playing = true;
    } else if (ambiance_playing && !background_music_enabled) {
        media_player_play(true);
        media_player_cleanup();
        media_player_play(false);
    }
    */
}

// From https://github.com/ocornut/imgui/issues/707#issuecomment-4763488457
inline static void ImGui_StyleNuklearDarkGray(ImGuiStyle *dst = nullptr) {
    ImGuiStyle *style = dst ? dst : &ImGui::GetStyle();
    ImVec4 *colors = style->Colors;

    style->WindowBorderSize = 1.0f;
    style->ChildBorderSize = 1.0f;
    style->PopupBorderSize = 1.0f;
    style->FrameBorderSize = 1.0f;

    style->WindowRounding = 2.0f;
    style->ChildRounding = 2.0f;
    style->FrameRounding = 2.0f;
    style->PopupRounding = 2.0f;
    style->GrabRounding = 2.0f;

    colors[ImGuiCol_Text] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    colors[ImGuiCol_CheckboxSelectedBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.50f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 0.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.22f, 0.22f, 0.78f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.29f, 0.29f, 0.29f, 0.78f);
    colors[ImGuiCol_Separator] = ImVec4(0.29f, 0.29f, 0.29f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    colors[ImGuiCol_InputTextCursor] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_Tab] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.29f, 0.29f, 0.29f, 0.78f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.39f, 0.39f, 0.39f, 0.78f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    // colors[ImGuiCol_DockingPreview] = ImVec4(0.69f, 0.69f, 0.69f, 0.78f);
    // colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.29f, 0.29f, 0.29f, 0.50f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink] = ImVec4(0.29f, 0.50f, 1.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_TreeLines] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_DragDropTargetBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_UnsavedMarker] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    colors[ImGuiCol_NavCursor] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void ui_init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    io->LogFilename = nullptr; // don't save log
    io->IniFilename = nullptr; // don't save ini

    io->ConfigDragScroll = true;
    io->ConfigWindowsMoveFromTitleBarOnly = true;
    io->MouseDragThreshold = 25;
    io->ConfigInputTrickleEventQueue = false;

    ImGui_StyleNuklearDarkGray();

    font_load_all();

    ImGui_ImplWiiU_Init();
    ImGui_ImplGX2_Init();

    ui_scene_register(STATE_MENU, {[]() {}, []() {}, []() { scene_main_menu_render(); }, []() {}});
    ui_scene_register(STATE_MENU_FILES, {[]() {}, []() { scene_file_browser_input(); }, []() { scene_file_browser_render(); }, []() {}});

    ui_scene_register(STATE_VIEWING_PHOTO, {[]() {
                                                scene_photo_viewer_init(media_info_get()->path);
                                                ui_handle_ambiance(false);
                                            },
                                            []() { scene_photo_viewer_input(); }, []() { scene_photo_viewer_render(); }, []() { ui_handle_ambiance(true); }});

    ui_scene_register(STATE_VIEWING_PDF, {[]() {
                                              scene_pdf_viewer_init(media_info_get()->path);
                                              ui_handle_ambiance(false);
                                          },
                                          []() { scene_pdf_viewer_input(); }, []() { scene_pdf_viewer_render(); }, []() { ui_handle_ambiance(true); }});

    ui_scene_register(STATE_PLAYING_VIDEO, {[]() {
                                                scene_media_player_init(media_info_get()->path);
                                                ui_handle_ambiance(false);
                                            },
                                            []() { scene_media_player_input(); }, []() { scene_media_player_render(); },
                                            []() {
                                                scene_media_player_shutdown();
                                                ui_handle_ambiance(true);
                                            }});

    ui_scene_register(STATE_PLAYING_AUDIO, {[]() {
                                                scene_media_player_init(media_info_get()->path);
                                                ui_handle_ambiance(false);
                                            },
                                            []() { scene_media_player_input(); }, []() { scene_media_player_render(); },
                                            []() {
                                                scene_media_player_shutdown();
                                                ui_handle_ambiance(true);
                                            }});

    input_init();

    ui_scene_set(app_state_get());
    ui_handle_ambiance(true);
}

void ui_render() {
    input_poll();

    GX2ColorBuffer *cb = WHBGfxGetTVColourBuffer();

    ImGui_ImplWiiU_NewFrame(cb);
    ImGui_ImplGX2_NewFrame();
    ImGui::NewFrame();

    {
        ui_scene_render();
        ui_scene_input();
        widget_cursor_render();
    }

    ImGui::EndFrame();

    ImGui::Render();
    WHBGfxBeginRender();

    WHBGfxBeginRenderTV();
    // GX2SetViewport(0, 0, display_get().width, display_get().height, 0.0f, 1.0f);

    if (app_state_get() == STATE_PLAYING_VIDEO)
        scene_media_player_render();
    else
        WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    ImGui_ImplGX2_RenderDrawData(ImGui::GetDrawData());
    ImGui_ImplWiiU_DrawKeyboardOverlay(ImGui_KeyboardOverlay_Auto);

    WHBGfxFinishRenderTV();
    GX2CopyColorBufferToScanBuffer(WHBGfxGetTVColourBuffer(), GX2_SCAN_TARGET_DRC);
    WHBGfxFinishRender();

    /*
        if ((int)media_player_get_current_time() == (int)media_player_get_total_time() && ambiance_playing && background_music_enabled) {
            log_message(LOG_DEBUG, "UI", "Restarting ambiance");
            media_player_seek(-1000);
        }
    */
}

void ui_shutdown() {
    input_shutdown();

    ui_handle_ambiance(false);

    ImGui_ImplGX2_Shutdown();
    ImGui_ImplWiiU_Shutdown();

    ImGui::DestroyContext();
}
