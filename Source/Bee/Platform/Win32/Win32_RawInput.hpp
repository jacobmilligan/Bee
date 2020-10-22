/*
 *  Win32_RawInput.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Input/Input.hpp"
#include "Bee/Input/Keyboard.hpp"
#include "Bee/Input/Mouse.hpp"
#include "Bee/Core/Win32/MinWindows.h"

namespace bee {


struct RawInputKeyboard
{
    InputDevice device;
    InputButton buttons[Key::max];
    InputState  states[2][Key::max];
    i32         scancode_table[Key::max] { -1 };

    InputState* current_state { nullptr };
    InputState* last_state { nullptr };
};

struct RawInputMouse
{
    InputDevice device;
    InputButton buttons[MouseButton::max];
    InputState  states[2][MouseButton::max];

    InputState* current_state { nullptr };
    InputState* last_state { nullptr };
};

struct RawInput // NOLINT
{
    RawInputKeyboard    keyboard;
    RawInputMouse       mouse;
    LPBYTE              data_buffer[512];
};

void register_input_devices();

void unregister_input_devices();

bool register_raw_input(HWND hwnd, const DWORD flags);

void process_raw_input(LPARAM lParam);

void reset_raw_input();


} // namespace bee