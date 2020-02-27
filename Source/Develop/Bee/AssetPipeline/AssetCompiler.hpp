/*
 *  AssetCompiler.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Memory/SmartPointers.hpp"
#include "Bee/AssetPipeline/AssetPlatform.hpp"
#include "Bee/Core/Handle.hpp"


namespace bee {




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

    AssetCompilerContext(const AssetPlatform platform, const StringView& location, const TypeInstance& options, Allocator* allocator);

    io::MemoryStream add_artifact();

    void calculate_hashes();

    inline AssetPlatform platform() const
    {
        return platform_;
    }

    inline const StringView& location() const
    {
        return location_;
    }

    inline Allocator* temp_allocator()
    {
        return allocator_;
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
    StringView                      location_;
    const TypeInstance&             options_;
    Allocator*                      allocator_ { nullptr };
    DynamicArray<Artifact>          artifacts_;
};

struct AssetCompiler
{
    virtual ~AssetCompiler() = default;

    virtual AssetCompilerStatus compile(const i32 thread_index, AssetCompilerContext* ctx) = 0;
};


BEE_RAW_HANDLE_I32(AssetCompilerId);


class BEE_DEVELOP_API AssetCompilerPipeline
{
public:
    template <typename T, typename... ConstructorArgs>
    inline void register_compiler(const AssetCompilerKind kind, ConstructorArgs&&... args)
    {
        scoped_rw_read_lock_t lock(lock_);

        auto type = get_type<T>();

        // Validate unique compiler
        if (BEE_FAIL_F(find_compiler(type->hash) < 0, "%s is already a registered asset compiler", type->name))
        {
            return;
        }

        auto compiler = make_unique<T>(system_allocator(), std::forward<Args>(args)...);
        register_compiler(std::move(compiler));
    }

    template <typename T>
    inline void unregister_compiler()
    {
        unregister_compiler(get_type<T>());
    }

    Span<const AssetCompilerId> get_compiler_ids(const StringView& path);

    Span<const u32> get_compiler_hashes(const StringView& path);

    AssetCompiler* get_default_compiler(const StringView& path);

    AssetCompiler* get_compiler(const AssetCompilerId& id);

    AssetCompiler* get_compiler(const u32 hash);

    const Type* get_options_type(const AssetCompilerId& id);

    const Type* get_options_type(const u32 hash);

private:
    struct CompilerInfo
    {
        const Type*                 type { nullptr };
        const Type*                 options_type { nullptr };
        UniquePtr<AssetCompiler>    compiler { nullptr };
        DynamicArray<u32>           extensions;
    };

    struct FileTypeMapping
    {
        StaticString<32>                extension;
        DynamicArray<AssetCompilerId>   compiler_ids;
        DynamicArray<u32>               compiler_hashes;
    };

    ReaderWriterMutex                       mutex_;
    DynamicArray<CompilerInfo>              compilers_;
    DynamicHashMap<u32, FileTypeMapping>    filetype_map_;

    void register_compiler(const AssetCompilerKind kind, const Type* type, UniquePtr<AssetCompiler>&& compiler);

    void unregister_compiler(const Type* type);

    AssetCompilerId find_compiler(const u32 hash);
};


} // namespace bee