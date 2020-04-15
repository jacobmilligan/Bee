/*
 *  Input.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/Input.hpp"

#include <string.h> // memcpy & memset

namespace bee {


u32 InputBuffer::vk_translation_table[InputBuffer::key_max];


void init_vk_translation_table();


void input_buffer_init(InputBuffer* buffer)
{
    static bool needs_vk_table_init = true;
    if (needs_vk_table_init)
    {
        init_vk_translation_table();
    }

    memset(buffer->previous_keyboard, 0, InputBuffer::key_max * sizeof(KeyState));
    memset(buffer->current_keyboard, 0, InputBuffer::key_max * sizeof(KeyState));
}

void input_buffer_frame(InputBuffer* buffer)
{
    memcpy(buffer->previous_keyboard, buffer->current_keyboard, InputBuffer::key_max * sizeof(KeyState));
    memset(buffer->current_keyboard, 0, InputBuffer::key_max * sizeof(KeyState));
}

bool key_down(const InputBuffer& buffer, Key key)
{
    return buffer.current_keyboard[static_cast<u32>(key)] == KeyState::down;
}

bool key_up(const InputBuffer& buffer, Key key)
{
    return buffer.current_keyboard[static_cast<u32>(key)] == KeyState::up;
}

bool key_typed(const InputBuffer& buffer, Key key)
{
    const auto key_index = static_cast<u32>(key);
    return buffer.current_keyboard[key_index] == KeyState::down && buffer.previous_keyboard[key_index] == KeyState::up;
}


} // namespace bee