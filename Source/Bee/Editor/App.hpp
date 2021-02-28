/*
 *  App.hpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"


namespace bee {


struct BEE_REFLECT(serializable) Project
{

};


#define BEE_EDITOR_APP_MODULE_NAME "BEE_EDITOR_APP"

struct EditorAppModule
{
    bool (*startup)() { nullptr };

    void (*shutdown)() { nullptr };

    void (*tick)() { nullptr };

    bool (*quit_requested)() { nullptr };
};


} // namespace bee