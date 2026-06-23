extern "C" {
#include <mupdf/fitz.h>
}

#include "logger/logger.hpp"
#include "main.hpp"
#include "player/pdf_viewer.hpp"
#include "utils/display.hpp"

#include <backends/imgui_impl_gx2.h>
#include <cstring>
#include <imgui/imgui.h>
#include <vector>

struct GX2Image {
    ImTextureData *texture = nullptr;
    ImTextureID tex_id = 0;
    int width = 0;
    int height = 0;
};

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

static GX2Image create_texture_rgba(const uint8_t *rgba, int width, int height) {
    GX2Image result;

    ImTextureData *tex = IM_NEW(ImTextureData);
    tex->Create(ImTextureFormat_RGBA32, width, height);

    uint32_t *dst_pixels = reinterpret_cast<uint32_t *>(tex->GetPixels());
    for (int i = 0; i < width * height; ++i) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];
        dst_pixels[i] = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
    }

    tex->SetStatus(ImTextureStatus_WantCreate);
    ImGui_ImplGX2_HandleTexture(tex);

    result.texture = tex;
    result.tex_id = tex->TexID;
    result.width = width;
    result.height = height;

    return result;
}

struct PdfViewer {
    fz_context *ctx = nullptr;
    fz_document *doc = nullptr;
    int current_page = 0;
    float zoom = 1.0f;
    int pan_x = 0;
    int pan_y = 0;

    GX2Image image;
    bool texture_dirty = true;
};

static PdfViewer g_pdf_viewer;

static GX2Image render_page_to_gx2(fz_context *ctx, fz_document *doc, int page_num, float zoom) {
    GX2Image result;
    if (!doc) return result;

    fz_page *page = fz_load_page(ctx, doc, page_num);
    fz_rect bounds = fz_bound_page(ctx, page);

    int w = (int)((bounds.x1 - bounds.x0) * zoom);
    int h = (int)((bounds.y1 - bounds.y0) * zoom);

    fz_pixmap *pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_irect{0, 0, w, h}, nullptr, 0);
    fz_clear_pixmap_with_value(ctx, pix, 0xFF);

    fz_matrix transform = fz_scale(zoom, zoom);
    fz_device *dev = fz_new_draw_device(ctx, transform, pix);
    fz_run_page(ctx, page, dev, fz_identity, nullptr);
    fz_close_device(ctx, dev);
    fz_drop_device(ctx, dev);
    fz_drop_page(ctx, page);

    // Expand RGB24 → RGBA32
    const uint8_t *rgb = fz_pixmap_samples(ctx, pix);
    std::vector<uint8_t> rgba(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        rgba[i * 4 + 0] = rgb[i * 3 + 0]; // R
        rgba[i * 4 + 1] = rgb[i * 3 + 1]; // G
        rgba[i * 4 + 2] = rgb[i * 3 + 2]; // B
        rgba[i * 4 + 3] = 0xFF;           // A
    }

    fz_drop_pixmap(ctx, pix);

    result = create_texture_rgba(rgba.data(), w, h);
    return result;
}

static void flush_texture_if_dirty() {
    if (!g_pdf_viewer.texture_dirty) return;
    if (!g_pdf_viewer.doc) return;

    destroy_texture(g_pdf_viewer.image);

    g_pdf_viewer.image = render_page_to_gx2(g_pdf_viewer.ctx, g_pdf_viewer.doc, g_pdf_viewer.current_page, g_pdf_viewer.zoom);

    g_pdf_viewer.texture_dirty = false;
}

static ImVec2 rotate90cw(ImVec2 p, ImVec2 center) {
    float dx = p.x - center.x;
    float dy = p.y - center.y;
    return ImVec2(center.x - dy, center.y + dx);
}

void pdf_viewer_init() { log_message(LOG_OK, "Pdf Viewer", "Starting Pdf Viewer"); }

void pdf_viewer_open_file(const char *filepath) {
    if (!filepath) return;

    destroy_texture(g_pdf_viewer.image);

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
        g_pdf_viewer.pan_x = 0;
        g_pdf_viewer.pan_y = 0;
        g_pdf_viewer.texture_dirty = true;
    }
    fz_catch(g_pdf_viewer.ctx) {
        log_message(LOG_ERROR, "Pdf Viewer", "Failed to open PDF");
        return;
    }

    pdf_viewer_fit_to_screen();
}

void pdf_texture_zoom(float delta_zoom) {
    if (!g_pdf_viewer.doc) return;

    g_pdf_viewer.zoom += delta_zoom;
    if (g_pdf_viewer.zoom < 0.1f) g_pdf_viewer.zoom = 0.1f;

    g_pdf_viewer.texture_dirty = true;
}

void pdf_viewer_pan(int delta_x, int delta_y) {
    g_pdf_viewer.pan_x += delta_x;
    g_pdf_viewer.pan_y += delta_y;
}

void pdf_viewer_fit_to_screen() {
    if (!g_pdf_viewer.doc) return;

    fz_page *page = fz_load_page(g_pdf_viewer.ctx, g_pdf_viewer.doc, g_pdf_viewer.current_page);
    fz_rect bounds = fz_bound_page(g_pdf_viewer.ctx, page);
    fz_drop_page(g_pdf_viewer.ctx, page);

    int page_width = (int)(bounds.x1 - bounds.x0);

    g_pdf_viewer.zoom = display_get().height / page_width;
    g_pdf_viewer.texture_dirty = true;

    flush_texture_if_dirty();

    g_pdf_viewer.pan_x = (display_get().width - g_pdf_viewer.image.width) / 2;
    g_pdf_viewer.pan_y = (display_get().height - g_pdf_viewer.image.height) / 2;
}

void pdf_viewer_render() {
    ImDrawList *draw = ImGui::GetBackgroundDrawList();

    flush_texture_if_dirty();

    if (!g_pdf_viewer.image.tex_id) return;

    const int tw = g_pdf_viewer.image.width;
    const int th = g_pdf_viewer.image.height;
    const float x = (float)(g_pdf_viewer.pan_x);
    const float y = (float)(g_pdf_viewer.pan_y);
    const float w = (float)(tw);
    const float h = (float)(th);

    ImVec2 center(x + w * 0.5f, y + h * 0.5f);

    ImVec2 tl(x, y);
    ImVec2 tr(x + w, y);
    ImVec2 br(x + w, y + h);
    ImVec2 bl(x, y + h);

    ImVec2 rtl = rotate90cw(tl, center);
    ImVec2 rtr = rotate90cw(tr, center);
    ImVec2 rbr = rotate90cw(br, center);
    ImVec2 rbl = rotate90cw(bl, center);

    draw->AddImageQuad(g_pdf_viewer.image.tex_id, rtl, rtr, rbr, rbl, ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1));
}

void pdf_viewer_goto_page(int page_num) {
    if (!g_pdf_viewer.doc) return;

    int page_count = fz_count_pages(g_pdf_viewer.ctx, g_pdf_viewer.doc);
    if (page_num < 0) page_num = 0;
    if (page_num >= page_count) page_num = page_count - 1;

    if (page_num == g_pdf_viewer.current_page) return;

    g_pdf_viewer.current_page = page_num;
    g_pdf_viewer.texture_dirty = true;
    pdf_viewer_fit_to_screen();
}

void pdf_viewer_next_page() { pdf_viewer_goto_page(g_pdf_viewer.current_page + 1); }
void pdf_viewer_prev_page() { pdf_viewer_goto_page(g_pdf_viewer.current_page - 1); }

void pdf_viewer_cleanup() {
    destroy_texture(g_pdf_viewer.image);

    if (g_pdf_viewer.doc) fz_drop_document(g_pdf_viewer.ctx, g_pdf_viewer.doc);
    if (g_pdf_viewer.ctx) fz_drop_context(g_pdf_viewer.ctx);

    g_pdf_viewer = {};
    log_message(LOG_OK, "Pdf Viewer", "Cleaned up");
}
