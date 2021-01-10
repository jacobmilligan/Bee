/*
 *  ImGui.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ImGui/Api.hpp"

#include "Bee/Gpu/GpuHandle.hpp"

#include "Bee/Core/Result.hpp"
#include "Bee/Core/Enum.hpp"


namespace bee {


class Allocator;
struct AssetPipelineModule;
struct AssetPipeline;
struct ShaderCache;
struct GpuBackend;


#define BEE_IMGUI_MODULE_NAME "BEE_IMGUI_MODULE"
#define BEE_IMGUI_RENDER_MODULE_NAME "BEE_IMGUI_RENDER"

struct ImGuiError
{
    enum Enum
    {
        missing_shader,
        failed_to_load_shader,
        failed_to_create_font_texture,
        count
    };

    BEE_ENUM_STRUCT(ImGuiError);

    const char* to_string() const
    {
        BEE_TRANSLATION_TABLE(value, Enum, const char*, Enum::count,
            "ImGui shader was missing in the shader cache", // missing_shader
            "ImGui shader failed to load",                  // failed_to_load_shader
            "Failed to create ImGui font texture",          // failed_to_create_font_texture
        )
    }
};

struct ImGuiRender;
struct ImGuiRenderModule
{
    Result<ImGuiRender*, ImGuiError> (*create)(const DeviceHandle device, GpuBackend* gpu, ShaderCache* shader_cache, Allocator* allocator) { nullptr };

    Result<void, ImGuiError> (*destroy)(ImGuiRender* render) { nullptr };

    void (*draw)(ImGuiRender* render, CommandBuffer* cmd_buf) { nullptr };
};


} // namespace bee