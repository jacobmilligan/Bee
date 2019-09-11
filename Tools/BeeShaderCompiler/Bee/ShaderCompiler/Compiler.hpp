/*
 *  Commands.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ShaderCompiler/Commands.hpp"
#include "Bee/Core/DynamicLibrary.hpp"


struct IDxcCompiler;
struct IDxcLibrary;


namespace bee {


class BSCCompiler
{
public:
    BSCCompiler();

    ~BSCCompiler();

    inline bool shutdown_immediate()
    {
        return shutdown_immediate_;
    }

    inline bool shutdown_deferred()
    {
        return shutdown_deferred_;
    }

    inline void reset()
    {
        shutdown_deferred_ = false;
        shutdown_immediate_ = false;
    }

    /*
     *******************************************************
     *
     * # Bee Shader Compiler command processing functions
     *
     ********************************************************
     */
    bool process_command(const BSCShutdownCmd& cmd);

    bool process_command(const BSCCompileCmd& cmd);

private:
    DynamicLibrary  dxc_dll_;

    IDxcCompiler*   dxc_compiler_ { nullptr };
    IDxcLibrary*    dxc_library_ { nullptr };

    bool            shutdown_deferred_ { false };
    bool            shutdown_immediate_ { false };
};


} // namespace bee