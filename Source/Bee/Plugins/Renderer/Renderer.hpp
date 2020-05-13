/*
 *  Renderer.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/GPU.hpp"

namespace bee {


struct RenderStageData;

struct RenderStage
{
    RenderStageData* data {nullptr };

    void (*init)(const DeviceHandle& device, RenderStageData* data) {nullptr };

    void (*destroy)(const DeviceHandle& device, RenderStageData* data) {nullptr };

    void (*execute)(const DeviceHandle& device, RenderStageData* data) {nullptr };
};


#define BEE_RENDERER_MODULE_NAME "BEE_RENDERER_MODULE"

struct RendererModule
{
    bool (*init)(const DeviceCreateInfo& device_info) { nullptr };

    void (*destroy)() { nullptr };

    void (*frame)() { nullptr };

    void (*add_stage)(const u32 id, RenderStage* stage) {nullptr };

    void (*remove_stage)(RenderStage* stage) {nullptr };
};


} // namespace bee