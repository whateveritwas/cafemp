#ifndef UI_SCENE_H
#define UI_SCENE_H

#include <functional>

struct UIScene {
    std::function<void()> init;
    std::function<void(struct nk_context*)> input;
    std::function<void(struct nk_context*)> render;
    std::function<void()> shutdown;
};

void ui_scene_register(int state, const UIScene& scene);
void ui_scene_set(int state);
void ui_scene_input(struct nk_context* ctx);
void ui_scene_render(struct nk_context* ctx);
void ui_scene_shutdown();

#endif
