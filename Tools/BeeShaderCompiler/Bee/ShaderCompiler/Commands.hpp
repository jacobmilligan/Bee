/*
 *  Commands.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

namespace bee {


/*
 *******************************************************
 *
 * # Bee Shader Compiler command header types
 *
 ********************************************************
 */
enum class BSCCommandType
{
    unknown,
    complete,
    shutdown
};

struct BSCCommand
{
    const BSCCommandType header { BSCCommandType::unknown };
};

template <BSCCommandType T>
struct BSCCommandData : public BSCCommand
{
    static constexpr BSCCommandType type = T;

    BSCCommandData()
        : BSCCommand { type }
    {}
};


/*
 *******************************************************
 *
 * # Bee Shader Compiler command structs
 *
 ********************************************************
 */
struct BSCShutdownCmd : BSCCommandData<BSCCommandType::shutdown>
{
    bool immediate { true };
};


} // namespace bee