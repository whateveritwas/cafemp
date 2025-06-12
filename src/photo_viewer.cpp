#include "photo_viewer.hpp"
#include <SDL2/SDL_image.h>
#include <iostream>

static SDL_Renderer* photo_renderer = nullptr;
static SDL_Texture* photo_texture = nullptr;

void photo_viewer_init(SDL_Renderer* renderer, SDL_Texture*& texture) {
    photo_renderer = renderer;
    photo_texture = texture;
}

void photo_viewer_open_picture(const char* filepath) {
    if (photo_texture) {
        SDL_DestroyTexture(photo_texture);
        photo_texture = nullptr;
    }

    SDL_Surface* surface = IMG_Load(filepath);  // <-- Correct function name
    if (!surface) {
        std::cerr << "Failed to load image: " << IMG_GetError() << std::endl;  // <-- Correct error function
        return;
    }

    photo_texture = SDL_CreateTextureFromSurface(photo_renderer, surface);
    SDL_FreeSurface(surface);

    if (!photo_texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
    }
}

void photo_viewer_render(SDL_Rect rect) {
    if (!photo_texture) return;

    SDL_RenderClear(photo_renderer);

    int w, h;
    SDL_QueryTexture(photo_texture, nullptr, nullptr, &w, &h);
    SDL_Rect dst = { 0, 0, w, h };

    SDL_RenderCopy(photo_renderer, photo_texture, nullptr, &rect);
}

void photo_viewer_cleanup() {
    if (photo_texture) {
        SDL_DestroyTexture(photo_texture);
        photo_texture = nullptr;
    }
}
