#ifndef WPAD_HPP
#define WPAD_HPP

#include "input/input_device.hpp"

void input_device_wpad_init();
void input_device_wpad_poll();
input_device *input_device_wpad_get();
void input_device_wpad_shutdown();

#endif
