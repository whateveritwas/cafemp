#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <string>

// Opens the on-screen keyboard with optional initial text
void keyboard_open(const char* initial = "");

// Returns true if the keyboard overlay is currently open
bool keyboard_is_open();

// Renders and updates the keyboard (call inside your UI loop)
void keyboard_update(struct nk_context* ctx);

// If the keyboard has been closed, returns the last result
// Returns empty string if cancelled or not yet confirmed
std::string keyboard_get_result();

#endif
