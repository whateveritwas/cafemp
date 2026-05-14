#include <unordered_map>

#include "input/input_actions.hpp"

#include "ui/scene.hpp"

static std::unordered_map<int, UIScene> scenes;
static UIScene* current_scene = nullptr;

void ui_scene_register(int state, const UIScene& scene) {
    scenes[state] = scene;
}

void ui_scene_set(int state) {
    if (current_scene && current_scene->shutdown)
        current_scene->shutdown();

    auto it = scenes.find(state);
    if (it != scenes.end()) {
        current_scene = &it->second;
        if (current_scene->init)
            current_scene->init();
    } else {
        current_scene = nullptr;
    }
}

void ui_scene_input(InputState& input) {
    if (current_scene && current_scene->input)
        current_scene->input(input);
}

void ui_scene_render() {
    if (current_scene && current_scene->render)
        current_scene->render();
}

void ui_scene_shutdown() {
    if (current_scene && current_scene->shutdown)
        current_scene->shutdown();
}
