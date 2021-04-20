/*
 *  Win32Input.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Enum.hpp"

#define BEE_MINWINDOWS_ENABLE_USER
#define BEE_MINWINDOWS_ENABLE_MSG
#define BEE_MINWINDOWS_ENABLE_WINDOWING
#include "Bee/Platform/Win32/Win32_RawInput.hpp"

#include <hidusage.h>

namespace bee {


RawInput* g_raw_input = nullptr;

/*
 *************
 *
 * Mouse
 *
 *************
 */
i32 mouse_enumerate_buttons(InputButton const** dst)
{
    *dst = g_raw_input->mouse.buttons;
    return static_array_length(g_raw_input->mouse.buttons);
}

i32 mouse_get_button_id(const char* name)
{
    return find_index_if(g_raw_input->mouse.buttons, [&](const InputButton& btn)
    {
        return str::compare(btn.name, name) == 0;
    });
}

const InputButton* mouse_find_button(const char* name)
{
    const i32 id = mouse_get_button_id(name);
    return id >= 0 ? &g_raw_input->mouse.buttons[id] : nullptr;
}

const InputButton* mouse_get_button(const i32 id)
{
    BEE_ASSERT(id < static_array_length(g_raw_input->mouse.buttons));
    return &g_raw_input->mouse.buttons[id];
}

const InputState* mouse_get_state(const i32 button_id)
{
    BEE_ASSERT(button_id < static_array_length(g_raw_input->mouse.states[0]));
    return &g_raw_input->mouse.current_state[button_id];
}

const InputState* mouse_get_previous_state(const i32 button_id)
{
    BEE_ASSERT(button_id < static_array_length(g_raw_input->mouse.states[0]));
    return &g_raw_input->mouse.last_state[button_id];
}

static void init_mouse_button(RawInputMouse* mouse, const MouseButton& btn, const InputStateType type)
{
    auto& current = mouse->states[0][btn];
    memset(current.types, static_cast<i32>(type), sizeof(InputStateType) * static_array_length(current.types));
    memset(current.values, 0, sizeof(InputStateValue) * static_array_length(current.values));
}

static void init_mouse(RawInputMouse* mouse)
{
    init_mouse_button(mouse, MouseButton::button_1, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::button_2, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::button_3, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::button_4, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::button_5, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::button_6, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::button_7, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::button_8, InputStateType::flag);
    init_mouse_button(mouse, MouseButton::wheel,    InputStateType::float32); // horizontal/vertical wheel
    init_mouse_button(mouse, MouseButton::delta,    InputStateType::float32); // x/y
    init_mouse_button(mouse, MouseButton::position, InputStateType::float32); // x/y

    memcpy(&mouse->states[1], &mouse->states[0], sizeof(InputState) * static_array_length(mouse->states[0]));
    mouse->current_state = mouse->states[0];
    mouse->last_state = mouse->states[1];
}

static void update_mouse_flag(RawInputMouse* mouse, const USHORT flags, const USHORT test_flag, const MouseButton& btn, const bool flag)
{
    if ((flags & test_flag) != 0)
    {
        mouse->current_state[btn].values[0].flag = flag;
        enqueue_state_event(mouse, btn);
    }
}

static void process_mouse(const RAWMOUSE& state, RawInputMouse* mouse)
{
    static float x = 0.0f;
    if (state.usFlags == MOUSE_MOVE_RELATIVE)
    {
        auto& delta = mouse->current_state[MouseButton::delta].values;
        delta[0].float32 = static_cast<float>(state.lLastX);
        delta[1].float32 = static_cast<float>(-state.lLastY);
        enqueue_state_event(mouse, MouseButton::delta);

        auto& pos = mouse->current_state[MouseButton::position].values;
        const DWORD msg_pos = GetMessagePos();
        pos[0].float32 = static_cast<float>((msg_pos & 0x0000ffff));
        pos[1].float32 = static_cast<float>((msg_pos & 0xffff0000) >> 16);
        enqueue_state_event(mouse, MouseButton::position);
    }

    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_1_DOWN, MouseButton::button_1, true);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_1_UP,   MouseButton::button_1, false);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_2_DOWN, MouseButton::button_2, true);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_2_UP,   MouseButton::button_2, false);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_3_DOWN, MouseButton::button_3, true);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_3_UP,   MouseButton::button_3, false);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_4_DOWN, MouseButton::button_4, true);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_4_UP,   MouseButton::button_4, false);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_5_DOWN, MouseButton::button_5, true);
    update_mouse_flag(mouse, state.usButtonFlags, RI_MOUSE_BUTTON_5_UP,   MouseButton::button_5, false);

    const bool vertical_wheel = (state.usButtonFlags & RI_MOUSE_WHEEL) != 0;
    const bool horizontal_wheel = (state.usButtonFlags & RI_MOUSE_HWHEEL) != 0;

    if (vertical_wheel)
    {
        mouse->current_state[MouseButton::wheel].values[0].float32 = static_cast<float>(state.usButtonData);
    }
    if (horizontal_wheel)
    {
        mouse->current_state[MouseButton::wheel].values[1].float32 = static_cast<float>(state.usButtonData);
    }
    if (vertical_wheel || horizontal_wheel)
    {
        enqueue_state_event(mouse, MouseButton::wheel);
    }
}

static const Span<const InputEvent> mouse_get_events()
{
    return g_raw_input->mouse.events.const_span();
}


/*
 *************
 *
 * Keyboard
 *
 *************
 */
i32 keyboard_enumerate_buttons(InputButton const** dst)
{
    *dst = g_raw_input->keyboard.buttons;
    return static_array_length(g_raw_input->keyboard.buttons);
}

i32 keyboard_get_button_id(const char* name)
{
    return find_index_if(g_raw_input->keyboard.buttons, [&](const InputButton& btn)
    {
        return str::compare(btn.name, name) == 0;
    });
}

const InputButton* keyboard_get_button(const i32 id)
{
    return id >= 0 ? &g_raw_input->keyboard.buttons[id] : nullptr;
}

const InputButton* keyboard_find_button(const char* name)
{
    const i32 id = keyboard_get_button_id(name);
    return keyboard_get_button(id);
}

const InputState* keyboard_get_state(const i32 button_id)
{
    BEE_ASSERT(button_id < static_array_length(g_raw_input->keyboard.states[0]));
    return &g_raw_input->keyboard.current_state[button_id];
}

const InputState* keyboard_get_previous_state(const i32 button_id)
{
    BEE_ASSERT(button_id < static_array_length(g_raw_input->keyboard.states[0]));
    return &g_raw_input->keyboard.last_state[button_id];
}

static void init_key(RawInputKeyboard* keyboard, const Key& key, const i32 scancode, const char* name)
{
    keyboard->buttons[key].name = name;
    keyboard->buttons[key].id = key;

    keyboard->states[0][key].types[0] = InputStateType::flag;
    keyboard->states[0][key].values[0].flag = false;

    keyboard->scancode_table[scancode] = key;
}

static void init_keyboard(RawInputKeyboard* keyboard)
{
    static_assert(Key::max >= 0x76, "Raw keyboard scancode table is too small");

    init_key(keyboard, Key::unknown,            0x0,    "UNKNOWN");
    init_key(keyboard, Key::keypad_0,           0x52,   "KEYPAD_0");
    init_key(keyboard, Key::keypad_1,           0x4F,   "KEYPAD_1");
    init_key(keyboard, Key::keypad_2,           0x50,   "KEYPAD_2");
    init_key(keyboard, Key::keypad_3,           0x51,   "KEYPAD_3");
    init_key(keyboard, Key::keypad_4,           0x4B,   "KEYPAD_4");
    init_key(keyboard, Key::keypad_5,           0x4C,   "KEYPAD_5");
    init_key(keyboard, Key::keypad_6,           0x4D,   "KEYPAD_6");
    init_key(keyboard, Key::keypad_7,           0x47,   "KEYPAD_7");
    init_key(keyboard, Key::keypad_8,           0x48,   "KEYPAD_8");
    init_key(keyboard, Key::keypad_9,           0x49,   "KEYPAD_9");
    init_key(keyboard, Key::keypad_decimal,     0x53,   "KEYPAD_DECIMAL");
    init_key(keyboard, Key::keypad_divide,      0x35,   "KEYPAD_DIVIDE");
    init_key(keyboard, Key::keypad_multiply,    0x37,   "KEYPAD_MULTIPLY");
    init_key(keyboard, Key::keypad_minus,       0x4A,   "KEYPAD_MINUS");
    init_key(keyboard, Key::keypad_plus,        0x4E,   "KEYPAD_PLUS");
    init_key(keyboard, Key::keypad_enter,       0x1C,   "KEYPAD_ENTER");
    init_key(keyboard, Key::keypad_equals,      0x59,   "KEYPAD_EQUALS");
    init_key(keyboard, Key::end,                0x4F,   "END");
    init_key(keyboard, Key::scroll_lock,        0x46,   "SCROLL_LOCK");
    init_key(keyboard, Key::left_shift,         0x2A,   "LEFT_SHIFT");
    init_key(keyboard, Key::left_control,       0x1D,   "LEFT_CONTROL");
    init_key(keyboard, Key::left_alt,           0x38,   "LEFT_ALT");
    init_key(keyboard, Key::left_super,         0x5B,   "LEFT_SUPER");
    init_key(keyboard, Key::right_shift,        0x36,   "RIGHT_SHIFT");
    init_key(keyboard, Key::right_control,      0x1D,   "RIGHT_CONTROL");
    init_key(keyboard, Key::right_alt,          0x38,   "RIGHT_ALT");
    init_key(keyboard, Key::right_super,        0x5C,   "RIGHT_SUPER");
    init_key(keyboard, Key::menu,               0x5D,   "MENU");
    init_key(keyboard, Key::oem_1,              0x56,   "OEM_1");
    init_key(keyboard, Key::oem_2,              0x73,   "OEM_2");
    init_key(keyboard, Key::oem_3,              0x70,   "OEM_3");
    init_key(keyboard, Key::space,              0x39,   "SPACE");
    init_key(keyboard, Key::escape,             0x01,   "ESCAPE");
    init_key(keyboard, Key::enter,              0x1C,   "ENTER");
    init_key(keyboard, Key::tab,                0x0F,   "TAB");
    init_key(keyboard, Key::backspace,          0x0E,   "BACKSPACE");
    init_key(keyboard, Key::insert,             0x52,   "INSERT");
    init_key(keyboard, Key::delete_key,         0x53,   "DELETE_KEY");
    init_key(keyboard, Key::apostrophe,         0x28,   "APOSTROPHE");
    init_key(keyboard, Key::right,              0x4D,   "RIGHT");
    init_key(keyboard, Key::left,               0x4B,   "LEFT");
    init_key(keyboard, Key::down,               0x50,   "DOWN");
    init_key(keyboard, Key::up,                 0x48,   "UP");
    init_key(keyboard, Key::comma,              0x33,   "COMMA");
    init_key(keyboard, Key::minus,              0x0C,   "MINUS");
    init_key(keyboard, Key::period,             0x34,   "PERIOD");
    init_key(keyboard, Key::slash,              0x35,   "SLASH");
    init_key(keyboard, Key::num0,               0x0B,   "NUM0");
    init_key(keyboard, Key::num1,               0x02,   "NUM1");
    init_key(keyboard, Key::num2,               0x03,   "NUM2");
    init_key(keyboard, Key::num3,               0x04,   "NUM3");
    init_key(keyboard, Key::num4,               0x05,   "NUM4");
    init_key(keyboard, Key::num5,               0x06,   "NUM5");
    init_key(keyboard, Key::num6,               0x07,   "NUM6");
    init_key(keyboard, Key::num7,               0x08,   "NUM7");
    init_key(keyboard, Key::num8,               0x09,   "NUM8");
    init_key(keyboard, Key::num9,               0x0A,   "NUM9");
    init_key(keyboard, Key::print_screen,       0x37,   "PRINT_SCREEN");
    init_key(keyboard, Key::semicolon,          0x27,   "SEMICOLON");
    init_key(keyboard, Key::pause,              0x45,   "PAUSE");
    init_key(keyboard, Key::equal,              0x0D,   "EQUAL");
    init_key(keyboard, Key::page_up,            0x49,   "PAGE_UP");
    init_key(keyboard, Key::page_down,          0x51,   "PAGE_DOWN");
    init_key(keyboard, Key::home,               0x47,   "HOME");
    init_key(keyboard, Key::A,                  0x1E,   "A");
    init_key(keyboard, Key::B,                  0x30,   "B");
    init_key(keyboard, Key::C,                  0x2E,   "C");
    init_key(keyboard, Key::D,                  0x20,   "D");
    init_key(keyboard, Key::E,                  0x12,   "E");
    init_key(keyboard, Key::F,                  0x21,   "F");
    init_key(keyboard, Key::G,                  0x22,   "G");
    init_key(keyboard, Key::H,                  0x23,   "H");
    init_key(keyboard, Key::I,                  0x17,   "I");
    init_key(keyboard, Key::J,                  0x24,   "J");
    init_key(keyboard, Key::K,                  0x25,   "K");
    init_key(keyboard, Key::L,                  0x26,   "L");
    init_key(keyboard, Key::M,                  0x32,   "M");
    init_key(keyboard, Key::N,                  0x31,   "N");
    init_key(keyboard, Key::O,                  0x18,   "O");
    init_key(keyboard, Key::P,                  0x19,   "P");
    init_key(keyboard, Key::Q,                  0x10,   "Q");
    init_key(keyboard, Key::R,                  0x13,   "R");
    init_key(keyboard, Key::S,                  0x1F,   "S");
    init_key(keyboard, Key::T,                  0x14,   "T");
    init_key(keyboard, Key::U,                  0x16,   "U");
    init_key(keyboard, Key::V,                  0x2F,   "V");
    init_key(keyboard, Key::W,                  0x11,   "W");
    init_key(keyboard, Key::X,                  0x2D,   "X");
    init_key(keyboard, Key::Y,                  0x15,   "Y");
    init_key(keyboard, Key::Z,                  0x2C,   "Z");
    init_key(keyboard, Key::left_bracket,       0x1A,   "LEFT_BRACKET");
    init_key(keyboard, Key::backslash,          0x2B,   "BACKSLASH");
    init_key(keyboard, Key::right_bracket,      0x1B,   "RIGHT_BRACKET");
    init_key(keyboard, Key::caps_lock,          0x3A,   "CAPS_LOCK");
    init_key(keyboard, Key::num_lock,           0x45,   "NUM_LOCK");
    init_key(keyboard, Key::grave_accent,       0x29,   "GRAVE_ACCENT");
    init_key(keyboard, Key::f1,                 0x3B,   "F1");
    init_key(keyboard, Key::f2,                 0x3C,   "F2");
    init_key(keyboard, Key::f3,                 0x3D,   "F3");
    init_key(keyboard, Key::f4,                 0x3E,   "F4");
    init_key(keyboard, Key::f5,                 0x3F,   "F5");
    init_key(keyboard, Key::f6,                 0x40,   "F6");
    init_key(keyboard, Key::f7,                 0x41,   "F7");
    init_key(keyboard, Key::f8,                 0x42,   "F8");
    init_key(keyboard, Key::f9,                 0x43,   "F9");
    init_key(keyboard, Key::f10,                0x44,   "F10");
    init_key(keyboard, Key::f11,                0x57,   "F11");
    init_key(keyboard, Key::f12,                0x58,   "F12");
    init_key(keyboard, Key::f13,                0x64,   "F13");
    init_key(keyboard, Key::f14,                0x65,   "F14");
    init_key(keyboard, Key::f15,                0x66,   "F15");
    init_key(keyboard, Key::f16,                0x67,   "F16");
    init_key(keyboard, Key::f17,                0x68,   "F17");
    init_key(keyboard, Key::f18,                0x69,   "F18");
    init_key(keyboard, Key::f19,                0x6A,   "F19");
    init_key(keyboard, Key::f20,                0x6B,   "F20");
    init_key(keyboard, Key::f21,                0x6C,   "F21");
    init_key(keyboard, Key::f22,                0x6D,   "F22");
    init_key(keyboard, Key::f23,                0x6E,   "F23");
    init_key(keyboard, Key::f24,                0x6F,   "F24");
    init_key(keyboard, Key::f25,                0x76,   "F25");

    // init both current and last states to initial
    memcpy(&keyboard->states[1], &keyboard->states[0], sizeof(InputState) * static_array_length(keyboard->states[0]));
    keyboard->current_state = keyboard->states[0];
    keyboard->last_state = keyboard->states[1];
}

void process_key(const RAWKEYBOARD& state, RawInputKeyboard* keyboard)
{
    const i32 index = keyboard->scancode_table[state.MakeCode];
    keyboard->current_state[index].values[0].flag = (state.Flags & RI_KEY_BREAK) == 0;
    enqueue_state_event(keyboard, index);
}

const Span<const InputEvent> keyboard_get_events()
{
    auto* keyboard = &g_raw_input->keyboard;
    return keyboard->events.const_span();
}


/*
 *********************************
 *
 * Platform-internal functions
 *
 *********************************
 */
void register_input_devices()
{
    auto* input = static_cast<InputModule*>(get_module(BEE_INPUT_MODULE_NAME));

    auto& mouse = g_raw_input->mouse;
    mouse.device.name = "RawInput_Mouse";
    mouse.device.type = InputDeviceType::mouse;
    mouse.device.enumerate_buttons = mouse_enumerate_buttons;
    mouse.device.find_button = mouse_find_button;
    mouse.device.get_button_id = mouse_get_button_id;
    mouse.device.get_button = mouse_get_button;
    mouse.device.get_state = mouse_get_state;
    mouse.device.get_previous_state = mouse_get_previous_state;
    mouse.device.get_events = mouse_get_events;

    auto& keyboard = g_raw_input->keyboard;
    keyboard.device.name = "RawInput_Keyboard";
    keyboard.device.type = InputDeviceType::keyboard;
    keyboard.device.enumerate_buttons = keyboard_enumerate_buttons;
    keyboard.device.find_button = keyboard_find_button;
    keyboard.device.get_button_id = keyboard_get_button_id;
    keyboard.device.get_button = keyboard_get_button;
    keyboard.device.get_state = keyboard_get_state;
    keyboard.device.get_previous_state = keyboard_get_previous_state;
    keyboard.device.get_events = keyboard_get_events;

    init_keyboard(&keyboard);
    init_mouse(&mouse);

    input->register_device(&keyboard.device);
    input->register_device(&mouse.device);
}

void unregister_input_devices()
{
    auto* input = static_cast<InputModule*>(get_module(BEE_INPUT_MODULE_NAME));

    input->unregister_device(&g_raw_input->mouse.device);
    input->unregister_device(&g_raw_input->keyboard.device);
}

bool register_raw_input(HWND hwnd, const DWORD flags)
{
    RAWINPUTDEVICE rid[2];

    // Keyboard
    rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    rid[0].dwFlags = 0;
    rid[0].hwndTarget = hwnd;

    // Mouse
    rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid[1].usUsage = HID_USAGE_GENERIC_MOUSE;
    rid[1].dwFlags = 0;
    rid[1].hwndTarget = hwnd;

    return RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)) != FALSE;
}

void process_raw_input(LPARAM lParam)
{
    UINT size = sizeof(g_raw_input->data_buffer);
    const UINT result = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, g_raw_input->data_buffer, &size, sizeof(RAWINPUTHEADER));

    BEE_ASSERT(GetLastError() != ERROR_INSUFFICIENT_BUFFER);

    if (result == (UINT)-1)
    {
        log_error("RawInput processing error: %s", win32_get_last_error_string());
        return;
    }

    auto* raw_state = reinterpret_cast<RAWINPUT*>(g_raw_input->data_buffer);

    switch (raw_state->header.dwType)
    {
        case RIM_TYPEKEYBOARD:
        {
            process_key(raw_state->data.keyboard, &g_raw_input->keyboard);
            break;
        }

        case RIM_TYPEMOUSE:
        {
            process_mouse(raw_state->data.mouse, &g_raw_input->mouse);
            break;
        }
    }
}

void reset_raw_input()
{
    memcpy(g_raw_input->keyboard.last_state, g_raw_input->keyboard.current_state, sizeof(InputState) * static_array_length(g_raw_input->keyboard.states[0]));
    memcpy(g_raw_input->mouse.last_state, g_raw_input->mouse.current_state, sizeof(InputState) * static_array_length(g_raw_input->mouse.states[0]));

    // reset the mouse delta for this frame
    memset(g_raw_input->mouse.current_state[MouseButton::delta].values, 0, sizeof(InputStateValue) * 4);

    // clear event queues
    g_raw_input->keyboard.events.size = 0;
    g_raw_input->mouse.events.size = 0;
}

} // namespace bee

