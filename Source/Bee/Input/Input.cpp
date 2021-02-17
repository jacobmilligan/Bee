/*
 *  Input.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Input/Input.hpp"
#include "Bee/Core/Plugin.hpp"


namespace bee {


struct Input // NOLINT
{
    i32                 device_count { 0 };
    const InputDevice*  devices[BEE_MAX_INPUT_DEVICES];
};

static Input* g_input = nullptr;


bool register_device(const InputDevice* device)
{
    if (g_input->device_count >= BEE_MAX_INPUT_DEVICES)
    {
        log_error("Failed to register InputDevice %s: cannot register more than %d input devices", device->name, BEE_MAX_INPUT_DEVICES);
        return false;
    }

    g_input->devices[g_input->device_count] = device;
    ++g_input->device_count;
    return true;
}

void unregister_device(const InputDevice* device)
{
    const i32 index = find_index(g_input->devices, device);
    if (index < 0)
    {
        log_warning("Failed to unregister InputDevice: %s is not registered", device->name);
        return;
    }

    if (index < g_input->device_count - 1)
    {
        g_input->devices[index] = g_input->devices[g_input->device_count - 1];
    }

    --g_input->device_count;
    BEE_ASSERT(g_input->device_count >= 0);
}

i32 enumerate_devices(InputDevice const** dst)
{
    if (dst != nullptr && g_input->device_count > 0)
    {
        memcpy(dst, g_input->devices, sizeof(InputDevice*) * g_input->device_count);
    }

    return g_input->device_count;
}

bool find_device(const StringView& name, InputDevice const** dst)
{
    if (g_input->device_count <= 0)
    {
        return false;
    }

    const i32 index = find_index_if(g_input->devices, g_input->devices + g_input->device_count, [&](const InputDevice* device)
    {
        return device->name == name;
    });

    if (index < 0)
    {
        return false;
    }

    *dst = g_input->devices[index];
    return true;
}

const InputDevice* default_device(const InputDeviceType type)
{
    if (g_input->device_count <= 0)
    {
        return nullptr;
    }

    const i32 index = find_index_if(g_input->devices, g_input->devices + g_input->device_count, [&](const InputDevice* device)
    {
        return device->type == type;
    });

    return index >= 0 ? g_input->devices[index] : nullptr;
}


} // namespace bee

static bee::InputModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee::g_input = loader->get_static<bee::Input>("InputData");

    g_module.register_device = bee::register_device;
    g_module.unregister_device = bee::unregister_device;
    g_module.enumerate_devices = bee::enumerate_devices;
    g_module.find_device = bee::find_device;
    g_module.default_device = bee::default_device;

    loader->set_module(BEE_INPUT_MODULE_NAME, &g_module, state);
}
