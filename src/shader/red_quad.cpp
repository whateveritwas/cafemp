#include <whb/gfx.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include "logger/logger.hpp"

#include "shader/red_quad.hpp"

static WHBGfxShaderGroup shaderGroup;

struct Vertex {
	float pos[2];
	float tex[2];
};

static Vertex quad[4] = {
    {{-1,  1}, {0, 0}},
    {{ 1,  1}, {1, 0}},
    {{-1, -1}, {0, 1}},
    {{ 1, -1}, {1, 1}},
};

void red_quad_init() {
    if (!WHBGfxLoadGFDShaderGroup(&shaderGroup, 0, red_quad_gsh)) {
        log_message(LOG_ERROR, "red_quad", "Cannot load shader");
        return;
    }

    WHBGfxInitShaderAttribute(&shaderGroup, "a_position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&shaderGroup, "a_texcoord", 0, sizeof(float)*2, GX2_ATTRIB_FORMAT_FLOAT_32_32);

    if (!WHBGfxInitFetchShader(&shaderGroup)) {
        log_message(LOG_ERROR, "red_quad", "Cannot init fetch shader");
    }
}

void render() {
    WHBGfxClearColor(0,0,0,1);

    GX2SetFetchShader(&shaderGroup.fetchShader);
    GX2SetVertexShader(shaderGroup.vertexShader);
    GX2SetPixelShader(shaderGroup.pixelShader);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, quad, sizeof(quad));
    GX2SetAttribBuffer(0, sizeof(quad), sizeof(Vertex), quad); // position
    GX2SetAttribBuffer(1, sizeof(quad), sizeof(Vertex), quad); // texcoord

    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);
}

void red_quad_render() {
    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();

    render();

    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();

    render();

    WHBGfxFinishRenderDRC();
    WHBGfxFinishRender();
}

void red_quad_shutdown() {
    WHBGfxFreeShaderGroup(&shaderGroup);
}
