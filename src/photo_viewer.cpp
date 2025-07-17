#include "main.hpp"
#include "utils.hpp"
#include "photo_viewer.hpp"
#include <SDL2/SDL_image.h>
#include <iostream>

static SDL_Renderer* photo_renderer = nullptr;
static SDL_Texture* photo_texture = nullptr;
float scale = 1.0f;
const float min_scale = 0.5f;
const float max_scale = 5.0f;
SDL_Rect dst = { 0, 0, 0, 0 };
bool dst_set = false;

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
        return;
    }

    int tex_w, tex_h;
    SDL_QueryTexture(photo_texture, nullptr, nullptr, &tex_w, &tex_h);

    dst = calculate_aspect_fit_rect(tex_w, tex_h);

    scale = static_cast<float>(dst.w) / tex_w;

    dst_set = true;
}

void photo_texture_zoom(float delta_zoom) {
    scale += delta_zoom;

    if (scale > max_scale) scale = max_scale;
    if (scale < min_scale) scale = min_scale;

    // Horizontal clamp
    if (dst.w > SCREEN_WIDTH) {
        if (dst.x > 0) dst.x = 0;
        if (dst.x < SCREEN_WIDTH - dst.w) dst.x = SCREEN_WIDTH - dst.w;
    } else {
        if (dst.x < 0) dst.x = 0;
        if (dst.x > SCREEN_WIDTH - dst.w) dst.x = SCREEN_WIDTH - dst.w;
    }

    // Vertical clamp
    if (dst.h > SCREEN_HEIGHT) {
        if (dst.y > 0) dst.y = 0;
        if (dst.y < SCREEN_HEIGHT - dst.h) dst.y = SCREEN_HEIGHT - dst.h;
    } else {
        if (dst.y < 0) dst.y = 0;
        if (dst.y > SCREEN_HEIGHT - dst.h) dst.y = SCREEN_HEIGHT - dst.h;
    }

    dst_set = false;
}

void photo_viewer_pan(int delta_x, int delta_y) {
    dst.x += delta_x;
    dst.y += delta_y;

    // Horizontal clamp
    if (dst.w > SCREEN_WIDTH) {
        if (dst.x > 0) dst.x = 0;
        if (dst.x < SCREEN_WIDTH - dst.w) dst.x = SCREEN_WIDTH - dst.w;
    } else {
        if (dst.x < 0) dst.x = 0;
        if (dst.x > SCREEN_WIDTH - dst.w) dst.x = SCREEN_WIDTH - dst.w;
    }

    // Vertical clamp
    if (dst.h > SCREEN_HEIGHT) {
        if (dst.y > 0) dst.y = 0;
        if (dst.y < SCREEN_HEIGHT - dst.h) dst.y = SCREEN_HEIGHT - dst.h;
    } else {
        if (dst.y < 0) dst.y = 0;
        if (dst.y > SCREEN_HEIGHT - dst.h) dst.y = SCREEN_HEIGHT - dst.h;
    }
}

void draw_checkerboard_pattern(SDL_Renderer* renderer, int width, int height, int cell_size) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);

    bool toggle = false;
    for (int y = 0; y < height; y += cell_size) {
        toggle = !toggle;
        for (int x = 0; x < width; x += cell_size) {
            if (toggle) {
                SDL_Rect cell = {x, y, cell_size, cell_size};
                SDL_RenderFillRect(renderer, &cell);
            }
            toggle = !toggle;
        }
    }
}

void photo_viewer_render() {
    if (!photo_texture) return;

    int screen_w, screen_h;
    SDL_GetRendererOutputSize(photo_renderer, &screen_w, &screen_h);

    draw_checkerboard_pattern(photo_renderer, screen_w, screen_h, 40);

    if (!dst_set) {
        int tex_w, tex_h;
        SDL_QueryTexture(photo_texture, nullptr, nullptr, &tex_w, &tex_h);

        dst.w = tex_w * scale;
        dst.h = tex_h * scale;

        dst_set = true;
    }

    SDL_RenderCopy(photo_renderer, photo_texture, nullptr, &dst);
}

void photo_viewer_cleanup() {
    if (photo_texture) {
        SDL_DestroyTexture(photo_texture);
        photo_texture = nullptr;
    }
}
