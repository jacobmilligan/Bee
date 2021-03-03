/*
 *  Input.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/String.hpp"


namespace bee {


#define BEE_MAX_INPUT_DEVICES 64

enum class InputDeviceType
{
    none,
    keyboard,
    mouse,
    gamepad,
    other
};

enum class InputStateType
{
    dummy,
    flag,
    int32,
    float32,
};

union InputStateValue // NOLINT
{
    bool    flag;
    i32     int32;
    float   float32;
};

struct InputState // NOLINT
{
    i32             count { 0 };
    InputStateType  types[4] { InputStateType::dummy };
    InputStateValue values[4];
};

struct InputButton
{
    const char* name { nullptr };
    i32         id { -1 };
    BEE_PAD(4);
};

struct InputDevice
{
    const char*     name { nullptr };
    InputDeviceType type { InputDeviceType::none };
    BEE_PAD(4);

    i32 (*enumerate_buttons)(InputButton const** dst) { nullptr };

    const InputButton* (*find_button)(const char* name) { nullptr };

    i32 (*get_button_id)(const char* name) { nullptr };

    const InputButton* (*get_button)(const i32 id) { nullptr };

    const InputState* (*get_state)(const i32 button_id) { nullptr };

    const InputState* (*get_previous_state)(const i32 button_id) { nullptr };
};

#define BEE_INPUT_MODULE_NAME "BEE_INPUT_MODULE"

struct InputModule
{
    bool (*register_device)(const InputDevice* device) { nullptr };

    void (*unregister_device)(const InputDevice* device) { nullptr };

    i32 (*enumerate_devices)(InputDevice const** dst) { nullptr };

    bool (*find_device)(const StringView& name, InputDevice const** dst) { nullptr };

    const InputDevice* (*default_device)(const InputDeviceType type) { nullptr };
};


} // namespace bee