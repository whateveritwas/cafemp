#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <vpad/input.h>
#include <coreinit/debug.h>
#include <gx2/context.h>
#include <string>

#include "config.hpp"
#include "menu.hpp"
#include "video_player.hpp"

AppState app_state = STATE_MENU;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;
SDL_AudioSpec wanted_spec;

TTF_Font* font;

std::vector<std::string> video_files;

int selected_index = 0;
bool playing_video = true;

int init_sdl() {
    printf("Starting SDL...\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0) {
        printf("Failed to init SDL: %s\n", SDL_GetError());
        return -1;
    }

    window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    wanted_spec = create_audio_spec();

    if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        printf("SDL_OpenAudio error: %s\n", SDL_GetError());
        return -1;
    }

    avformat_network_init();

    TTF_Init();
    font = TTF_OpenFont(FONT_PATH, 24);

    return 0;
}

void handle_vpad_input() {
    VPADStatus buf;
    int key_press = VPADRead(VPAD_CHAN_0, &buf, 1, nullptr);

    if (key_press == 1) {
        if(app_state == STATE_PLAYING) {
            if (buf.trigger == DRC_BUTTON_A) {
                playing_video = !playing_video;
                SDL_PauseAudio(playing_video ? 0 : 1);
            }

            else if(buf.trigger == DRC_BUTTON_SELECT) {
                app_state = STATE_MENU;
                video_player_cleanup();
                scan_directory(VIDEO_PATH, video_files);
            } else if(buf.trigger == DRC_BUTTON_DPAD_LEFT) {
                video_player_scrub(-5);
                // int64_t seek_target = (current_pts_seconds - 4) * AV_TIME_BASE;
                // av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD);
            } else if(buf.trigger == DRC_BUTTON_DPAD_RIGHT) {
                video_player_scrub(5);
                // int64_t seek_target = (current_pts_seconds + 4) * AV_TIME_BASE;
                // av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_ANY);
            }
        }

        if (app_state == STATE_MENU) {
            if (buf.trigger == DRC_BUTTON_DPAD_UP && selected_index > 0) {
                selected_index--;
            } else if (buf.trigger == DRC_BUTTON_DPAD_DOWN && selected_index < (int)video_files.size() - 1) {
                selected_index++;
            } else if (buf.trigger == DRC_BUTTON_A && !video_files.empty()) {
                std::string full_path = std::string(VIDEO_PATH) + video_files[selected_index];
                video_player_start(full_path.c_str(), &app_state, *renderer, texture, *SDL_CreateMutex(), wanted_spec);
                playing_video = true;
            }
        }        
    }
}

int main(int argc, char **argv) {
    WHBProcInit();

    if (init_sdl() != 0) return -1;

    scan_directory(VIDEO_PATH, video_files);

    while (WHBProcIsRunning()) {
        handle_vpad_input();
        switch(app_state) {
            case STATE_PLAYING:
            if (!playing_video) {
                SDL_RenderPresent(renderer);
                SDL_Delay(50);
                break;
            }
            //render_video_hud(renderer, font, texture, video_player_get_current_time(), 0);
            video_player_update(&app_state, renderer, texture);
            break;

            case STATE_MENU:
            render_file_browser(renderer, font, selected_index, video_files);
            SDL_Delay(50);
            break;
        }
    }

    video_player_cleanup();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_CloseAudio();
    SDL_Quit();

    WHBProcShutdown();
    return 0;
}
