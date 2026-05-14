#include <coreinit/memory.h>
#include <imgui.h>
#include <imgui_impl_wiiu.h>
#include <imgui_impl_gx2.h>

#include "logger/logger.hpp"

static const float default_font_size = 32;

// Wii U / Switch Style Icons
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
        log_message(LOG_DEBUG, "Font", "Loading font \"%s\" with size %d", names[font], sizeof config.Name);
        io.Fonts->AddFontFromMemoryTTF(font_data, font_size, default_font_size, &config);
    }
}

static void font_load_all() {
    font_load(OS_SHAREDDATATYPE_FONT_STANDARD, false);
    font_load(OS_SHAREDDATATYPE_FONT_CHINESE, true);
    font_load(OS_SHAREDDATATYPE_FONT_KOREAN, true);
    font_load(OS_SHAREDDATATYPE_FONT_TAIWANESE, true);
}
