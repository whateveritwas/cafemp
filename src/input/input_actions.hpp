#ifndef INPUT_ACTIONS_HPP
#define INPUT_ACTIONS_HPP

#include "input/input_device.hpp"

void input_init();
void input_poll();

bool input_pressed(buttons button);
bool input_held(buttons button);
bool input_released(buttons button);

input_device *input_get();
const input_device *input_get_previous();

void input_shutdown();

#endif
