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
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Time.hpp"


namespace bee {


struct RawInputKeyboard
{
    InputDevice                     device;
    InputButton                     buttons[Key::max];
    InputState                      states[2][Key::max];
    i32                             scancode_table[Key::max] { -1 };

    InputState*                     current_state { nullptr };
    InputState*                     last_state { nullptr };
    StaticArray<InputEvent, 1024>   events;
};

struct RawInputMouse
{
    InputDevice                     device;
    InputButton                     buttons[MouseButton::max];
    InputState                      states[2][MouseButton::max];

    InputState*                     current_state { nullptr };
    InputState*                     last_state { nullptr };
    StaticArray<InputEvent, 1024>   events;
};

struct RawInput // NOLINT
{
    RawInputKeyboard    keyboard;
    RawInputMouse       mouse;
    LPBYTE              data_buffer[512];
};

template <typename T>
void enqueue_text_event(T* device, const u32 codepoint)
{
    device->events.emplace_back();
    auto& event = device->events.back();
    event.type = InputEventType::text;
    event.device = &device->device;
    event.timestamp = time::now();
    event.codepoint = codepoint;
}

template <typename T>
void enqueue_state_event(T* device, const i32 button_id)
{
    device->events.emplace_back();
    auto& event = device->events.back();
    event.type = InputEventType::state_change;
    event.device = &device->device;
    event.button_id = button_id;
    event.timestamp = time::now();
    memcpy(&event.state, &device->current_state[button_id], sizeof(InputState));
}

void register_input_devices();

void unregister_input_devices();

bool register_raw_input(HWND hwnd, const DWORD flags);

void process_raw_input(LPARAM lParam);

void reset_raw_input();


} // namespace bee