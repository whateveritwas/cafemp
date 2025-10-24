#include <unordered_map>
#include "vendor/ui/nuklear.h"

#include "ui/scene.hpp"

static std::unordered_map<int, UIScene> g_scenes;
static UIScene* g_current_scene = nullptr;

void ui_scene_register(int state, const UIScene& scene) {
    g_scenes[state] = scene;
}

void ui_scene_set(int state) {
    if (g_current_scene && g_current_scene->shutdown)
        g_current_scene->shutdown();

    auto it = g_scenes.find(state);
    if (it != g_scenes.end()) {
        g_current_scene = &it->second;
        if (g_current_scene->init)
            g_current_scene->init();
    } else {
        g_current_scene = nullptr;
    }
}

void ui_scene_input(struct nk_context* ctx) {
    if (g_current_scene && g_current_scene->input)
        g_current_scene->input(ctx);
}

void ui_scene_render(struct nk_context* ctx) {
    if (g_current_scene && g_current_scene->render)
        g_current_scene->render(ctx);
}

void ui_scene_shutdown() {
    if (g_current_scene && g_current_scene->shutdown)
        g_current_scene->shutdown();
}
