#include "photo_viewer.hpp"
#include <SDL2/SDL_image.h>
#include <iostream>

static SDL_Renderer* photo_renderer = nullptr;
static SDL_Texture* photo_texture = nullptr;
float x = 0.0f;
float y = 0.0f;

void photo_viewer_init(SDL_Renderer* renderer, SDL_Texture*& texture) {
    photo_renderer = renderer;
    photo_texture = texture;
}

void photo_viewer_open_picture(const char* filepath) {
    if (photo_texture) {
        SDL_DestroyTexture(photo_texture);
        photo_texture = nullptr;
    }

    SDL_Surface* surface = IMG_Load(filepath);
    if (!surface) {
        printf("[Photo Viewer] Failed to load image: %s\n", IMG_GetError());
        return;
    }

    photo_texture = SDL_CreateTextureFromSurface(photo_renderer, surface);
    SDL_FreeSurface(surface);

    if (!photo_texture) {
        printf("[Photo Viewer] Failed to create texture: %s\n", SDL_GetError());
    }

    x = 0.0f;
    y = 0.0f;
}

void photo_viewer_pan(float delta_x, float delta_y) {
    x += delta_x;
    y += delta_y;
}

void photo_viewer_render() {
    if (!photo_texture) return;

    SDL_RenderClear(photo_renderer);

    int w, h;
    SDL_QueryTexture(photo_texture, nullptr, nullptr, &w, &h);
    SDL_Rect dst = { (int)x, (int)y, w, h };

    SDL_RenderCopy(photo_renderer, photo_texture, nullptr, &dst);
}

void photo_viewer_cleanup() {
    if (photo_texture) {
        SDL_DestroyTexture(photo_texture);
        photo_texture = nullptr;
    }
}
