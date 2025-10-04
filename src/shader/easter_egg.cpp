#ifdef DEBUG
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/display.h>
#include <whb/gfx.h>
#include "logger/logger.hpp"
#include "utils/sdl.hpp"
#include "shader/easter_egg.hpp"

struct Vertex { float pos[2]; };
static Vertex quad[4] = {
    {{-1,  1}}, {{1,  1}},
    {{-1, -1}}, {{1, -1}}
};

static WHBGfxShaderGroup shaderGroup;

void easter_egg_init() {
    if (!WHBGfxLoadGFDShaderGroup(&shaderGroup, 0, easter_egg_shader)) {
        log_message(LOG_ERROR, "easter egg", "Cannot load shader");
        return;
    }
    WHBGfxInitShaderAttribute(&shaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitFetchShader(&shaderGroup);

    sdl_get()->use_native_renderer = true;
}

static void render_quad() {
    WHBGfxClearColor(0,0,0,1);
    GX2SetFetchShader(&shaderGroup.fetchShader);
    GX2SetVertexShader(shaderGroup.vertexShader);
    GX2SetPixelShader(shaderGroup.pixelShader);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, quad, sizeof(quad));
    GX2SetAttribBuffer(0, sizeof(quad), sizeof(Vertex), quad);

    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);
}

void easter_egg_render() {
	render_quad();
}

void easter_egg_shutdown() {
    WHBGfxFreeShaderGroup(&shaderGroup);

    sdl_get()->use_native_renderer = false;
}
#endif
