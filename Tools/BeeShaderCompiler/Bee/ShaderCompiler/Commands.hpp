/*
 *  Commands.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ShaderCompiler/Module.hpp"
#include "Bee/Core/Serialization/MemorySerializer.hpp"

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
    shutdown,
    compile
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

BEE_SERIALIZE(BSCShutdownCmd, 1)
{
    BEE_ADD_FIELD(1, immediate);
}

struct BSCCompileCmd : BSCCommandData<BSCCommandType::compile>
{
    BSCTarget           target;
    FixedArray<Path>    source_paths;
};

BEE_SERIALIZE(BSCCompileCmd, 1)
{
    BEE_ADD_FIELD(1, target);
    BEE_ADD_FIELD(1, source_paths);
}


} // namespace bee