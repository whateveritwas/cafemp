#include <gx2/registers.h>
#include <gx2/swap.h>
#include <memory>
#include "vendor/ui/imgui.h"
#include "vendor/ui/backends/imgui_impl_gx2.h"
#include "vendor/ui/backends/imgui_impl_wiiu.h"
#include <whb/gfx.h>

#include "main.hpp"
#include "input/input_actions.hpp"
#include "utils/app_state.hpp"
#include "ui/scene.hpp"
#include "ui/scenes/scene_file_browser.hpp"
#include "ui/scenes/scene_main_menu.hpp"
#include "ui/scenes/scene_media_player.hpp"
#include "ui/scenes/scene_pdf_viewer.hpp"
#include "ui/scenes/scene_photo_viewer.hpp"
#include "ui/widgets/widget_cursor.hpp"
#include "utils/font.hpp"
#include "utils/media_info.hpp"
#include "player/media_player.hpp"

#include "ui/menu.hpp"

static ImGuiIO *io{};
const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.00f, 1.00f);
static InputState input{};

static bool ambiance_playing = false;
static bool background_music_enabled = true;

void ui_handle_ambiance(bool new_state) {
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

    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(2.0f);

    font_load_all();

    ImGui_ImplWiiU_Init();
    ImGui_ImplGX2_Init();

    ui_scene_register(STATE_MENU, {[]() {}, [](InputState &input) {}, []() { scene_main_menu_render(); }, []() {}});
    ui_scene_register(STATE_MENU_VIDEO_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});
    ui_scene_register(STATE_MENU_AUDIO_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});
    ui_scene_register(STATE_MENU_IMAGE_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});
    ui_scene_register(STATE_MENU_PDF_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});

    ui_scene_register(STATE_VIEWING_PHOTO, {[]() {
                                                scene_photo_viewer_init(media_info_get()->path);
                                                ui_handle_ambiance(false);
                                            },
                                            [](InputState &input) { scene_photo_viewer_input(input); }, []() { scene_photo_viewer_render(); },
                                            []() {
						ui_handle_ambiance(true);
					    }});

    ui_scene_register(STATE_VIEWING_PDF, {[]() {
                                              scene_pdf_viewer_init(media_info_get()->path);
                                              ui_handle_ambiance(false);
                                          },
                                          [](InputState &input) { scene_pdf_viewer_input(input); }, []() { scene_pdf_viewer_render(); },
                                          []() {
					      ui_handle_ambiance(true);
					  }});

    ui_scene_register(STATE_PLAYING_VIDEO, {[]() {
                                                scene_media_player_init(media_info_get()->path);
                                                ui_handle_ambiance(false);
                                            },
                                            [](InputState &input) { scene_media_player_input(input); }, []() { scene_media_player_render(); },
                                            []() {
                                                scene_media_player_shutdown();
						ui_handle_ambiance(true);
					    }});

    ui_scene_register(STATE_PLAYING_AUDIO, {[]() {
                                                scene_media_player_init(media_info_get()->path);
                                                ui_handle_ambiance(false);
                                            },
                                            [](InputState &input) { scene_media_player_input(input); }, []() { scene_media_player_render(); },
                                            []() {
                                                scene_media_player_shutdown();
                                                ui_handle_ambiance(true);
                                            }});
    

    ui_scene_set(app_state_get());
    ui_handle_ambiance(true);
}

void ui_render() {
    input_poll(input);

    GX2ColorBuffer *cb = WHBGfxGetTVColourBuffer();

    ImGui_ImplWiiU_NewFrame(cb);
    ImGui_ImplGX2_NewFrame();
    ImGui::NewFrame();

    media_player_update();

    {
        ui_scene_render();
        ui_scene_input(input);
        widget_cursor_render(input);
    }

    ImGui::EndFrame();

    ImGui::Render();
    WHBGfxBeginRender();

    WHBGfxBeginRenderTV();
    GX2SetViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f);

    if (app_state_get() == STATE_PLAYING_VIDEO) scene_media_player_render();
    else WHBGfxClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

    ImGui_ImplGX2_RenderDrawData(ImGui::GetDrawData());

    ImGui_ImplWiiU_DrawKeyboardOverlay(ImGui_KeyboardOverlay_Auto);

    WHBGfxFinishRenderTV();

    GX2CopyColorBufferToScanBuffer(WHBGfxGetTVColourBuffer(), GX2_SCAN_TARGET_DRC);

    WHBGfxFinishRender();

    if ((int)media_player_get_current_time() == (int)media_player_get_total_time() && ambiance_playing && background_music_enabled) {
	log_message(LOG_DEBUG, "UI", "Restarting ambiance");
        media_player_seek(-1000);
    } 
}

void ui_shutdown() {
    ui_handle_ambiance(false);
    
    ImGui_ImplGX2_Shutdown();
    ImGui_ImplWiiU_Shutdown();

    ImGui::DestroyContext();
}
