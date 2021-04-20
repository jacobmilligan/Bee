/*
 *  EditorWindow.hpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/String.hpp"

namespace bee {


struct NewProjectWindow
{
    bool                open { false };
    BEE_PAD(3);
    StaticString<256>   name;
    StaticString<1024>  location;
};

struct EditorWindow
{
    NewProjectWindow new_project;
};

struct ImGuiModule;
struct PlatformModule;

void tick_editor_window(PlatformModule* platform, ImGuiModule* imgui, EditorWindow* window);


} // namespace bee