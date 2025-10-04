#include <whb/gfx.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/utils.h>

#include "main.hpp"
#include "logger/logger.hpp"
#include "utils/sdl.hpp"
#include "shader/yuv2rgb.hpp"

static WHBGfxShaderGroup shaderGroup;
static GX2Sampler sampler;

static Vertex quad[4];
static float ySize[2];
static float uvSize[2];

static bool quad_set = false;
static yuv_texture tex;
static bool textures_created = false;

float yuv2rgb601[3][3] = {
    {1.0f,  0.0f,  1.402f},
    {1.0f, -0.344136f, -0.714136f},
    {1.0f,  1.772f, 0.0f}
};

float yuv2rgb709[3][3] = {
    {1.0f,  0.0f,  1.5748f},
    {1.0f, -0.187324f, -0.468124f},
    {1.0f,  1.8556f, 0.0f}
};

static void update_quad_aspect(float videoWidth, float videoHeight, float screenWidth, float screenHeight) {
    float screenAspect = screenWidth / screenHeight;
    float videoAspect  = videoWidth / videoHeight;

    float scaleX = 1.0f;
    float scaleY = 1.0f;

    if (videoAspect > screenAspect) {
        scaleY = screenAspect / videoAspect;
    } else {
        scaleX = videoAspect / screenAspect;
    }

    quad[0] = {{-scaleX,  scaleY}, {0, 0}};
    quad[1] = {{ scaleX,  scaleY}, {1, 0}};
    quad[2] = {{-scaleX, -scaleY}, {0, 1}};
    quad[3] = {{ scaleX, -scaleY}, {1, 1}};
}

void yuv2rgb_init() {
    memset(&shaderGroup, 0, sizeof(shaderGroup));

    if (!WHBGfxLoadGFDShaderGroup(&shaderGroup, 0, yuv2rgb_shader_code)) {
        log_message(LOG_ERROR, "yuv2rgb", "Cannot load shader");
        return;
    }

    WHBGfxInitShaderAttribute(&shaderGroup, "a_position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&shaderGroup, "a_texcoord", 1, sizeof(float)*2, GX2_ATTRIB_FORMAT_FLOAT_32_32);

    if (!WHBGfxInitFetchShader(&shaderGroup)) {
        log_message(LOG_ERROR, "yuv2rgb", "Cannot init fetch shader");
        return;
    }

    GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    quad_set = false;
    textures_created = false;

    sdl_get()->use_native_renderer = true;

    log_message(LOG_OK, "yuv2rgb", "Shader initialized");
}

yuv_texture* make_yuv_texture(AVFrame* frame) {
    if (!textures_created) {
        memset(&tex, 0, sizeof(tex));

        tex.widthY  = frame->width;
        tex.heightY = frame->height;

        tex.widthUV  = frame->width / 2;
        tex.heightUV = frame->height / 2;

        tex.yPlane.surface.dim       = GX2_SURFACE_DIM_TEXTURE_2D;
        tex.yPlane.surface.use       = GX2_SURFACE_USE_TEXTURE;
        tex.yPlane.surface.format    = GX2_SURFACE_FORMAT_UNORM_R8;
        tex.yPlane.surface.tileMode  = GX2_TILE_MODE_LINEAR_ALIGNED;
        tex.yPlane.surface.width     = frame->width;
        tex.yPlane.surface.height    = frame->height;
        tex.yPlane.surface.mipLevels = 1;
        tex.yPlane.viewNumSlices     = 1;
        tex.yPlane.viewNumMips       = 1;
        tex.yPlane.compMap           = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
        GX2CalcSurfaceSizeAndAlignment(&tex.yPlane.surface);
        GX2InitTextureRegs(&tex.yPlane);

        tex.uPlane.surface           = tex.yPlane.surface;
        tex.uPlane.surface.width     = frame->width / 2;
        tex.uPlane.surface.height    = frame->height / 2;
        tex.uPlane.compMap           = tex.yPlane.compMap;
        GX2InitTextureRegs(&tex.uPlane);

        tex.vPlane.surface           = tex.yPlane.surface;
        tex.vPlane.surface.width     = frame->width / 2;
        tex.vPlane.surface.height    = frame->height / 2;
        tex.vPlane.compMap           = tex.yPlane.compMap;
        GX2InitTextureRegs(&tex.vPlane);

        ySize[0]  = 1.0f; ySize[1]  = 1.0f;
        uvSize[0] = 1.0f; uvSize[1] = 1.0f;

        textures_created = true;
    }

    tex.yPlane.surface.image = frame->data[0];
    tex.uPlane.surface.image = frame->data[1];
    tex.vPlane.surface.image = frame->data[2];

    return &tex;
}


void yuv2rgb_render(yuv_texture* tex) {
	if (!quad_set) {
		update_quad_aspect(float(tex->widthY), float(tex->heightY), float(SCREEN_WIDTH), float(SCREEN_HEIGHT));

		ySize[0] = float(tex->widthY) / float(tex->yPlane.surface.width);
		ySize[1] = float(tex->heightY) / float(tex->yPlane.surface.height);

		uvSize[0] = float(tex->widthUV) / float(tex->uPlane.surface.width);
		uvSize[1] = float(tex->heightUV) / float(tex->uPlane.surface.height);

		quad_set = true;
	}

    GX2SetPixelTexture(&tex->yPlane, 0);
    GX2SetPixelTexture(&tex->uPlane, 1);
    GX2SetPixelTexture(&tex->vPlane, 2);

    GX2SetPixelUniformReg(0, 2, ySize);
    GX2SetPixelUniformReg(1, 2, uvSize);
    GX2SetPixelUniformReg(2, 2, uvSize);

    GX2SetPixelSampler(&sampler, 0);
    GX2SetPixelSampler(&sampler, 1);
    GX2SetPixelSampler(&sampler, 2);

    GX2SetFetchShader(&shaderGroup.fetchShader);
    GX2SetVertexShader(shaderGroup.vertexShader);
    GX2SetPixelShader(shaderGroup.pixelShader);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, quad, sizeof(quad));
    GX2SetAttribBuffer(0, sizeof(quad), sizeof(Vertex), quad); // position
    GX2SetAttribBuffer(1, sizeof(quad), sizeof(Vertex), quad); // texcoord

    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);
}

void yuv2rgb_shutdown() {
    WHBGfxFreeShaderGroup(&shaderGroup);

    sdl_get()->use_native_renderer = false;

    log_message(LOG_OK, "yuv2rgb", "Shader unloaded");
}
