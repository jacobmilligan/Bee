/*
 *  BeeShaderCompiler.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Socket.hpp"
#include "Bee/ShaderCompiler/Module.hpp"

namespace bee {


#define BSC_DEFAULT_PORT 8888

#ifndef BSC_MAX_CLIENTS
    #define BSC_MAX_CLIENTS 16
#endif // BSC_MAX_CLIENTS


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

BEE_SHADERCOMPILER_API bool bsc_compile(const socket_t client, const BSCTarget target, i32 source_count, const Path* source_paths, BSCModule* dst_modules);


} // namespace bee