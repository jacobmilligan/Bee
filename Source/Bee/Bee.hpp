/*
 *  Bee.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Error.hpp"
#include "Bee/Application/Platform.hpp"
#include "Bee/Core/Plugin.hpp"

namespace bee {


enum class ApplicationState
{
    running,
    quit_requested
};

struct Application;

#define BEE_APPLICATION_MODULE_NAME "BEE_APPLICATION_MODULE"

struct ApplicationModule
{
    Application*        instance {nullptr };

    int                 (*launch)(Application* app, int argc, char** argv) { nullptr };
    void                (*shutdown)(Application* app) { nullptr };
    void                (*fail)(Application* app) { nullptr };
    ApplicationState    (*tick)(Application* app) { nullptr };
};


} // namespace bee