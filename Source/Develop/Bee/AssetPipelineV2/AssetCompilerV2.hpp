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
    AssetCompilerContext(const GUID& guid, const AssetPlatform platform, Allocator* allocator);

    template <typename T>
    inline void add_artifact(T* data)
    {
        // We need to write into a temp buffer first before we can write to file to get a content hash
        DynamicArray<u8> buffer(allocator_);
        BinarySerializer serializer(&buffer);
        serialize(SerializerMode::writing, &serializer, data);

        HashState128 hash;
        hash.add(guid_);
        hash.add(type->hash);
        hash.add(platform_);
        hash.add(buffer.data(), buffer.size());
    }
private:
    struct Artifact
    {
        u128        hash;
        void*       data { nullptr };
    };

    GUID                    guid_;
    Allocator*              allocator_ { nullptr };
    AssetPlatform           platform_ { AssetPlatform::unknown };
    DynamicArray<Artifact>  artifacts_;

    void add_artifact(const Type* type, void* data);
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

void register_asset_compiler(const Type* type, AssetCompiler*(*allocate_function)());

void unregister_asset_compiler(const Type* type);

void asset_compiler_add_file_type(const Type* type, const char* extension);

void asset_compiler_remove_file_type(const Type* type, const char* extension);

template <typename T>
inline void register_asset_compiler()
{
    register_asset_compiler(get_type<T>(), []() -> AssetCompiler*
    {
        return BEE_NEW(system_allocator(), T)();
    });
}

template <typename T>
inline void unregister_asset_compiler()
{
    unregister_asset_compiler(get_type<T>());
}

template <typename T>
inline void asset_compiler_add_file_type(const char* file_type)
{
    asset_compiler_add_file_type(get_type<T>(), file_type);
}

template <typename T>
inline void asset_compiler_remove_file_type(const char* file_type)
{
    asset_compiler_remove_file_type(get_type<T>(), file_type);
}

bool compile_asset(JobGroup* group, const AssetPlatform platform, const char* path);

bool compile_asset_sync(const AssetPlatform platform, const char* path);


} // namespace bee