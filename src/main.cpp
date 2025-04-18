#include <whb/proc.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <string>
#include <thread>

#include "config.hpp"
#include "video_player.hpp"
#include "menu.hpp"

AppState app_state = STATE_MENU;
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;
SDL_AudioSpec wanted_spec;

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
    //font = TTF_OpenFont(FONT_PATH, 24);

    return 0;
}

int main(int argc, char **argv) {
    WHBProcInit();

    if (init_sdl() != 0) return -1;

    ui_init(window, renderer, texture, &app_state, wanted_spec);

    while (WHBProcIsRunning()) {
        ui_render();
        SDL_RenderPresent(renderer);
    }

    video_player_cleanup();
    ui_shutodwn();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_CloseAudio();
    SDL_Quit();

    WHBProcShutdown();
    return 0;
}
