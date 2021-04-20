/*
 *  Api.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#define BEE_IMGUI_GENERATOR_IMPLEMENTATION
#include "Bee/ImGui/ImGui.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/String.hpp"


namespace bee {


static ImGuiModule g_imgui{};


static void* imgui_alloc_fun(size_t sz, void* user_data)
{
    return BEE_MALLOC(bee::system_allocator(), sz);
}

static void imgui_free_func(void* ptr, void* user_data)
{
    if (ptr == nullptr)
    {
        return;
    }

    BEE_FREE(bee::system_allocator(), ptr);
}

bool InputTextLeft(const char* label, char* buf, const size_t buf_size, const ImGuiInputTextFlags flags, const ImGuiInputTextCallback callback, void* user_data)
{
    static thread_local StaticString<1024> with_id;

    g_imgui.Text(label);
    g_imgui.SameLine(0, -1.0f);

    with_id = "##";
    with_id.append(label);

    return g_imgui.InputText(with_id.c_str(), buf, buf_size, flags, callback, user_data);
}

} // namespace bee


void bee_load_imgui(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee_load_imgui_api(&bee::g_imgui);

    // Load extensions
    bee::g_imgui.InputTextLeft = bee::InputTextLeft;

    // Set defaults
    bee::g_imgui.SetAllocatorFunctions(bee::imgui_alloc_fun, bee::imgui_free_func, nullptr);

    loader->set_module(BEE_IMGUI_MODULE_NAME, &bee::g_imgui, state);
}