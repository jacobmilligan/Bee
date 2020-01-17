/*
 *  AssetCompiler.hpp
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

const char* asset_compiler_status_to_string(const AssetCompilerStatus value);

constexpr AssetPlatform current_asset_os()
{
#if BEE_OS_WINDOWS == 1
    return AssetPlatform::windows;
#elif BEE_OS_MACOS == 1
    return AssetPlatform::macos;
#elif BEE_OS_LINUX == 1
    return AssetPlatform::linux;
#endif // BEE_OS_*
}

constexpr AssetPlatform current_asset_gfx_backend()
{
#if BEE_CONFIG_METAL_BACKEND == 1
    return AssetPlatform::metal;
#elif BEE_CONFIG_VULKAN_BACKEND == 1
    return AssetPlatform::vulkan;
#else
    return AssetPlatform::unknown
#endif // BEE_CONFIG_*_BACKEND
}


constexpr AssetPlatform default_asset_platform = current_asset_os() | current_asset_gfx_backend();


class AssetCompilerContext
{
public:
    struct Artifact
    {
        u128                hash;
        DynamicArray<u8>    buffer;

        explicit Artifact(Allocator* allocator = system_allocator())
            : buffer(allocator)
        {}
    };

    AssetCompilerContext(const AssetPlatform platform, const TypeInstance& options, Allocator* allocator);

    io::MemoryStream add_artifact();

    void calculate_hashes();

    inline AssetPlatform platform() const
    {
        return platform_;
    }

    inline const DynamicArray<Artifact>& artifacts() const
    {
        return artifacts_;
    }

    template <typename OptionsType>
    const OptionsType& options() const
    {
        return *options_.get<OptionsType>();
    }
private:
    AssetPlatform                   platform_ { AssetPlatform::unknown };
    const TypeInstance&             options_;
    Allocator*                      allocator_ { nullptr };
    DynamicArray<Artifact>          artifacts_;
};

struct AssetCompiler
{
    virtual ~AssetCompiler() = default;

    virtual AssetCompilerStatus compile(AssetCompilerContext* ctx) = 0;
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

Span<const i32> get_asset_compiler_ids(const StringView& path);

Span<const u32> get_asset_compiler_hashes(const StringView& path);

AssetCompiler* get_default_asset_compiler(const StringView& path);

AssetCompiler* get_asset_compiler(const i32 id);

AssetCompiler* get_asset_compiler(const u32 hash);

const Type* get_asset_compiler_options_type(const u32 compiler_hash);


} // namespace bee