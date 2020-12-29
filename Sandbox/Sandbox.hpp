/*
 *  Sandbox.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/ShaderPipeline/Cache.hpp"
#include "Bee/Core/GUID.hpp"


namespace bee {


#define BEE_SANDBOX_MODULE_NAME "BEE_SANDBOX"

struct BEE_REFLECT(serializable) ShaderImportSettings
{
    bool compile_debug_shaders { false };
};

struct BEE_REFLECT(serializable) ShaderAsset
{
    DynamicArray<u32> shader_hashes;

    ShaderAsset(Allocator* allocator = system_allocator())
        : shader_hashes(allocator)
    {}
};

struct SandboxModule
{
    bool (*startup)() { nullptr };

    void (*shutdown)() { nullptr };

    bool (*tick)() { nullptr };

};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.Sandbox/Sandbox.generated.inl"
#endif // BEE_ENABLE_REFLECTION