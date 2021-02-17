/*
 *  ShaderPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ShaderPipeline/ShaderPipeline.hpp"

#include "Bee/Core/Result.hpp"
#include "Bee/Core/GUID.hpp"


namespace bee {


struct ShaderCompilerError
{
    enum Enum
    {
        invalid_source,
        shader_cache_create_failed,
        dxc_compilation_failed,
        spirv_failed_to_generate,
        reflection_failed,
        incompatible_resource_layouts,
        fatal_error,
        unknown
    };

    BEE_ENUM_STRUCT(ShaderCompilerError);

    const char* to_string() const
    {
        BEE_TRANSLATION_TABLE(value, Enum, const char*, Enum::unknown,
            "invalid_source",
            "shader_cache_create_failed",
            "dxc_compilation_failed",
            "spirv_failed_to_generate",
            "reflection_failed",
            "incompatible_resource_layouts",
            "fatal_error",
        )
    }
};

BEE_REFLECTED_FLAGS(ShaderTarget, u32, serializable)
{
    unknown     = 0u,
    spirv       = 1u << 0u,
    spirv_debug = 1u << 1u
};


#define BEE_SHADER_COMPILER_MODULE_NAME "BEE_SHADER_COMPILER"

struct AssetPipeline;
struct ShaderCache;
struct ShaderCompilerModule
{
    bool (*init)() { nullptr };

    void (*destroy)() { nullptr };

    Result<void, ShaderCompilerError> (*compile_shader)(const StringView& source_path, const StringView& source, const ShaderTarget target, DynamicArray<Shader>* dst, Allocator* code_allocator) { nullptr };

    void (*disassemble_shader)(const StringView& source_path, const Shader& shader, String* dst) { nullptr };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Compiler.generated.inl"
#endif // BEE_ENABLE_REFLECTION