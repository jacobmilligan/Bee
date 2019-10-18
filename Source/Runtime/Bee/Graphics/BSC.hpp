/*
 *  Shader.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Serialization/Serialization.hpp"
#include "Bee/Graphics/GPU.hpp"

namespace bee {


#define BSC_MAX_NAME 1024


enum class BSCTarget
{
    MSL,
    HLSL,
    SPIRV,
    none
};


enum class BSCShaderType
{
    vertex = 0,
    fragment,
    count
};


/*
 *******************************************************
 *
 * # BSCShader
 *
 * A single shader stage in a graphics pipeline
 * description, i.e. vertex or fragment shader. Contains
 * the target code as binary data
 *
 ********************************************************
 */
struct BSCShader
{
    ShaderStage     stage { ShaderStage::unknown };
    char            entry[BSC_MAX_NAME];
    FixedArray<u8>  binary;
};


/*
 *******************************************************
 *
 * # BSCModule
 *
 * A group of shaders, resources, and a pipeline
 * description grouped into a single module. Represents
 * the result of compiling an entire .bsc file
 *
 ********************************************************
 */
struct BSCModule
{
    BSCTarget               target { BSCTarget::none };
    char                    name[BSC_MAX_NAME];
    char                    filename[BSC_MAX_NAME];
    BSCShader               shaders[static_cast<i32>(BSCShaderType::count)];
    i32                     shader_count { 0 };
    PipelineStateDescriptor pipeline_state;
};


struct BSCTextSource
{
    String                  name;
    String                  text;
    String                  shader_entries[static_cast<i32>(BSCShaderType::count)];
    i32                     shader_count { 0 };
    PipelineStateDescriptor pipeline_state;
};


/*
 *******************************************************
 *
 * # BSCShader & BSCModule serialization
 *
 ********************************************************
 */
BEE_SERIALIZE(1, BSCShader)
{
    BEE_ADD_FIELD(1, stage);
    BEE_ADD_FIELD(1, entry);
    BEE_ADD_FIELD(1, binary);
}

BEE_SERIALIZE(1, BSCModule)
{
    BEE_ADD_FIELD(1, target);
    BEE_ADD_FIELD(1, name);
    BEE_ADD_FIELD(1, filename);
    BEE_ADD_FIELD(1, shaders);
    BEE_ADD_FIELD(1, shader_count);
    BEE_ADD_FIELD(1, pipeline_state);
}


BEE_RUNTIME_API String bsc_target_to_string(const BSCTarget target, Allocator* allocator = system_allocator());

BEE_RUNTIME_API BSCTarget bsc_target_from_string(const StringView& target_string);

BEE_RUNTIME_API BSCTextSource bsc_parse_source(const Path& path, Allocator* allocator = system_allocator());



} // namespace bee