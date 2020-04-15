/*
 *  Renderer.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/GPU.hpp"

namespace bee {


#define BEE_RENDER_MODULE_API_NAME "BEE_RENDER_MODULE_API"

struct RenderModuleApi
{
    const char* (*get_name)() { nullptr };

    void (*create_resources)(const DeviceHandle& device) { nullptr };

    void (*destroy_resources)(const DeviceHandle& device) { nullptr };

    void (*execute)(const DeviceHandle& device) { nullptr };
};


#define BEE_RENDERER_API_NAME "BEE_RENDERER_API"

struct RendererApi
{
    bool (*init)(const DeviceCreateInfo& device_info) { nullptr };

    void (*destroy)() { nullptr };

    void (*frame)() { nullptr };
};


} // namespace bee