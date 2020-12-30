/*
 *  ShaderPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


struct Shader;

enum class ShaderCompilerResult
{
    success,
    invalid_source,
    shader_cache_create_failed,
    dxc_compilation_failed,
    spirv_failed_to_generate,
    reflection_failed,
    incompatible_resource_layouts,
    fatal_error
};

BEE_REFLECTED_FLAGS(ShaderTarget, u32, serializable)
{
    unknown     = 0u,
    spirv       = 1u << 0u,
    spirv_debug = 1u << 1u
};

struct BEE_REFLECT(serializable, version = 1) ShaderImportSettings
{
    bool output_debug_shaders { false };
};


#define BEE_SHADER_COMPILER_MODULE_NAME "BEE_SHADER_COMPILER"

struct AssetPipeline;
struct ShaderCache;
struct ShaderPipelineHandle;
struct ShaderCompilerModule
{
    bool (*init)() { nullptr };

    void (*destroy)() { nullptr };

    ShaderCompilerResult (*compile_shader)(ShaderCache* cache, const StringView& source_path, const StringView& source, const ShaderTarget target, DynamicArray<ShaderPipelineHandle>* dst) { nullptr };

    void (*register_importer)(AssetPipeline* pipeline, ShaderCache* cache) { nullptr };

    void (*unregister_importer)(AssetPipeline* pipeline) { nullptr };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Compiler.generated.inl"
#endif // BEE_ENABLE_REFLECTION