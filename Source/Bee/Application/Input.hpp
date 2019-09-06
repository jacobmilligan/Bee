/*
 *  Keycodes.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {


/**
 * Enumeration of all available modifiers to use for keyboard events.
 * A modifier is different to a Key code in that modifiers are considered as
 * a part of the same event as a Key, modifying them, whereas a Key event can only
 * have one keyboard key at any given time
 */
enum class ModifierKey : u32
{
    /**
     * The left/right shift key modifier
     */
    shift   = 1u << 0u,
    /**
     * The left/right control key modifier
     */
    control = 1u << 1u,
    /**
     * The left/right alt key modifier
     */
    alt     = 1u << 2u,
    /**
     * The left/right super key modifier - this is usually the windows key
     * on Windows or linux systems with a Windows keyboard or the option
     * key on macOS
     */
    super   = 1u << 3u
};

/**
 * Enumeration of all keycodes on a US standard keyboard. The keycodes map
 * to the ASCII standard with non-ASCII characters allocated to the 256+ range
 */
enum class Key : u32
{
    unknown = 0, // NULL
    space = 32,
    apostrophe = 39,
    comma = 44,
    minus,
    period,
    slash,
    num0,
    num1,
    num2,
    num3,
    num4,
    num5,
    num6,
    num7,
    num8,
    num9,
    semicolon = 59,
    equal = 61,
    A = 65,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    left_bracket,
    backslash,
    right_bracket,
    grave_accent = 96,
    /// Keycode to use for non-US mapped key
    international_1 = 161,
    /// Keycode to use for non-US mapped key
    international_2,
    escape = 256,
    enter,
    tab,
    backspace,
    insert,
    delete_key,
    right,
    left,
    down,
    up,
    page_up,
    page_down,
    home,
    end,
    caps_lock = 280,
    scroll_lock,
    num_lock,
    print_screen,
    pause,
    f1 = 290,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12,
    f13,
    f14,
    f15,
    f16,
    f17,
    f18,
    f19,
    f20,
    f21,
    f22,
    f23,
    f24,
    f25,
    keypad_0 = 320,
    keypad_1,
    keypad_2,
    keypad_3,
    keypad_4,
    keypad_5,
    keypad_6,
    keypad_7,
    keypad_8,
    keypad_9,
    keypad_decimal,
    keypad_divide,
    keypad_multiply,
    keypad_minus,
    keypad_plus,
    keypad_enter,
    keypad_equals,
    left_shift = 340,
    left_control,
    left_alt,
    left_super,
    right_shift,
    right_control,
    right_alt,
    right_super,
    menu,
    last = static_cast<u32>(menu)
};


enum class KeyState
{
    up = 0,
    down
};


constexpr const char* get_key_name(const Key key)
{
#define BEE_KEYNAME(k, str) case Key::k : return str;

    switch (key) {
        BEE_KEYNAME(unknown, "Unknown")
        BEE_KEYNAME(space, "Space")
        BEE_KEYNAME(apostrophe, "Apostrophe")
        BEE_KEYNAME(comma, "Comma")
        BEE_KEYNAME(minus, "Minus")
        BEE_KEYNAME(period, "Period")
        BEE_KEYNAME(slash, "Slash")
        BEE_KEYNAME(num0, "0")
        BEE_KEYNAME(num1, "1")
        BEE_KEYNAME(num2, "2")
        BEE_KEYNAME(num3, "3")
        BEE_KEYNAME(num4, "4")
        BEE_KEYNAME(num5, "5")
        BEE_KEYNAME(num6, "6")
        BEE_KEYNAME(num7, "7")
        BEE_KEYNAME(num8, "8")
        BEE_KEYNAME(num9, "9")
        BEE_KEYNAME(semicolon, "Semicolon")
        BEE_KEYNAME(equal, "Equal")
        BEE_KEYNAME(A, "A")
        BEE_KEYNAME(B, "B")
        BEE_KEYNAME(C, "C")
        BEE_KEYNAME(D, "D")
        BEE_KEYNAME(E, "E")
        BEE_KEYNAME(F, "F")
        BEE_KEYNAME(G, "G")
        BEE_KEYNAME(H, "H")
        BEE_KEYNAME(I, "I")
        BEE_KEYNAME(J, "J")
        BEE_KEYNAME(K, "K")
        BEE_KEYNAME(L, "L")
        BEE_KEYNAME(M, "M")
        BEE_KEYNAME(N, "N")
        BEE_KEYNAME(O, "O")
        BEE_KEYNAME(P, "P")
        BEE_KEYNAME(Q, "Q")
        BEE_KEYNAME(R, "R")
        BEE_KEYNAME(S, "S")
        BEE_KEYNAME(T, "T")
        BEE_KEYNAME(U, "U")
        BEE_KEYNAME(V, "V")
        BEE_KEYNAME(W, "W")
        BEE_KEYNAME(X, "X")
        BEE_KEYNAME(Y, "Y")
        BEE_KEYNAME(Z, "Z")
        BEE_KEYNAME(left_bracket, "LeftBracket")
        BEE_KEYNAME(backslash, "Backslash")
        BEE_KEYNAME(right_bracket, "RightBracket")
        BEE_KEYNAME(grave_accent, "GraveAccent")
        BEE_KEYNAME(international_1, "International1")
        BEE_KEYNAME(international_2, "International2")
        BEE_KEYNAME(escape, "Escape")
        BEE_KEYNAME(enter, "Enter")
        BEE_KEYNAME(tab, "Tab")
        BEE_KEYNAME(backspace, "Backspace")
        BEE_KEYNAME(insert, "Insert")
        BEE_KEYNAME(delete_key, "Delete")
        BEE_KEYNAME(right, "Right")
        BEE_KEYNAME(left, "Left")
        BEE_KEYNAME(down, "Down")
        BEE_KEYNAME(up, "Up")
        BEE_KEYNAME(page_up, "PageUp")
        BEE_KEYNAME(page_down, "PageDown")
        BEE_KEYNAME(home, "Home")
        BEE_KEYNAME(end, "End")
        BEE_KEYNAME(caps_lock, "CapsLock")
        BEE_KEYNAME(scroll_lock, "ScrollLock")
        BEE_KEYNAME(num_lock, "NumLock")
        BEE_KEYNAME(print_screen, "PrintScreen")
        BEE_KEYNAME(pause, "Pause")
        BEE_KEYNAME(f1, "F1")
        BEE_KEYNAME(f2, "F2")
        BEE_KEYNAME(f3, "F3")
        BEE_KEYNAME(f4, "F4")
        BEE_KEYNAME(f5, "F5")
        BEE_KEYNAME(f6, "F6")
        BEE_KEYNAME(f7, "F7")
        BEE_KEYNAME(f8, "F8")
        BEE_KEYNAME(f9, "F9")
        BEE_KEYNAME(f10, "F10")
        BEE_KEYNAME(f11, "F11")
        BEE_KEYNAME(f12, "F12")
        BEE_KEYNAME(f13, "F13")
        BEE_KEYNAME(f14, "F14")
        BEE_KEYNAME(f15, "F15")
        BEE_KEYNAME(f16, "F16")
        BEE_KEYNAME(f17, "F17")
        BEE_KEYNAME(f18, "F18")
        BEE_KEYNAME(f19, "F19")
        BEE_KEYNAME(f20, "F20")
        BEE_KEYNAME(f21, "F21")
        BEE_KEYNAME(f22, "F22")
        BEE_KEYNAME(f23, "F23")
        BEE_KEYNAME(f24, "F24")
        BEE_KEYNAME(f25, "F25")
        BEE_KEYNAME(keypad_0, "Keypad0")
        BEE_KEYNAME(keypad_1, "Keypad1")
        BEE_KEYNAME(keypad_2, "Keypad2")
        BEE_KEYNAME(keypad_3, "Keypad3")
        BEE_KEYNAME(keypad_4, "Keypad4")
        BEE_KEYNAME(keypad_5, "Keypad5")
        BEE_KEYNAME(keypad_6, "Keypad6")
        BEE_KEYNAME(keypad_7, "Keypad7")
        BEE_KEYNAME(keypad_8, "Keypad8")
        BEE_KEYNAME(keypad_9, "Keypad9")
        BEE_KEYNAME(keypad_decimal, "KeypadDecimal")
        BEE_KEYNAME(keypad_divide, "KeypadDivide")
        BEE_KEYNAME(keypad_multiply, "KeypadMultiply")
        BEE_KEYNAME(keypad_minus, "KeypadMinus")
        BEE_KEYNAME(keypad_plus, "KeypadPlus")
        BEE_KEYNAME(keypad_enter, "KeypadEnter")
        BEE_KEYNAME(keypad_equals, "KeypadEquals")
        BEE_KEYNAME(left_shift, "LeftShift")
        BEE_KEYNAME(left_control, "LeftControl")
        BEE_KEYNAME(left_alt, "LeftAlt")
        BEE_KEYNAME(left_super, "LeftSuper")
        BEE_KEYNAME(right_shift, "RightShift")
        BEE_KEYNAME(right_control, "RightControl")
        BEE_KEYNAME(right_alt, "RightAlt")
        BEE_KEYNAME(right_super, "RightSuper")
        BEE_KEYNAME(menu, "Menu")
    }

    return "";

#undef BEE_KEYNAME
}


struct InputBuffer
{
    static constexpr u32 key_max = static_cast<u32>(Key::last);

    /*
     * Keyboard - translation and state
     */
    static u32  vk_translation_table[key_max];
    KeyState    previous_keyboard[key_max];
    KeyState    current_keyboard[key_max];
};


BEE_API void input_buffer_init(InputBuffer* buffer);

BEE_API void input_buffer_frame(InputBuffer* buffer);

BEE_API bool key_down(const InputBuffer& buffer, Key key);

BEE_API bool key_up(const InputBuffer& buffer, Key key);

BEE_API bool key_typed(const InputBuffer& buffer, Key key);


} // namespace bee