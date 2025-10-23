#include "widget_player_hud.hpp"

#include <string>

#include "main.hpp"
#include "utils/app_state.hpp"
#include "utils/media_info.hpp"
#include "utils/utils.hpp"
#include "vendor/ui/nuklear.h"


void widget_player_hud_render(struct nk_context *ctx, media_info* info) {
    const int hud_height = 80 * UI_SCALE;
    struct nk_rect hud_rect = nk_rect(0, SCREEN_HEIGHT - hud_height, SCREEN_WIDTH, hud_height);

    if (nk_begin(ctx, "HUD", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, (hud_height / 2) - 5, 1);

        double progress_seconds = 0.0;
        double total_seconds = 0.0;

        switch(info->type) {
            case 'V': // Video
                progress_seconds = std::min(info->current_video_playback_time, info->total_video_playback_time);
                total_seconds = info->total_video_playback_time;
                break;
            case 'A': // Audio
                progress_seconds = std::min(info->current_audio_playback_time, info->total_audio_playback_time);
                total_seconds = info->total_audio_playback_time;
                break;
            default:
                progress_seconds = 0.0;
                total_seconds = 1.0;
                break;
        }

        nk_size progress = static_cast<nk_size>(progress_seconds);
        nk_size total = static_cast<nk_size>(total_seconds > 0 ? total_seconds : 1);

        nk_progress(ctx, &progress, total, NK_FIXED);

        nk_layout_row_begin(ctx, NK_DYNAMIC, hud_height / 2, 2);
        nk_layout_row_push(ctx, 0.8f); // 80% left for playback info text
        {
            std::string hud_str = (info->playback_status ? "> " : "|| ");
            hud_str += format_time(progress_seconds);
            hud_str += " / ";
            hud_str += format_time(total_seconds);
            hud_str += " [";
            hud_str += info->filename;
            hud_str += "]";

            nk_label(ctx, hud_str.c_str(), NK_TEXT_LEFT);
        }

        if(app_state_get() == STATE_PLAYING_VIDEO) {
            nk_layout_row_push(ctx, 0.2f); // 20% right for audio/caption track info
            {
                std::string hud_str = "A:";
                hud_str += std::to_string(info->current_audio_track_id);
                hud_str += "/";
                hud_str += std::to_string(info->total_audio_track_count);
                hud_str += " S:";
                hud_str += std::to_string(info->current_caption_id);
                hud_str += "/";
                hud_str += std::to_string(info->total_caption_count);
                nk_label(ctx, hud_str.c_str(), NK_TEXT_RIGHT);
            }
        }
        nk_end(ctx);
    }
}
