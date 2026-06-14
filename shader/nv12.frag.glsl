#version 420 core

layout(binding = 0) uniform sampler2D tex_y;
layout(binding = 1) uniform sampler2D tex_uv;  // .r = U (Cb), .g = V (Cr)

in  vec2 v_uv;
out vec4 out_color;

void main() {
    float y = texture(tex_y,  v_uv).r;
    vec2 uv = texture(tex_uv, v_uv).rg;

    float Y = 1.16438 * (y - 16.0 / 255.0);
    float Cb = uv.r - 128.0 / 255.0;
    float Cr = uv.g - 128.0 / 255.0;

    out_color = clamp(vec4(
        Y + 1.59603 * Cr,
        Y - 0.39176 * Cb - 0.81297 * Cr,
        Y + 2.01723 * Cb,
        1.0
    ), 0.0, 1.0);
}