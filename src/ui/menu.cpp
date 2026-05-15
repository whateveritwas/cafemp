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
#include "ui/scenes/scene_photo_viewer.hpp"
#include "utils/font.hpp"
#include "utils/media_info.hpp"
#include "utils/media_files.hpp"

#include "ui/menu.hpp"

static ImGuiIO *io{};
const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.00f, 1.00f);
static InputState input{};

void start_file(int index) {
    const std::string filename = get_media_files()[index];
    std::string full_path;

    auto new_info = std::make_unique<media_info>();
    media_info_set(std::move(new_info));

    media_info* info = media_info_get();
    info->type = '\0';
    info->path = "";
    info->filename = filename;
    info->current_video_playback_time = 0;
    info->current_audio_playback_time = 0;
    info->current_audio_track_id = 1;
    info->total_audio_track_count = 1;
    info->current_caption_id = 1;
    info->total_caption_count = 1;

    struct MediaMapping {
        const char* folder;
        char type;
    };

    static const std::unordered_map<int, MediaMapping> state_map = {
        {STATE_MENU_AUDIO_FILES, {"Audio/", 'A'}},
        {STATE_MENU_VIDEO_FILES, {"Video/", 'V'}},
        {STATE_MENU_IMAGE_FILES, {"Photo/", 'P'}},
        {STATE_MENU_PDF_FILES, {"Library/", 'L'}}
    };

    int state = app_state_get();
    auto it = state_map.find(state);
    if (it == state_map.end()) {
        log_message(LOG_ERROR, "Menu", "Unsupported file type");
        return;
    }

    const MediaMapping& mapping = it->second;
    full_path = std::string(BASE_PATH) + mapping.folder + filename;

    info->type = mapping.type;
    info->path = full_path;

    if (mapping.type == 'P' || mapping.type == 'L') {
        info->current_audio_track_id = 0;
        info->total_audio_track_count = 0;
        info->current_caption_id = index;
        info->total_caption_count = get_media_files().size();
    }

    switch (mapping.type) {
    case 'A': app_state_set(STATE_PLAYING_AUDIO); break;
    case 'V': app_state_set(STATE_PLAYING_VIDEO); break;
    case 'P': app_state_set(STATE_VIEWING_PHOTO); break;
    case 'L': app_state_set(STATE_VIEWING_PDF); break;
    }
}

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
    ui_scene_register(STATE_MENU_VIDEO_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});
    ui_scene_register(STATE_MENU_AUDIO_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});
    ui_scene_register(STATE_MENU_IMAGE_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});
    ui_scene_register(STATE_MENU_PDF_FILES, {[]() {}, [](InputState &input) {}, []() { scene_file_browser_render(); }, []() {}});

    ui_scene_register(STATE_VIEWING_PHOTO, {[](){ scene_photo_viewer_init(media_info_get()->path); }, [](InputState& input){ scene_photo_viewer_input(input); }, [](){ scene_photo_viewer_render(); }, [](){}});
    
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
