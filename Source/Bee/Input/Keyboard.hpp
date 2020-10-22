/*
 *  Keyboard.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/EnumStruct.hpp"


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
    shift = 1u << 0u,
    /**
     * The left/right control key modifier
     */
    control = 1u << 1u,
    /**
     * The left/right alt key modifier
     */
    alt = 1u << 2u,
    /**
     * The left/right super key modifier - this is usually the windows key
     * on Windows or linux systems with a Windows keyboard or the option
     * key on macOS
     */
    super = 1u << 3u
};

/**
 * Enumeration of all keycodes on a US standard keyboard. The keycodes map
 * to the ASCII standard with non-ASCII characters allocated to the 256+ range
 */
struct Key
{
    enum Enum
    {
        unknown         = 0, // NULL
        keypad_0        = 1,
        keypad_1        = 2,
        keypad_2        = 3,
        keypad_3        = 4,
        keypad_4        = 5,
        keypad_5        = 6,
        keypad_6        = 7,
        keypad_7        = 8,
        keypad_8        = 9,
        keypad_9        = 10,
        keypad_decimal  = 11,
        keypad_divide   = 12,
        keypad_multiply = 13,
        keypad_minus    = 14,
        keypad_plus     = 15,
        keypad_enter    = 16,
        keypad_equals   = 17,
        end             = 18,
        scroll_lock     = 19,
        left_shift      = 20,
        left_control    = 21,
        left_alt        = 22,
        left_super      = 23,
        right_shift     = 24,
        right_control   = 25,
        right_alt       = 26,
        right_super     = 27,
        menu            = 28,
        oem_1           = 29,
        oem_2           = 30,
        oem_3           = 31,
        space           = 32,
        escape          = 33,
        enter           = 34,
        tab             = 35,
        backspace       = 36,
        insert          = 37,
        delete_key      = 38,
        apostrophe      = 39,
        right           = 40,
        left            = 41,
        down            = 42,
        up              = 43,
        comma           = 44,
        minus           = 45,
        period          = 46,
        slash           = 47,
        num0            = 48,
        num1            = 49,
        num2            = 50,
        num3            = 51,
        num4            = 52,
        num5            = 53,
        num6            = 54,
        num7            = 55,
        num8            = 56,
        num9            = 57,
        print_screen    = 58,
        semicolon       = 59,
        pause           = 60,
        equal           = 61,
        page_up         = 62,
        page_down       = 63,
        home            = 64,
        A               = 65,
        B               = 66,
        C               = 67,
        D               = 68,
        E               = 69,
        F               = 70,
        G               = 71,
        H               = 72,
        I               = 73,
        J               = 74,
        K               = 75,
        L               = 76,
        M               = 77,
        N               = 78,
        O               = 79,
        P               = 80,
        Q               = 81,
        R               = 82,
        S               = 83,
        T               = 84,
        U               = 85,
        V               = 86,
        W               = 87,
        X               = 88,
        Y               = 89,
        Z               = 90,
        left_bracket    = 91,
        backslash       = 92,
        right_bracket   = 93,
        caps_lock       = 94,
        num_lock        = 95,
        grave_accent    = 96,
        f1,
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
        count
    };

    static constexpr i32 max = Key::count;

    BEE_ENUM_STRUCT(Key)
};


} // namespace bee