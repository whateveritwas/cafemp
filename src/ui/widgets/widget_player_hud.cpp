#include "widget_player_hud.hpp"

#include <algorithm>
#include <string>

#include "main.hpp"
#include "utils/app_state.hpp"
#include "utils/display.hpp"
#include "utils/media_info.hpp"
#include "vendor/ui/imgui.h"

std::string format_time(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

void widget_player_hud_render(media_info *info) {
    const float hud_height = 80.0f * display_get().scale;

    ImGui::SetNextWindowPos(ImVec2(0.0f, display_get().height - hud_height));
    ImGui::SetNextWindowSize(ImVec2(display_get().width, hud_height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("HUD", nullptr, flags)) {
        double progress_seconds = 0.0;
        double total_seconds = 0.0;

        progress_seconds = info->current_playback_time;
        total_seconds = info->total_playback_time;

        if (total_seconds <= 0.0) total_seconds = 1.0;

        float progress = static_cast<float>(progress_seconds / total_seconds);

        ImGui::ProgressBar(progress, ImVec2(-1.0f, (hud_height / 2.0f) - 10.0f));

        std::string left_text = (info->playback_status ? "> " : "|| ");

        left_text += format_time(progress_seconds);
        left_text += " / ";
        left_text += format_time(total_seconds);
        left_text += " [";
        left_text += info->filename;
        left_text += "]";

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(left_text.c_str());

        if (app_state_get() == STATE_PLAYING_VIDEO) {
            std::string right_text = "A:";
            right_text += std::to_string(info->current_audio_track_id);
            right_text += "/";
            right_text += std::to_string(info->total_audio_track_count);
            right_text += " S:";
            right_text += std::to_string(info->current_caption_id);
            right_text += "/";
            right_text += std::to_string(info->total_caption_count);

            float text_width = ImGui::CalcTextSize(right_text.c_str()).x;
            float avail_width = ImGui::GetContentRegionAvail().x;

            ImGui::SameLine(ImGui::GetCursorPosX() + std::max(0.0f, avail_width - text_width));

            ImGui::TextUnformatted(right_text.c_str());
        }
    }

    ImGui::End();
}
