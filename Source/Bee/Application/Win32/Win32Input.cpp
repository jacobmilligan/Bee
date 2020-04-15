/*
 *  Win32Input.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/Input.hpp"

namespace bee {



void init_vk_translation_table()
{
    InputBuffer::vk_translation_table[0x41] = static_cast<u32>(Key::A);
    InputBuffer::vk_translation_table[0x42] = static_cast<u32>(Key::B);
    InputBuffer::vk_translation_table[0x43] = static_cast<u32>(Key::C);
    InputBuffer::vk_translation_table[0x44] = static_cast<u32>(Key::D);
    InputBuffer::vk_translation_table[0x45] = static_cast<u32>(Key::E);
    InputBuffer::vk_translation_table[0x46] = static_cast<u32>(Key::F);
    InputBuffer::vk_translation_table[0x47] = static_cast<u32>(Key::G);
    InputBuffer::vk_translation_table[0x48] = static_cast<u32>(Key::H);
    InputBuffer::vk_translation_table[0x49] = static_cast<u32>(Key::I);
    InputBuffer::vk_translation_table[0x4A] = static_cast<u32>(Key::J);
    InputBuffer::vk_translation_table[0x4B] = static_cast<u32>(Key::K);
    InputBuffer::vk_translation_table[0x4C] = static_cast<u32>(Key::L);
    InputBuffer::vk_translation_table[0x4D] = static_cast<u32>(Key::M);
    InputBuffer::vk_translation_table[0x4E] = static_cast<u32>(Key::N);
    InputBuffer::vk_translation_table[0x4F] = static_cast<u32>(Key::O);
    InputBuffer::vk_translation_table[0x50] = static_cast<u32>(Key::P);
    InputBuffer::vk_translation_table[0x51] = static_cast<u32>(Key::Q);
    InputBuffer::vk_translation_table[0x52] = static_cast<u32>(Key::R);
    InputBuffer::vk_translation_table[0x53] = static_cast<u32>(Key::S);
    InputBuffer::vk_translation_table[0x54] = static_cast<u32>(Key::T);
    InputBuffer::vk_translation_table[0x55] = static_cast<u32>(Key::U);
    InputBuffer::vk_translation_table[0x56] = static_cast<u32>(Key::V);
    InputBuffer::vk_translation_table[0x57] = static_cast<u32>(Key::W);
    InputBuffer::vk_translation_table[0x58] = static_cast<u32>(Key::X);
    InputBuffer::vk_translation_table[0x59] = static_cast<u32>(Key::Y);
    InputBuffer::vk_translation_table[0x5A] = static_cast<u32>(Key::Z);
    InputBuffer::vk_translation_table[0x30] = static_cast<u32>(Key::num0);
    InputBuffer::vk_translation_table[0x31] = static_cast<u32>(Key::num1);
    InputBuffer::vk_translation_table[0x32] = static_cast<u32>(Key::num2);
    InputBuffer::vk_translation_table[0x33] = static_cast<u32>(Key::num3);
    InputBuffer::vk_translation_table[0x34] = static_cast<u32>(Key::num4);
    InputBuffer::vk_translation_table[0x35] = static_cast<u32>(Key::num5);
    InputBuffer::vk_translation_table[0x36] = static_cast<u32>(Key::num6);
    InputBuffer::vk_translation_table[0x37] = static_cast<u32>(Key::num7);
    InputBuffer::vk_translation_table[0x38] = static_cast<u32>(Key::num8);
    InputBuffer::vk_translation_table[0x39] = static_cast<u32>(Key::num9);

    InputBuffer::vk_translation_table[0xBB] = static_cast<u32>(Key::equal);
    InputBuffer::vk_translation_table[0xBD] = static_cast<u32>(Key::minus);
    InputBuffer::vk_translation_table[0xDD] = static_cast<u32>(Key::right_bracket);
    InputBuffer::vk_translation_table[0xDB] = static_cast<u32>(Key::left_bracket);
    InputBuffer::vk_translation_table[0xDE] = static_cast<u32>(Key::apostrophe);
    InputBuffer::vk_translation_table[0xBA] = static_cast<u32>(Key::semicolon);
    InputBuffer::vk_translation_table[0xDB] = static_cast<u32>(Key::backslash);
    InputBuffer::vk_translation_table[0xBC] = static_cast<u32>(Key::comma);
    InputBuffer::vk_translation_table[0xBF] = static_cast<u32>(Key::slash);
    InputBuffer::vk_translation_table[0xBE] = static_cast<u32>(Key::period);
    InputBuffer::vk_translation_table[0xC0] = static_cast<u32>(Key::grave_accent);
    InputBuffer::vk_translation_table[0x90] = static_cast<u32>(Key::num_lock);
    InputBuffer::vk_translation_table[0x6E] = static_cast<u32>(Key::keypad_decimal);
    InputBuffer::vk_translation_table[0x6A] = static_cast<u32>(Key::keypad_multiply);
    InputBuffer::vk_translation_table[0x6B] = static_cast<u32>(Key::keypad_plus);
    InputBuffer::vk_translation_table[0x6F] = static_cast<u32>(Key::keypad_divide);
    // InputBuffer::vk_translation_table[0x] = static_cast<u32>(Key::keypad_enter); windows defines keypad enter as the same as return. Will need to check for this using scancodes
    InputBuffer::vk_translation_table[0x6D] = static_cast<u32>(Key::keypad_minus);
    // InputBuffer::vk_translation_table[0x51] = static_cast<u32>(Key::keypad_equals); No such definition on windows
    InputBuffer::vk_translation_table[0x60] = static_cast<u32>(Key::keypad_0);
    InputBuffer::vk_translation_table[0x61] = static_cast<u32>(Key::keypad_1);
    InputBuffer::vk_translation_table[0x62] = static_cast<u32>(Key::keypad_2);
    InputBuffer::vk_translation_table[0x63] = static_cast<u32>(Key::keypad_3);
    InputBuffer::vk_translation_table[0x64] = static_cast<u32>(Key::keypad_4);
    InputBuffer::vk_translation_table[0x65] = static_cast<u32>(Key::keypad_5);
    InputBuffer::vk_translation_table[0x66] = static_cast<u32>(Key::keypad_6);
    InputBuffer::vk_translation_table[0x67] = static_cast<u32>(Key::keypad_7);
    InputBuffer::vk_translation_table[0x68] = static_cast<u32>(Key::keypad_8);
    InputBuffer::vk_translation_table[0x69] = static_cast<u32>(Key::keypad_9);

    // Keys independent of keyboard layout
    InputBuffer::vk_translation_table[0x0D] = static_cast<u32>(Key::enter);
    InputBuffer::vk_translation_table[0x09] = static_cast<u32>(Key::tab);
    InputBuffer::vk_translation_table[0x20] = static_cast<u32>(Key::space);
    InputBuffer::vk_translation_table[0x2E] = static_cast<u32>(Key::delete_key);
    InputBuffer::vk_translation_table[0x1B] = static_cast<u32>(Key::escape);
    InputBuffer::vk_translation_table[0x5B] = static_cast<u32>(Key::left_super); // left windows key
    InputBuffer::vk_translation_table[0xA0] = static_cast<u32>(Key::left_shift);
    InputBuffer::vk_translation_table[0x14] = static_cast<u32>(Key::caps_lock);
    InputBuffer::vk_translation_table[0xA4] = static_cast<u32>(Key::left_alt);
    InputBuffer::vk_translation_table[0xA2] = static_cast<u32>(Key::left_control);
    InputBuffer::vk_translation_table[0x5C] = static_cast<u32>(Key::right_super);
    InputBuffer::vk_translation_table[0xA1] = static_cast<u32>(Key::right_shift);
    InputBuffer::vk_translation_table[0xA5] = static_cast<u32>(Key::right_alt);
    InputBuffer::vk_translation_table[0xA3] = static_cast<u32>(Key::right_control);

    InputBuffer::vk_translation_table[0x70] = static_cast<u32>(Key::f1);
    InputBuffer::vk_translation_table[0x71] = static_cast<u32>(Key::f2);
    InputBuffer::vk_translation_table[0x72] = static_cast<u32>(Key::f3);
    InputBuffer::vk_translation_table[0x73] = static_cast<u32>(Key::f4);
    InputBuffer::vk_translation_table[0x74] = static_cast<u32>(Key::f5);
    InputBuffer::vk_translation_table[0x75] = static_cast<u32>(Key::f6);
    InputBuffer::vk_translation_table[0x76] = static_cast<u32>(Key::f7);
    InputBuffer::vk_translation_table[0x77] = static_cast<u32>(Key::f8);
    InputBuffer::vk_translation_table[0x78] = static_cast<u32>(Key::f9);
    InputBuffer::vk_translation_table[0x79] = static_cast<u32>(Key::f10);
    InputBuffer::vk_translation_table[0x7A] = static_cast<u32>(Key::f11);
    InputBuffer::vk_translation_table[0x7B] = static_cast<u32>(Key::f12);
    InputBuffer::vk_translation_table[0x7C] = static_cast<u32>(Key::f13);
    InputBuffer::vk_translation_table[0x7D] = static_cast<u32>(Key::f14);
    InputBuffer::vk_translation_table[0x7E] = static_cast<u32>(Key::f15);
    InputBuffer::vk_translation_table[0x7F] = static_cast<u32>(Key::f16);
    InputBuffer::vk_translation_table[0x80] = static_cast<u32>(Key::f17);
    InputBuffer::vk_translation_table[0x81] = static_cast<u32>(Key::f18);
    InputBuffer::vk_translation_table[0x82] = static_cast<u32>(Key::f19);
    InputBuffer::vk_translation_table[0x83] = static_cast<u32>(Key::f20);

    InputBuffer::vk_translation_table[0x2D] = static_cast<u32>(Key::insert);
    InputBuffer::vk_translation_table[0x24] = static_cast<u32>(Key::home);
    InputBuffer::vk_translation_table[0x21] = static_cast<u32>(Key::page_up);
    InputBuffer::vk_translation_table[0x2E] = static_cast<u32>(Key::delete_key);
    InputBuffer::vk_translation_table[0x23] = static_cast<u32>(Key::end);
    InputBuffer::vk_translation_table[0x22] = static_cast<u32>(Key::page_down);

    InputBuffer::vk_translation_table[0x25] = static_cast<u32>(Key::left);
    InputBuffer::vk_translation_table[0x27] = static_cast<u32>(Key::right);
    InputBuffer::vk_translation_table[0x28] = static_cast<u32>(Key::down);
    InputBuffer::vk_translation_table[0x26] = static_cast<u32>(Key::up);
}


} // namespace bee