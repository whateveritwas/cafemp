#include <algorithm>
#include <backends/imgui_impl_gx2.h>
#include <coreinit/time.h>
#include <gif_lib.h>
#include <imgui.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "player/photo_viewer.hpp"
#include "utils/display.hpp"

#include <stb_image.h>

#define MIN_ZOOM_SCALE 0.5f
#define MAX_ZOOM_SCALE 5.0f

struct GX2Image {
    ImTextureData *texture = nullptr;
    ImTextureID tex_id = 0;
    int width = 0;
    int height = 0;
};

struct GIFFrame {
    GX2Image image;
    int delay_ms;
};

static std::vector<GIFFrame> gif_frames;
static GX2Image static_image;

static int current_gif_frame = 0;
static uint64_t last_frame_time = 0;

static float scale = 1.0f;
static rect dst = {0, 0, 0, 0};
static bool dst_set = false;

static uint64_t current_time_ms() { return OSTicksToMilliseconds(OSGetTime()); }

static GX2Image create_texture_rgba(const uint8_t *rgba, int width, int height, bool is_gif) {
    GX2Image result;

    ImTextureData *tex = IM_NEW(ImTextureData);
    tex->Create(ImTextureFormat_RGBA32, width, height);

    uint32_t *dst = reinterpret_cast<uint32_t *>(tex->GetPixels());

    if (is_gif) {
        memcpy(dst, rgba, width * height * 4);
    } else {
        for (int i = 0; i < width * height; ++i) {
            uint8_t r = rgba[i * 4 + 0];
            uint8_t g = rgba[i * 4 + 1];
            uint8_t b = rgba[i * 4 + 2];
            uint8_t a = rgba[i * 4 + 3];

            dst[i] = (a << 24) | (b << 16) | (g << 8) | (r);
        }
    }

    tex->SetStatus(ImTextureStatus_WantCreate);
    ImGui_ImplGX2_HandleTexture(tex);

    result.texture = tex;
    result.tex_id = tex->TexID;
    result.width = width;
    result.height = height;

    return result;
}

static void destroy_texture(GX2Image &image) {
    if (!image.texture) return;

    image.texture->SetStatus(ImTextureStatus_WantDestroy);
    ImGui_ImplGX2_HandleTexture(image.texture);

    IM_DELETE(image.texture);

    image.texture = nullptr;
    image.tex_id = 0;
    image.width = 0;
    image.height = 0;
}

void photo_viewer_init() {
    current_gif_frame = 0;
    last_frame_time = 0;
    scale = 1.0f;
    dst = {0, 0, 0, 0};
    dst_set = false;
}

static void clamp_dst() {
    float max_x = display_get().width - dst.w;
    float max_y = display_get().height - dst.h;

    if (dst.w > display_get().width) {
        if (dst.x > 0) dst.x = 0;
        if (dst.x < max_x) dst.x = max_x;
    } else {
        if (dst.x < 0) dst.x = 0;
        if (dst.x > max_x) dst.x = max_x;
    }

    if (dst.h > display_get().height) {
        if (dst.y > 0) dst.y = 0;
        if (dst.y < max_y) dst.y = max_y;
    } else {
        if (dst.y < 0) dst.y = 0;
        if (dst.y > max_y) dst.y = max_y;
    }
}

static bool load_gif(const char *filepath) {
    int err = 0;
    GifFileType *gif = DGifOpenFileName(filepath, &err);

    if (!gif) return false;
    if (DGifSlurp(gif) != GIF_OK) {
        DGifCloseFile(gif, &err);
        return false;
    }

    gif_frames.clear();

    std::vector<uint32_t> canvas(gif->SWidth * gif->SHeight, 0);
    std::vector<uint32_t> backup = canvas;

    GifColorType *global_colors = gif->SColorMap ? gif->SColorMap->Colors : nullptr;

    for (int i = 0; i < gif->ImageCount; ++i) {
        SavedImage &frame = gif->SavedImages[i];
        GifImageDesc &desc = frame.ImageDesc;

        GifColorType *colors = desc.ColorMap ? desc.ColorMap->Colors : global_colors;

        if (!colors) continue;

        int delay = 100;
        int transparent_index = -1;
        int disposal = 0;

        for (int j = 0; j < frame.ExtensionBlockCount; ++j) {
            ExtensionBlock &ext = frame.ExtensionBlocks[j];

            if (ext.Function == GRAPHICS_EXT_FUNC_CODE && ext.ByteCount >= 4) {
                delay = ((ext.Bytes[2] << 8) | ext.Bytes[1]) * 10;
                if (delay < 20) delay = 20;

                transparent_index = (ext.Bytes[0] & 0x01) ? (unsigned char)ext.Bytes[3] : -1;

                disposal = (ext.Bytes[0] >> 2) & 0x07;
            }
        }

        if (i > 0) {
            if (disposal == 2) {
                for (int y = 0; y < desc.Height; ++y)
                    for (int x = 0; x < desc.Width; ++x) {
                        int px = desc.Left + x;
                        int py = desc.Top + y;
                        if (px < gif->SWidth && py < gif->SHeight) canvas[py * gif->SWidth + px] = 0;
                    }
            } else if (disposal == 3) {
                canvas = backup;
            }
        }

        if (disposal == 3) backup = canvas;

        for (int y = 0; y < desc.Height; ++y)
            for (int x = 0; x < desc.Width; ++x) {
                int idx = y * desc.Width + x;
                int ci = frame.RasterBits[idx];

                if (ci == transparent_index) continue;

                GifColorType c = colors[ci];

                int px = desc.Left + x;
                int py = desc.Top + y;

                if (px < gif->SWidth && py < gif->SHeight) {
                    canvas[py * gif->SWidth + px] = (255u << 24) | (c.Blue << 16) | (c.Green << 8) | (c.Red);
                }
            }

        GIFFrame out;
        out.image = create_texture_rgba(reinterpret_cast<uint8_t *>(canvas.data()), gif->SWidth, gif->SHeight, true);

        out.delay_ms = delay;

        gif_frames.push_back(out);
    }

    DGifCloseFile(gif, &err);

    current_gif_frame = 0;
    last_frame_time = current_time_ms();

    return !gif_frames.empty();
}

void photo_viewer_open_picture(const char *filepath) {
    photo_viewer_cleanup();

    const char *ext = strrchr(filepath, '.');

    if (ext && strcasecmp(ext, ".gif") == 0) {
        load_gif(filepath);
        if (gif_frames.empty()) return;

        rect r = display_calculate_aspect_fit(gif_frames[0].image.width, gif_frames[0].image.height);

        dst = r;
        scale = dst.w / gif_frames[0].image.width;
        dst_set = true;
        return;
    }

    int w, h, comp;
    uint8_t *pixels = stbi_load(filepath, &w, &h, &comp, 4);

    if (!pixels) return;

    static_image = create_texture_rgba(pixels, w, h, false);
    stbi_image_free(pixels);

    rect r = display_calculate_aspect_fit(w, h);
    dst = r;
    scale = dst.w / w;
    dst_set = true;
}

void photo_texture_zoom(float delta_zoom) {
    scale = std::clamp(scale + delta_zoom, MIN_ZOOM_SCALE, MAX_ZOOM_SCALE);

    int w = gif_frames.empty() ? static_image.width : gif_frames[current_gif_frame].image.width;
    int h = gif_frames.empty() ? static_image.height : gif_frames[current_gif_frame].image.height;

    dst.w = w * scale;
    dst.h = h * scale;

    clamp_dst();
    dst_set = true;
}

void photo_viewer_pan(int dx, int dy) {
    dst.x += dx;
    dst.y += dy;
    clamp_dst();
}

void photo_viewer_render() {
    ImDrawList *draw = ImGui::GetBackgroundDrawList();

    ImTextureID tex = 0;
    int w = 0, h = 0;

    if (!gif_frames.empty()) {
        uint64_t now = current_time_ms();

        if (now - last_frame_time >= (uint64_t)gif_frames[current_gif_frame].delay_ms) {
            current_gif_frame = (current_gif_frame + 1) % gif_frames.size();
            last_frame_time = now;
        }

        tex = gif_frames[current_gif_frame].image.tex_id;
        w = gif_frames[current_gif_frame].image.width;
        h = gif_frames[current_gif_frame].image.height;
    } else {
        tex = static_image.tex_id;
        w = static_image.width;
        h = static_image.height;
    }

    if (!tex) return;

    if (!dst_set) {
        rect r = display_calculate_aspect_fit(w, h);
        dst = r;
        scale = dst.w / w;
        dst_set = true;
    }

    draw->AddImage(tex, ImVec2(dst.x, dst.y), ImVec2(dst.x + dst.w, dst.y + dst.h));
}

void photo_viewer_cleanup() {
    for (auto &f : gif_frames)
        destroy_texture(f.image);

    gif_frames.clear();

    destroy_texture(static_image);

    current_gif_frame = 0;
    dst_set = false;
}
