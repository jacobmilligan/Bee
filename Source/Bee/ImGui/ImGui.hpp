/*
 *  ImGui.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Result.hpp"
#include "Bee/Core/Enum.hpp"

#ifndef BEE_IMGUI_INTERNAL
    BEE_PUSH_WARNING
        BEE_DISABLE_PADDING_WARNINGS
        #include "Bee/ImGui/Api.hpp"
    BEE_POP_WARNING
#endif // BEE_IMGUI_INTERNAL

#include "Bee/Gpu/GpuHandle.hpp"

#include "Bee/Platform/Platform.hpp"


namespace bee {


class Allocator;
struct AssetPipeline;
struct GpuBackend;


#define BEE_IMGUI_MODULE_NAME "BEE_IMGUI_MODULE"
#define BEE_IMGUI_BACKEND_MODULE_NAME "BEE_IMGUI_BACKEND"

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
            "Missing ImGui shader",                         // missing_shader
            "ImGui shader failed to load",                  // failed_to_load_shader
            "Failed to create ImGui font texture",          // failed_to_create_font_texture
        )
    }
};

struct ImGuiBackend;
struct ImGuiBackendModule
{
    Result<ImGuiBackend*, ImGuiError> (*create_backend)(const DeviceHandle device, GpuBackend* gpu, AssetPipeline* asset_pipeline, Allocator* allocator) { nullptr };

    Result<void, ImGuiError> (*destroy_backend)(ImGuiBackend* render) { nullptr };

    void (*draw)(ImGuiBackend* render, CommandBuffer* cmd_buf) { nullptr };

    void (*new_frame)(ImGuiBackend* frame, const WindowHandle window_handle) { nullptr };
};


} // namespace bee