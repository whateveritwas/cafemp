#include <harfbuzz/hb.h>     // Core HarfBuzz types and functions
#include <harfbuzz/hb-ft.h>  // FreeType integration for HarfBuzz

//#include <hb-buffer.h> // Buffer manipulation functions

// Note: Ensure that FT_Face type is available.
// You might need to include FreeType headers if not already implicitly included by hb-ft.h
// e.g., #include <ft2build.h> and #include FT_FREETYPE_H

// --- Wrapper implementations for fzhb_ functions ---

// For hb_buffer_add_codepoints
void fzhb_buffer_add_codepoints(hb_buffer_t* buffer, const hb_codepoint_t* text, unsigned int text_length, unsigned int item_offset, unsigned int item_length) {
    hb_buffer_add_codepoints(buffer, text, text_length, item_offset, item_length);
}

// For hb_buffer_create
hb_buffer_t* fzhb_buffer_create(void) {
    return hb_buffer_create();
}

// For hb_buffer_destroy
void fzhb_buffer_destroy(hb_buffer_t* buffer) {
    hb_buffer_destroy(buffer);
}

// For hb_buffer_get_glyph_infos
hb_glyph_info_t* fzhb_buffer_get_glyph_infos(hb_buffer_t* buffer, unsigned int* length) {
    return hb_buffer_get_glyph_infos(buffer, length);
}

// For hb_buffer_get_glyph_positions
hb_glyph_position_t* fzhb_buffer_get_glyph_positions(hb_buffer_t* buffer, unsigned int* length) {
    return hb_buffer_get_glyph_positions(buffer, length);
}

// For hb_buffer_get_length
unsigned int fzhb_buffer_get_length(hb_buffer_t* buffer) {
    return hb_buffer_get_length(buffer);
}

// For hb_buffer_set_cluster_level
void fzhb_buffer_set_cluster_level(hb_buffer_t* buffer, hb_buffer_cluster_level_t cluster_level) {
    hb_buffer_set_cluster_level(buffer, cluster_level);
}

// For hb_buffer_set_content_type
void fzhb_buffer_set_content_type(hb_buffer_t* buffer, hb_buffer_content_type_t content_type) {
    hb_buffer_set_content_type(buffer, content_type);
}

// For hb_buffer_set_direction
void fzhb_buffer_set_direction(hb_buffer_t* buffer, hb_direction_t direction) {
    hb_buffer_set_direction(buffer, direction);
}

// For hb_buffer_set_flags
void fzhb_buffer_set_flags(hb_buffer_t* buffer, hb_buffer_flags_t flags) {
    hb_buffer_set_flags(buffer, flags);
}

// For hb_buffer_set_script
void fzhb_buffer_set_script(hb_buffer_t* buffer, hb_script_t script) {
    hb_buffer_set_script(buffer, script);
}

// For hb_buffer_set_segment_properties
void fzhb_buffer_set_segment_properties(hb_buffer_t* buffer, hb_segment_properties_t* props) {
    hb_buffer_set_segment_properties(buffer, props);
}

// For fzhb_ft_font_create (requires FT_Face from FreeType)
// You might need to add: #include <ft2build.h> and #include FT_FREETYPE_H
hb_font_t* fzhb_ft_font_create(FT_Face ft_face, hb_destroy_func_t destroy) {
    return hb_ft_font_create(ft_face, destroy);
}

// For fzhb_ft_font_changed
void fzhb_ft_font_changed(hb_font_t* font) {
    hb_ft_font_changed(font);
}

// For fzhb_ft_font_get_face
FT_Face fzhb_ft_font_get_face(hb_font_t* font) {
    return hb_ft_font_get_face(font);
}

// For hb_shape
void fzhb_shape(hb_font_t* font, hb_buffer_t* buffer, const hb_feature_t* features, unsigned int num_features) {
    hb_shape(font, buffer, features, num_features);
}

// For hb_font_destroy
void fzhb_font_destroy(hb_font_t* font) {
    hb_font_destroy(font);
}
// We might also need other includes depending on the types used by MuPDF,
// such as fz_buffer for MuPDF's buffer type, if fzhb_buffer_t is not directly hb_buffer_t.

// Assuming fzhb_buffer_t is compatible with hb_buffer_t
// and fzhb_language_t is compatible with hb_language_t etc.
// If not, we might need typedefs or casts to reconcile the types.

void fzhb_buffer_clear_contents(hb_buffer_t *buffer) {
    if (buffer) {
        hb_buffer_clear_contents(buffer);
    }
}

hb_language_t fzhb_language_from_string(const char *str) {
    if (str) {
        return hb_language_from_string(str, -1); // -1 to auto-detect length
    }
    return HB_LANGUAGE_INVALID;
}

void fzhb_buffer_set_language(hb_buffer_t *buffer, hb_language_t language) {
    if (buffer) {
        hb_buffer_set_language(buffer, language);
    }
}

void fzhb_buffer_add_utf8(hb_buffer_t *buffer, const char *text, int text_length, unsigned int item_offset, int item_length) {
    if (buffer && text) {
        hb_buffer_add_utf8(buffer, text, text_length, item_offset, item_length);
    }
}

void fzhb_buffer_guess_segment_properties(hb_buffer_t *buffer) {
    if (buffer) {
        hb_buffer_guess_segment_properties(buffer);
    }
}
