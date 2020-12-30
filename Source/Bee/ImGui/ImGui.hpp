/*
 *  ImGui.hpp.h
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Gpu/Gpu.hpp"

#define IMGUI_USER_CONFIG "ImConfig.hpp"
#include <imgui.h>

namespace bee {


#define BEE_IMGUI_MODULE_NAME "BEE_IMGUI_MODULE"

struct ImGuiModule
{
    bool (*init)(const DeviceHandle device)
};


} // namespace bee