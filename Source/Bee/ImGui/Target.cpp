/*
 *  Target.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Config.hpp"

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    BEE_DISABLE_WARNING_MSVC(4127)
    BEE_DISABLE_WARNING_MSVC(4189)

    #include <imgui/imgui.cpp>
    #include <imgui/imgui_draw.cpp>
    #include <imgui/imgui_demo.cpp>
    #include <imgui/imgui_widgets.cpp>
    #include <imgui/imgui_tables.cpp>

    #ifdef snprintf
        #undef snprintf
    #endif // snprintf

    #include <cimgui.cpp>
BEE_POP_WARNING