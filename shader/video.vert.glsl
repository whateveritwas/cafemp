#version 420 core

layout(location = 0) in vec2 in_pos;

out vec2 v_uv;

void main() {
    gl_Position = vec4(in_pos, 0.0, 1.0);
    v_uv = in_pos * 0.5 + 0.5;
    v_uv.y = 1.0 - v_uv.y;
}