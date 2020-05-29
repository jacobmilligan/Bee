/*
 *  ImGui.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once


namespace bee {


#define BEE_IMGUI_MODULE_NAME "BEE_IMGUI_MODULE"

struct ImGuiModule
{
    void (*init)() { nullptr };

    void (*destroy)() { nullptr };
};


} // namespace bee