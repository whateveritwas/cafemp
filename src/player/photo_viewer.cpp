#include <SDL2/SDL_image.h>
#include <gif_lib.h>
#include <vector>
#include <string.h>

#include "main.hpp"
#include "utils.hpp"
#include "player/photo_viewer.hpp"

static SDL_Renderer* photo_renderer = nullptr;
static SDL_Texture* photo_texture = nullptr;

struct GIFFrame {
    SDL_Texture* texture;
    int delay_ms;
};

static std::vector<GIFFrame> gif_frames;
static int current_gif_frame = 0;
static Uint32 last_frame_time = 0;

float scale = 1.0f;
const float min_scale = 0.5f;
const float max_scale = 5.0f;

SDL_Rect dst = { 0, 0, 0, 0 };
bool dst_set = false;

void photo_viewer_init(SDL_Renderer* renderer, SDL_Texture*& texture) {
    photo_renderer = renderer;
    photo_texture = texture;
}

void clamp_dst() {
    if (dst.w > SCREEN_WIDTH) {
        if (dst.x > 0) dst.x = 0;
        if (dst.x < SCREEN_WIDTH - dst.w) dst.x = SCREEN_WIDTH - dst.w;
    } else {
        if (dst.x < 0) dst.x = 0;
        if (dst.x > SCREEN_WIDTH - dst.w) dst.x = SCREEN_WIDTH - dst.w;
    }

    if (dst.h > SCREEN_HEIGHT) {
        if (dst.y > 0) dst.y = 0;
        if (dst.y < SCREEN_HEIGHT - dst.h) dst.y = SCREEN_HEIGHT - dst.h;
    } else {
        if (dst.y < 0) dst.y = 0;
        if (dst.y > SCREEN_HEIGHT - dst.h) dst.y = SCREEN_HEIGHT - dst.h;
    }
}

bool load_gif(const char* filepath, SDL_Renderer* renderer) {
    int err = 0;
    GifFileType* gif = DGifOpenFileName(filepath, &err);
    if (!gif) {
        printf("[GIF] Failed to open: %s (error code: %d)\n", filepath, err);
        return false;
    }

    if (DGifSlurp(gif) != GIF_OK) {
        printf("[GIF] Failed to read data: %s\n", filepath);
        DGifCloseFile(gif, &err);
        return false;
    }

    gif_frames.clear();

    SDL_Surface* canvas = SDL_CreateRGBSurface(0, gif->SWidth, gif->SHeight, 32,
                                               0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!canvas) {
        DGifCloseFile(gif, &err);
        return false;
    }
    SDL_FillRect(canvas, nullptr, SDL_MapRGBA(canvas->format, 0, 0, 0, 0));

    SDL_Surface* backup = SDL_CreateRGBSurface(0, gif->SWidth, gif->SHeight, 32,
                                               0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    GifColorType* globalColors = gif->SColorMap ? gif->SColorMap->Colors : nullptr;

    for (int i = 0; i < gif->ImageCount; ++i) {
        SavedImage& frame = gif->SavedImages[i];
        GifImageDesc& desc = frame.ImageDesc;

        GifColorType* colors = desc.ColorMap ? desc.ColorMap->Colors : globalColors;
        if (!colors) continue;

        int delay = 100;
        int transparentIndex = -1;
        int disposal = 0;
        for (int j = 0; j < frame.ExtensionBlockCount; ++j) {
            ExtensionBlock& ext = frame.ExtensionBlocks[j];
            if (ext.Function == GRAPHICS_EXT_FUNC_CODE && ext.ByteCount >= 4) {
                delay = ((ext.Bytes[2] << 8) | ext.Bytes[1]) * 10;
                delay = delay < 20 ? 20 : delay; // enforce min delay
                transparentIndex = (ext.Bytes[0] & 0x01) ? (unsigned char)ext.Bytes[3] : -1;
                disposal = (ext.Bytes[0] >> 2) & 0x07;
            }
        }

        // --- Handle disposal of previous frame ---
        if (i > 0) {
            if (disposal == 2) {
                SDL_Rect r = { desc.Left, desc.Top, desc.Width, desc.Height };
                SDL_FillRect(canvas, &r, SDL_MapRGBA(canvas->format, 0, 0, 0, 0));
            } else if (disposal == 3 && backup) {
                SDL_BlitSurface(backup, nullptr, canvas, nullptr);
            }
        }

        if (disposal == 3 && backup) {
            SDL_BlitSurface(canvas, nullptr, backup, nullptr);
        }

        // --- Draw current frame onto canvas ---
        Uint32* pixels = (Uint32*)canvas->pixels;
        for (int y = 0; y < desc.Height; ++y) {
            for (int x = 0; x < desc.Width; ++x) {
                int idx = y * desc.Width + x;
                int color_idx = frame.RasterBits[idx];
                if (color_idx == transparentIndex) continue;

                GifColorType c = colors[color_idx];
                int px = desc.Left + x;
                int py = desc.Top + y;

                if (px < gif->SWidth && py < gif->SHeight) {
                    Uint32 color = SDL_MapRGBA(canvas->format, c.Red, c.Green, c.Blue, 255);
                    pixels[py * gif->SWidth + px] = color;
                }
            }
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, canvas);
        if (!tex) continue;

        gif_frames.push_back({ tex, delay });
    }

    SDL_FreeSurface(canvas);
    SDL_FreeSurface(backup);
    DGifCloseFile(gif, &err);

    current_gif_frame = 0;
    last_frame_time = SDL_GetTicks();
    return !gif_frames.empty();
}

void photo_viewer_open_picture(const char* filepath) {
    photo_viewer_cleanup();

    const char* ext = strrchr(filepath, '.');
    if (ext && strcasecmp(ext, ".gif") == 0) {
        if (load_gif(filepath, photo_renderer)) {
            int tex_w, tex_h;
            SDL_QueryTexture(gif_frames[0].texture, nullptr, nullptr, &tex_w, &tex_h);
            dst = calculate_aspect_fit_rect(tex_w, tex_h);
            scale = (float)dst.w / tex_w;
            dst_set = true;
            return;
        } else {
            printf("[Viewer] Failed to load animated GIF: %s\n", filepath);
            return;
        }
    }

    SDL_Surface* surface = IMG_Load(filepath);
    if (!surface) {
        printf("[Viewer] Failed to load image: %s\n", IMG_GetError());
        return;
    }

    photo_texture = SDL_CreateTextureFromSurface(photo_renderer, surface);
    SDL_FreeSurface(surface);

    if (!photo_texture) {
        printf("[Viewer] Failed to create texture: %s\n", SDL_GetError());
        return;
    }

    int tex_w, tex_h;
    SDL_QueryTexture(photo_texture, nullptr, nullptr, &tex_w, &tex_h);
    dst = calculate_aspect_fit_rect(tex_w, tex_h);
    scale = (float)dst.w / tex_w;
    dst_set = true;
}

void photo_texture_zoom(float delta_zoom) {
    scale += delta_zoom;
    if (scale > max_scale) scale = max_scale;
    if (scale < min_scale) scale = min_scale;

    int tex_w, tex_h;
    if (!gif_frames.empty()) {
        SDL_QueryTexture(gif_frames[current_gif_frame].texture, nullptr, nullptr, &tex_w, &tex_h);
    } else if (photo_texture) {
        SDL_QueryTexture(photo_texture, nullptr, nullptr, &tex_w, &tex_h);
    } else {
        return;
    }

    dst.w = (int)(tex_w * scale);
    dst.h = (int)(tex_h * scale);

    clamp_dst();
    dst_set = true;
}

void photo_viewer_pan(int delta_x, int delta_y) {
    dst.x += delta_x;
    dst.y += delta_y;
    clamp_dst();
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
                SDL_Rect cell = { x, y, cell_size, cell_size };
                SDL_RenderFillRect(renderer, &cell);
            }
            toggle = !toggle;
        }
    }
}

void photo_viewer_render() {
    int screen_w, screen_h;
    SDL_GetRendererOutputSize(photo_renderer, &screen_w, &screen_h);
    draw_checkerboard_pattern(photo_renderer, screen_w, screen_h, 40);

    SDL_Texture* tex = nullptr;
    if (!gif_frames.empty()) {
        Uint32 now = SDL_GetTicks();
        if (now - last_frame_time >= (Uint32)gif_frames[current_gif_frame].delay_ms) {
            current_gif_frame = (current_gif_frame + 1) % gif_frames.size();
            last_frame_time = now;
        }
        tex = gif_frames[current_gif_frame].texture;
    } else {
        tex = photo_texture;
    }

    if (!tex) return;

    if (!dst_set) {
        int tex_w, tex_h;
        SDL_QueryTexture(tex, nullptr, nullptr, &tex_w, &tex_h);
        dst = calculate_aspect_fit_rect(tex_w, tex_h);
        scale = (float)dst.w / tex_w;
        dst_set = true;
    }

    SDL_RenderCopy(photo_renderer, tex, nullptr, &dst);
}

void photo_viewer_cleanup() {
    for (auto& frame : gif_frames) {
        if (frame.texture) SDL_DestroyTexture(frame.texture);
    }
    gif_frames.clear();

    if (photo_texture) {
        SDL_DestroyTexture(photo_texture);
        photo_texture = nullptr;
    }
}
