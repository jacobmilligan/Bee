/*
 *  Compiler.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

namespace bee {


#ifndef BEE_MAX_ASSET_COMPILER_PLUGINS
    #define BEE_MAX_ASSET_COMPILER_PLUGINS 32
#endif // BEE_MAX_ASSET_COMPILER_PLUGINS


BEE_FLAGS(AssetPlatform, u32)
{
    unknown = 0u,
    windows = 1u << 0u,
    macos   = 1u << 1u,
    linux   = 1u << 2u,
    metal   = 1u << 3u,
    vulkan  = 1u << 4u,
};


enum class AssetCompilerStatus
{
    success,
    fatal_error,
    unsupported_platform,
    unknown
};


struct AssetCompilerResult
{
    AssetCompilerStatus status { AssetCompilerStatus::unknown };
    Type                compiled_type;
};


struct AssetPipelineContext
{
    const char*     location;
    io::Stream*     stream { nullptr };
    AssetPlatform   platform { AssetPlatform::unknown };
    Allocator*      temp_allocator { nullptr };
};


BEE_DEFINE_VERSIONED_HANDLE(AssetCompiler);


using create_asset_compiler_function_t = AssetCompilerHandle(*)();
using asset_compile_function_t = AssetCompilerResult(*)(const AssetCompilerHandle& handle, AssetPipelineContext* ctx);

struct AssetPipelinePlugin
{
    const char*                         name { nullptr };
    i32                                 supported_file_type_count { 0 };
    const char* const*                  supported_file_types { nullptr };

    create_asset_compiler_function_t    create_compiler { nullptr };
    asset_compile_function_t            compile { nullptr };
};


struct AssetCompileWaitHandle
{
    std::atomic_bool    is_complete_flag { false };
    AssetCompilerResult result;
    DynamicArray<u8>    data;

    explicit AssetCompileWaitHandle(Allocator* data_allocator = system_allocator())
        : data(data_allocator)
    {}

    inline bool is_complete() const
    {
        return is_complete_flag.load(std::memory_order_seq_cst);
    }
};


class Job;


class BEE_ASSETCOMPILER_API AssetPipeline
{
public:
    bool load_plugin(const char* directory, const char* filename);

    bool unload_plugin(const char* name);

    bool compile(AssetPlatform platform, const char* src, AssetCompileWaitHandle* wait_handle);
private:
    using load_plugin_function_t    = AssetPipelinePlugin();
    using unload_plugin_function_t  = void();

    BEE_DEFINE_VERSIONED_HANDLE(RegisteredPlugin);

    struct RegisteredPlugin
    {
        String                      name;
        AssetPipelinePlugin         desc;
        DynamicArray<u32>           file_type_mappings;
        DynamicLibrary              library;
        load_plugin_function_t*     load_plugin_symbol { nullptr };
        unload_plugin_function_t*   unload_plugin_symbol { nullptr };

        // Registered callbacks and compilers
        DynamicArray<AssetCompilerHandle>               compilers;
        AssetPipelinePlugin::create_compiler_function_t create_compiler { nullptr };
        AssetPipelinePlugin::compile_function_t         compile { nullptr };

        RegisteredPlugin() = default;

        RegisteredPlugin(
            const AssetPipelinePlugin& desc,
            const DynamicLibrary& new_lib,
            load_plugin_function_t* load_symbol,
            unload_plugin_function_t* unload_symbol
        ) : name(desc.name),
            file_type_mappings(desc.supported_file_type_count),
            library(new_lib),
            load_plugin_symbol(load_symbol),
            unload_plugin_symbol(unload_symbol),
            create_compiler(desc.create_compiler),
            compile(desc.compile)
        {
            for (int ft = 0; ft < desc.supported_file_type_count; ++ft)
            {
                file_type_mappings.push_back(get_hash(desc.supported_file_types[ft]));
            }
        }
    };

    Job*                                    root_job { nullptr };

    SpinLock                                plugin_mutex;
    DynamicHashMap<u32, RegisteredPlugin>   plugins;

    SpinLock                                file_type_mutex;
    DynamicHashMap<u32, u32>                file_type_map;
};


#define BEE_DECLARE_PLUGIN(name)                                                                        \
        static const char* BEE_ASSET_COMPILER_NAME_##name = #name;                                      \
        extern "C" BEE_EXPORT_SYMBOL bee::AssetPipelinePlugin bee_asset_compiler_load_plugin_##name();  \
        extern "C" BEE_EXPORT_SYMBOL void bee_asset_compiler_unload_plugin_#name()



} // namespace bee