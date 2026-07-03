#ifndef VPAD_HPP
#define VPAD_HPP

#include "input/input_device.hpp"

void input_device_vpad_init();
void input_device_vpad_poll();
input_device* input_device_vpad_get();
void input_device_vpad_shutdown();

#endif
