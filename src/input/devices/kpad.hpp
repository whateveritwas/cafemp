#ifndef KPAD_HPP
#define KPAD_HPP

#include "input/input_device.hpp"

void input_device_kpad_init();
void input_device_kpad_poll();
input_device *input_device_kpad_get();
void input_device_kpad_shutdown();

#endif
