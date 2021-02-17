/*
 *  Api.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#define BEE_IMGUI_GENERATOR_IMPLEMENTATION
#include "Bee/ImGui/ImGui.hpp"

#include "Bee/Core/Plugin.hpp"

static bee::ImGuiModule g_imgui{};

static void* bee_imgui_alloc_fun(size_t sz, void* user_data)
{
    return BEE_MALLOC(bee::system_allocator(), sz);
}

static void bee_imgui_free_func(void* ptr, void* user_data)
{
    if (ptr == nullptr)
    {
        return;
    }

    BEE_FREE(bee::system_allocator(), ptr);
}

void bee_load_imgui(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee_load_imgui_api(&g_imgui);

    g_imgui.SetAllocatorFunctions(bee_imgui_alloc_fun, bee_imgui_free_func, nullptr);

    loader->set_module(BEE_IMGUI_MODULE_NAME, &g_imgui, state);
}