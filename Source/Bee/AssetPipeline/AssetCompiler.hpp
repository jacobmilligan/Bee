/*
 *  AssetPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"


#include <atomic>

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
    unknown
};


struct AssetCompilerResult
{
    AssetCompilerStatus status { AssetCompilerStatus::unknown };
    Type                compiled_type;

    AssetCompilerResult() = default;

    AssetCompilerResult(const AssetCompilerStatus final_status, const Type& final_type)
        : status(final_status),
          compiled_type(final_type)
    {}
};

struct AssetCompileContext
{
    const AssetPlatform platform { AssetPlatform::unknown };
    const Path*         location;
    io::Stream*         stream { nullptr };
    Allocator*          temp_allocator { nullptr };

    AssetCompileContext(const AssetPlatform new_platform, const Path* new_location)
        : platform(new_platform),
          location(new_location)
    {}
};

struct AssetCompileRequest
{
    AssetPlatform   platform { AssetPlatform::unknown };
    const char*     src_path { nullptr };

    AssetCompileRequest() = default;

    AssetCompileRequest(const char* new_src_path, const AssetPlatform new_platform)
        : src_path(new_src_path),
          platform(new_platform)
    {}
};

struct AssetCompileOperation
{
    Job*                job { nullptr };
    AssetCompilerResult result;
    io::MemoryStream    data { nullptr };

    AssetCompileOperation() = default;

    explicit AssetCompileOperation(DynamicArray<u8>* dst_data)
        : data(dst_data)
    {}

    void reset(DynamicArray<u8>* dst_data)
    {
        new (&data) io::MemoryStream(dst_data);
    }
};


struct BEE_DEVELOP_API AssetCompiler
{
    virtual ~AssetCompiler() = default;

    virtual AssetCompilerResult compile(AssetCompileContext* ctx) = 0;
};


class BEE_DEVELOP_API AssetCompilerPipeline
{
public:

    template <typename CompilerType>
    bool register_compiler()
    {
        auto file_types = CompilerType::supported_file_types;
        auto file_type_count = static_array_length(CompilerType::supported_file_types);
        return register_compiler(Type::from<CompilerType>(), file_types, file_type_count, [](Allocator* allocator)
        {
            return BEE_NEW(allocator, CompilerType)();
        });
    }

    void unregister_compiler(const char* name);

    void compile_assets(JobGroup* group, i32 count, const AssetCompileRequest* requests, AssetCompileOperation* operations);
private:
    using create_function_t = Function<AssetCompiler*(Allocator*)>;

    struct RegisteredCompiler
    {
        Type                        type;
        FixedArray<u32>             file_types;
        create_function_t           create;
        FixedArray<AssetCompiler*>  instances;

        RegisteredCompiler(
            const Type& new_type,
            const char* const* new_file_types,
            const i32 new_file_type_count,
            create_function_t&& create_function
        );

        ~RegisteredCompiler();
    };

    struct AssetCompileJob final : public Job
    {
        RegisteredCompiler*     compiler { nullptr };
        AssetPlatform           platform;
        Path                    src_path;
        AssetCompileOperation*  operation{ nullptr };

        AssetCompileJob(RegisteredCompiler* requested_compiler, const AssetCompileRequest& request, AssetCompileOperation* dst_operation);

        void execute() override;
    };

    SpinLock                            mutex_;
    DynamicHashMap<u32, i32>            file_type_map_;
    DynamicArray<RegisteredCompiler>    compilers_;

    bool register_compiler(const Type& type, const char* const* supported_file_types, i32 supported_file_type_count, create_function_t&& create_function);

    RegisteredCompiler* find_compiler_no_lock(const char* name);

    i32 get_free_compiler_no_lock();
};


} // namespace bee