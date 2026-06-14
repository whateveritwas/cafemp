#version 420 core

layout(binding = 0) uniform sampler2D tex_y;
layout(binding = 1) uniform sampler2D tex_u;
layout(binding = 2) uniform sampler2D tex_v;

in  vec2 v_uv;
out vec4 out_color;

void main() {
    float y = texture(tex_y, v_uv).r;
    float u = texture(tex_u, v_uv).r;
    float v = texture(tex_v, v_uv).r;

    float Y = 1.16438 * (y - 16.0  / 255.0);
    float Cb = u - 128.0 / 255.0;
    float Cr = v - 128.0 / 255.0;

    out_color = clamp(vec4(
        Y + 1.59603 * Cr,
        Y - 0.39176 * Cb - 0.81297 * Cr,
        Y + 2.01723 * Cb,
        1.0
    ), 0.0, 1.0);
}