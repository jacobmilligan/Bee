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
#include "Bee/Core/Handle.hpp"
#include "Bee/AssetPipeline/AssetPlatform.hpp"


namespace bee {


enum class BEE_REFLECT() AssetCompilerStatus
{
    success,
    fatal_error,
    unsupported_platform,
    unsupported_filetype,
    invalid_source_format,
    unknown
};

class BEE_DEVELOP_API AssetCompilerContext
{
public:
    AssetCompilerContext(const AssetPlatform platform, const StringView& location, const StringView& cache_dir, const TypeInstance& options, Allocator* allocator);

    void add_artifact(const size_t size, const void* data);

    void calculate_hashes();

    inline AssetPlatform platform() const
    {
        return platform_;
    }

    inline const StringView& location() const
    {
        return location_;
    }

    inline const StringView& cache_directory() const
    {
        return cache_dir_;
    }

    inline Allocator* temp_allocator()
    {
        return allocator_;
    }

    inline const DynamicArray<AssetArtifact>& artifacts() const
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
    StringView                      cache_dir_;
    const TypeInstance&             options_;
    Allocator*                      allocator_ { nullptr };
    DynamicArray<AssetArtifact>     artifacts_;
};

struct AssetCompiler
{
    virtual void init(const i32 thread_count) = 0;

    virtual void destroy() = 0;

    virtual AssetCompilerStatus compile(const i32 thread_index, AssetCompilerContext* ctx) = 0;
};

class BEE_DEVELOP_API AssetCompilerRegistry final : public Noncopyable
{
public:
    ~AssetCompilerRegistry();

    void register_compiler(AssetCompiler* compiler, const Type* type);

    void unregister_compiler(const Type* type);

    template <typename CompilerType>
    inline void register_compiler(CompilerType* compiler)
    {
        register_compiler(compiler, get_type<CompilerType>());
    }

    template <typename CompilerType>
    inline void unregister_compiler()
    {
        unregister_compiler(get_type<CompilerType>());
    }

    AssetCompilerStatus compile(AssetCompilerContext* ctx);

    void clear();

private:
    struct CompilerInfo
    {
        const Type*         type { nullptr };
        const Type*         options_type { nullptr };
        AssetCompiler*      compiler { nullptr };
        DynamicArray<u32>   extensions;
    };

    struct FileTypeMapping
    {
        StaticString<32>    extension;
        DynamicArray<i32>   compiler_ids;
        DynamicArray<u32>   compiler_hashes;
    };

    DynamicArray<CompilerInfo>              compilers_;
    DynamicHashMap<u32, FileTypeMapping>    filetype_map_;

    i32 find_compiler(const u32 hash);
};


} // namespace bee