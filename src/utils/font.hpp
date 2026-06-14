#ifndef FONT_HPP
#define FONT_HPP
#include "main.hpp"
#include <coreinit/memory.h>
#include "vendor/ui/backends/imgui_impl_wiiu.h"
#include "logger/logger.hpp"

static const float default_font_size = 32;

#define FONT_GLYPH_A_BUTTON "\uE000"
#define FONT_GLYPH_B_BUTTON "\uE001"
#define FONT_GLYPH_X_BUTTON "\uE002"
#define FONT_GLYPH_Y_BUTTON "\uE003"
#define FONT_GLYPH_ANALOG_STICK "\uE080"
#define FONT_GLYPH_LEFT_ANALOG_STICK "\uE081"
#define FONT_GLYPH_RIGHT_ANALOG_STICK "\uE082"
#define FONT_GLYPH_L_BUTTON "\uE083"
#define FONT_GLYPH_R_BUTTON "\uE084"
#define FONT_GLYPH_ZL_BUTTON "\uE085"
#define FONT_GLYPH_ZR_BUTTON "\uE086"
#define FONT_GLYPH_WII_U_GAMEPAD "\uE087"
#define FONT_GLYPH_WIIMOTE "\uE088"
#define FONT_GLYPH_TV_BUTTON "\uE089"
#define FONT_GLYPH_LEFT_STICK_DOWN "\uE08A"
#define FONT_GLYPH_RIGHT_STICK_DOWN "\uE08B"
#define FONT_GLYPH_NFC_SENSOR "\uE099"
#define FONT_GLYPH_CLOSE_TAB "\uE098"
#define FONT_GLYPH_LEFT_TO_RIGHT_ARROWS "\uE08C"
#define FONT_GLYPH_UP_TO_DOWN_ARROWS "\uE08D"
#define FONT_GLYPH_CLOCKWISE_ARROW "\uE08E"
#define FONT_GLYPH_ANTICLOCKWISE_ARROW "\uE08F"
#define FONT_GLYPH_RIGHT_ARROW "\uE090"
#define FONT_GLYPH_LEFT_ARROW "\uE091"
#define FONT_GLYPH_UP_ARROW "\uE092"
#define FONT_GLYPH_DOWN_ARROW "\uE093"
#define FONT_GLYPH_UP_RIGHT_ARROW "\uE094"
#define FONT_GLYPH_DOWN_RIGHT_ARROW "\uE095"
#define FONT_GLYPH_DOWN_LEFT_ARROW "\uE096"
#define FONT_GLYPH_UP_LEFT_ARROW "\uE097"
#define FONT_GLYPH_CIRCLE_PAD "\uE077"
#define FONT_GLYPH_DPAD_UP "\uE079"
#define FONT_GLYPH_DPAD_DOWN "\uE07A"
#define FONT_GLYPH_DPAD_LEFT "\uE07B"
#define FONT_GLYPH_DPAD_RIGHT "\uE07C"
#define FONT_GLYPH_DPAD_UP_DOWN "\uE07D"
#define FONT_GLYPH_DPAD_LEFT_RIGHT "\uE07E"
#define FONT_GLYPH_POWER_BUTTON "\uE078"
#define FONT_GLYPH_VIDEO_ICON "\uE076"
#define FONT_GLYPH_TURNING_ARROW "\uE072"
#define FONT_GLYPH_HOME_MENU "\uE073"
#define FONT_GLYPH_PEDOMETER "\uE074"
#define FONT_GLYPH_PLAY_COIN "\uE075"
#define FONT_GLYPH_CLOSE_BUTTON "\uE071"
#define FONT_GLYPH_UNUSED_CLOSE_BUTTON "\uE070"

#define SYMBOLS_FONT "/vol/content/NerdFontsSymbolsOnly/SymbolsNerdFont-Regular.ttf"

#define ICON_HOME "\uf015"
#define ICON_VIDEO "\uf03d"
#define ICON_AUDIO "\uf001"
#define ICON_PHOTO "\uf03e"
#define ICON_LIBRARY "\uf02d"
#define ICON_SETTINGS "\uf013"

static const ImWchar nerd_font_ranges[] = {
    0xE0A0, 0xE0A3, // Powerline
    0xE0B0, 0xE0C8, // Powerline Extra
    0xE0CC, 0xE0D4,
    0xE200, 0xE2A9, // Font Awesome Extension
    0xE300, 0xE3E3, // Weather Icons
    0xE700, 0xE7C5, // Seti-UI / File icons
    0xEA60, 0xEC1E, // Codicons
    0xED00, 0xEFC1,
    0xF000, 0xF2E0, // Font Awesome
    0xF300, 0xF372, // Font Logos
    0xF400, 0xF533, // Octicons
    0xF500, 0xFD46, // Material Design Icons
    0,
};

static void font_load(OSSharedDataType font, bool merge) {
    static const char* names[OS_SHAREDDATATYPE_FONT_MAX] = {
        [OS_SHAREDDATATYPE_FONT_CHINESE] = "CafeCn.ttf",
        [OS_SHAREDDATATYPE_FONT_KOREAN] = "CafeKr.ttf",
        [OS_SHAREDDATATYPE_FONT_STANDARD] = "CafeStd.ttf",
        [OS_SHAREDDATATYPE_FONT_TAIWANESE] = "CafeCn.ttf",
    };
    assert(font < OS_SHAREDDATATYPE_FONT_MAX);

    auto& io = ImGui::GetIO();
    ImFontConfig config;
    config.MergeMode = merge;
    config.EllipsisChar = U'…';
    config.FontDataOwnedByAtlas = false;

    void* font_data = nullptr;
    uint32_t font_size = 0;

    if (OSGetSharedData(font, 0, &font_data, &font_size)) {
        log_message(LOG_DEBUG, "Font", "Loading font \"%s\" with data size %u", names[font], font_size);
        io.Fonts->AddFontFromMemoryTTF(font_data, font_size, default_font_size * UI_SCALE, &config);
    }
}

static void font_load_all() {
    font_load(OS_SHAREDDATATYPE_FONT_STANDARD, false);
    font_load(OS_SHAREDDATATYPE_FONT_CHINESE, true);
    font_load(OS_SHAREDDATATYPE_FONT_KOREAN, true);
    font_load(OS_SHAREDDATATYPE_FONT_TAIWANESE, true);

    auto& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.PixelSnapH = true;
    cfg.GlyphMinAdvanceX = default_font_size;

    io.Fonts->AddFontFromFileTTF(SYMBOLS_FONT, default_font_size, &cfg, nerd_font_ranges);
}
#endif
