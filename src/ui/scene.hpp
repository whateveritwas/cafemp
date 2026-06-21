#ifndef UI_SCENE_HPP
#define UI_SCENE_HPP

#include <functional>
#include "input/input_actions.hpp"

struct UIScene {
    std::function<void()> init;
    std::function<void(InputState& input)> input;
    std::function<void()> render;
    std::function<void()> shutdown;
};

void ui_scene_register(int state, const UIScene& scene);
void ui_scene_set(int state);
void ui_scene_input(InputState& input);
void ui_scene_render();
void ui_scene_shutdown();

#endif
