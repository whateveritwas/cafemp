#include <SDL2/SDL.h>
extern "C" {
#include <mupdf/fitz.h>
}

#include "main.hpp"
#include "utils/sdl.hpp"
#include "utils/utils.hpp"
#include "logger/logger.hpp"
#include "player/pdf_viewer.hpp"

struct PdfViewer {
    fz_context* ctx = nullptr;
    fz_document* doc = nullptr;
    int current_page = 0;
    float zoom = 1.0f;
    int pan_x = 0, pan_y = 0;

    SDL_Texture* texture = nullptr;
    int tex_width = 0, tex_height = 0;
};

static PdfViewer g_pdf_viewer;

static SDL_Texture* render_pdf_page_to_texture(
    fz_context* ctx,
    fz_document* doc,
    int page_num,
    float zoom,
    int pan_x,
    int pan_y,
    SDL_Renderer* renderer,
    int* out_w,
    int* out_h
) {
    if (!doc) return nullptr;

    fz_page* page = fz_load_page(ctx, doc, page_num);

    fz_rect bounds = fz_bound_page(ctx, page);
    fz_irect bbox = fz_round_rect(bounds);

    int w = static_cast<int>((bbox.x1 - bbox.x0) * zoom);
    int h = static_cast<int>((bbox.y1 - bbox.y0) * zoom);

    fz_pixmap* pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_irect{0,0,w,h}, nullptr, 0);
    fz_clear_pixmap_with_value(ctx, pix, 0xFF);

    fz_matrix transform = fz_scale(zoom, zoom);
    fz_device* dev = fz_new_draw_device(ctx, transform, pix);
    fz_run_page(ctx, page, dev, fz_identity, nullptr);
    fz_close_device(ctx, dev);
    fz_drop_device(ctx, dev);
    fz_drop_page(ctx, page);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        fz_pixmap_samples(ctx, pix),
        w, h,
        24,
        w * 3,
        SDL_PIXELFORMAT_RGB24
    );

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    fz_drop_pixmap(ctx, pix);

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;

    return texture;
}

void pdf_viewer_init() {
    log_message(LOG_OK, "Pdf Viewer", "Starting Pdf Viewer");
}

void pdf_viewer_open_file(const char* filepath) {
    if (!filepath) return;

    // Cleanup old state
    if (g_pdf_viewer.texture) {
        SDL_DestroyTexture(g_pdf_viewer.texture);
        g_pdf_viewer.texture = nullptr;
    }
    if (g_pdf_viewer.doc) {
        fz_drop_document(g_pdf_viewer.ctx, g_pdf_viewer.doc);
        g_pdf_viewer.doc = nullptr;
    }
    if (!g_pdf_viewer.ctx) {
        g_pdf_viewer.ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
        fz_register_document_handlers(g_pdf_viewer.ctx);
    }

    fz_try(g_pdf_viewer.ctx) {
        g_pdf_viewer.doc = fz_open_document(g_pdf_viewer.ctx, filepath);
        g_pdf_viewer.current_page = 0;
        g_pdf_viewer.zoom = 1.0f;
        g_pdf_viewer.pan_x = g_pdf_viewer.pan_y = 0;
    } fz_catch(g_pdf_viewer.ctx) {
        log_message(LOG_ERROR, "Pdf Viewer", "Failed to open PDF");
        return;
    }

    pdf_viewer_fit_to_screen();
}

void pdf_texture_zoom(float delta_zoom) {
    if (!g_pdf_viewer.doc) return;

    g_pdf_viewer.zoom += delta_zoom;
    if (g_pdf_viewer.zoom < 0.1f) g_pdf_viewer.zoom = 0.1f;

    if (g_pdf_viewer.texture) {
        SDL_DestroyTexture(g_pdf_viewer.texture);
        g_pdf_viewer.texture = nullptr;
    }

    g_pdf_viewer.texture = render_pdf_page_to_texture(
        g_pdf_viewer.ctx,
        g_pdf_viewer.doc,
        g_pdf_viewer.current_page,
        g_pdf_viewer.zoom,
        g_pdf_viewer.pan_x,
        g_pdf_viewer.pan_y,
        sdl_get()->sdl_renderer,
        &g_pdf_viewer.tex_width,
        &g_pdf_viewer.tex_height
    );
}

void pdf_viewer_pan(int delta_x, int delta_y) {
    g_pdf_viewer.pan_x += delta_x;
    g_pdf_viewer.pan_y += delta_y;
    pdf_texture_zoom(0.0f);
}

void pdf_viewer_fit_to_screen() {
    if (!g_pdf_viewer.doc) return;

    fz_page* page = fz_load_page(g_pdf_viewer.ctx, g_pdf_viewer.doc, g_pdf_viewer.current_page);
    fz_rect bounds = fz_bound_page(g_pdf_viewer.ctx, page);
    fz_drop_page(g_pdf_viewer.ctx, page);

    int page_width  = static_cast<int>(bounds.x1 - bounds.x0);

    float zoom = static_cast<float>(SCREEN_HEIGHT) / page_width;

    if (g_pdf_viewer.texture) {
        SDL_DestroyTexture(g_pdf_viewer.texture);
        g_pdf_viewer.texture = nullptr;
    }

    g_pdf_viewer.texture = render_pdf_page_to_texture(
        g_pdf_viewer.ctx,
        g_pdf_viewer.doc,
        g_pdf_viewer.current_page,
        zoom,
        g_pdf_viewer.pan_x,
        g_pdf_viewer.pan_y,
        sdl_get()->sdl_renderer,
        &g_pdf_viewer.tex_width,
        &g_pdf_viewer.tex_height
    );

    g_pdf_viewer.pan_x = (SCREEN_WIDTH - g_pdf_viewer.tex_width) / 2;
    g_pdf_viewer.pan_y = (SCREEN_HEIGHT - g_pdf_viewer.tex_height) / 2;
}

void pdf_viewer_render() {
    auto* r = sdl_get()->sdl_renderer;

    draw_checkerboard_pattern(r, SCREEN_WIDTH, SCREEN_HEIGHT, 40);

    if (g_pdf_viewer.texture) {
        SDL_Rect dst = {
            g_pdf_viewer.pan_x,
            g_pdf_viewer.pan_y,
            g_pdf_viewer.tex_width,
            g_pdf_viewer.tex_height
        };

        SDL_Point center = { dst.w / 2, dst.h / 2 };
        const double rotation_angle = 90.0;

        SDL_RenderCopyEx(r, g_pdf_viewer.texture, nullptr, &dst, rotation_angle, &center, SDL_FLIP_NONE);
    }
}

void pdf_viewer_goto_page(int page_num) {
    if (!g_pdf_viewer.doc) return;
    int page_count = fz_count_pages(g_pdf_viewer.ctx, g_pdf_viewer.doc);

    if (page_num < 0) page_num = 0;
    if (page_num >= page_count) page_num = page_count - 1;

    g_pdf_viewer.current_page = page_num;
    pdf_viewer_fit_to_screen();
}

void pdf_viewer_next_page() {
    pdf_viewer_goto_page(g_pdf_viewer.current_page + 1);
}

void pdf_viewer_prev_page() {
    pdf_viewer_goto_page(g_pdf_viewer.current_page - 1);
}

void pdf_viewer_cleanup() {
    if (g_pdf_viewer.texture) SDL_DestroyTexture(g_pdf_viewer.texture);
    if (g_pdf_viewer.doc) fz_drop_document(g_pdf_viewer.ctx, g_pdf_viewer.doc);
    if (g_pdf_viewer.ctx) fz_drop_context(g_pdf_viewer.ctx);

    g_pdf_viewer.texture = nullptr;
    g_pdf_viewer.doc = nullptr;
    g_pdf_viewer.ctx = nullptr;

    log_message(LOG_OK, "Pdf Viewer", "Cleaned up");
}
