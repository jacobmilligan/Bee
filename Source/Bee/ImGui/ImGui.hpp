/*
 *  ImGui.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ImGui/Api.hpp"

namespace bee {


#define BEE_IMGUI_MODULE_NAME "BEE_IMGUI_MODULE"

#define BEE_IMGUI_RENDER_MODULE "BEE_IMGUI_RENDER"

struct RenderGraphPass;
struct RenderGraph;
struct RenderGraphModule;
struct ImGuiRenderModule
{
    RenderGraphPass* (*add_render_pass)(RenderGraphModule* rg, RenderGraph* graph) { nullptr };
};


} // namespace bee