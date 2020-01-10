/*
 *  AssetCompilerV2.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"


namespace bee {


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
    invalid_source_format,
    unknown
};

enum class AssetCompilerKind
{
    default_compiler,
    custom_compiler
};

struct AssetCompilerResult
{
    AssetCompilerStatus status { AssetCompilerStatus::unknown };
    const Type*         compiled_type { nullptr };

    AssetCompilerResult() = default;

    AssetCompilerResult(const AssetCompilerStatus new_status, const Type* new_compiled_type)
        : status(new_status),
          compiled_type(new_compiled_type)
    {}
};

class AssetCompilerContext
{
public:
    AssetCompilerContext(const AssetPlatform platform, Allocator* allocator);

    io::MemoryStream add_artifact();

    inline AssetPlatform platform() const
    {
        return platform_;
    }

    inline const DynamicArray<DynamicArray<u8>>& artifacts() const
    {
        return artifacts_;
    }
private:
    Allocator*                      allocator_ { nullptr };
    AssetPlatform                   platform_ { AssetPlatform::unknown };
    DynamicArray<DynamicArray<u8>>  artifacts_;
};

struct AssetCompiler
{
    virtual ~AssetCompiler() = default;

    virtual AssetCompilerStatus compile(AssetCompilerContext* ctx) = 0;
};

struct BEE_REFLECT(serializable) AssetMeta
{
    GUID    guid;
    Path    source;
};

void register_asset_compiler(const AssetCompilerKind kind, const Type* type, AssetCompiler*(*allocate_function)());

void unregister_asset_compiler(const Type* type);

template <typename T>
inline void register_asset_compiler(const AssetCompilerKind kind)
{
    register_asset_compiler(kind, get_type<T>(), []() -> AssetCompiler*
    {
        return BEE_NEW(system_allocator(), T)();
    });
}

template <typename T>
inline void unregister_asset_compiler()
{
    unregister_asset_compiler(get_type<T>());
}

AssetCompiler* get_default_asset_compiler(const char* path);

AssetCompiler* get_asset_compiler(const u32 hash);

AssetCompiler* get_asset_compiler(const Type* type);

template <typename T>
inline AssetCompiler* get_asset_compiler()
{
    return get_asset_compiler(get_type<T>());
}


} // namespace bee