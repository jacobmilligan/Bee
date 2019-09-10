/*
 *  BeeShaderCompiler.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Graphics/GPU.hpp"
#include "Bee/Core/Socket.hpp"
#include "Bee/ShaderCompiler/Commands.hpp"

namespace bee {


#define BSC_MAX_NAME 1024

#define BSC_DEFAULT_PORT 8888

#ifndef BSC_MAX_CLIENTS
    #define BSC_MAX_CLIENTS 16
#endif // BSC_MAX_CLIENTS


enum class BSCTarget
{
    MSL,
    HLSL,
    SPIRV,
    none
};


enum class BSCShaderStage
{
    vertex,
    fragment,
    last = fragment
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
    BSCShaderStage  stage { BSCShaderStage::last };
    char            name[BSC_MAX_NAME];
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
    BSCShader               shaders[static_cast<i32>(BSCShaderStage::last)];
    PipelineStateDescriptor pipeline_state;
};


/*
 *******************************************************
 *
 * # BSCShader & BSCModule serialization
 *
 ********************************************************
 */
BEE_SERIALIZE(BSCShader, 1)
{
    BEE_ADD_FIELD(1, stage);
    BEE_ADD_FIELD(1, name);
    BEE_ADD_FIELD(1, binary);
}

BEE_SERIALIZE(BSCModule, 1)
{
    BEE_ADD_FIELD(1, target);
    BEE_ADD_FIELD(1, name);
    BEE_ADD_FIELD(1, filename);
    BEE_ADD_FIELD(1, shaders);
    BEE_ADD_FIELD(1, pipeline_state);
}


/*
 *******************************************************
 *
 * # Bee Shader Compiler API
 *
 ********************************************************
 */
BEE_SHADERCOMPILER_API int bsc_server_listen(const SocketAddress& address);

BEE_SHADERCOMPILER_API socket_t bsc_connect_client(const SocketAddress& address);

BEE_SHADERCOMPILER_API bool bsc_shutdown_server(const socket_t client, const bool immediate = true);


} // namespace bee