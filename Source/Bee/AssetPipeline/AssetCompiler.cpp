/*
 *  AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetCompiler.hpp"

namespace bee {


AssetCompilerPipeline::RegisteredCompiler::RegisteredCompiler(
    const bee::Type& new_type,
    const char* const* new_file_types,
    const i32 new_file_type_count,
    create_function_t&& create_function
) : type(new_type), create(create_function)
{
    file_types = FixedArray<u32>::with_size(new_file_type_count);
    instances = FixedArray<AssetCompiler*>::with_size(get_job_worker_count());

    for (int ft = 0; ft < new_file_type_count; ++ft)
    {
        file_types[ft] = get_hash(new_file_types[ft]);
    }

    for (auto& instance : instances)
    {
        instance = nullptr;
    }
}

AssetCompilerPipeline::RegisteredCompiler::~RegisteredCompiler()
{
    for (auto& instance : instances)
    {
        if (instance != nullptr)
        {
            BEE_DELETE(system_allocator(), instance);
        }
    }

    instances.clear();
    file_types.clear();
    type = Type{};
}


AssetCompilerPipeline::RegisteredCompiler* AssetCompilerPipeline::find_compiler_no_lock(const char* name)
{
    const auto hash = get_hash(name);
    for (auto& compiler : compilers_)
    {
        if (compiler.type.hash == hash)
        {
            return &compiler;
        }
    }
    return nullptr;
}

i32 AssetCompilerPipeline::get_free_compiler_no_lock()
{
    for (int i = 0; i < compilers_.size(); ++i)
    {
        if (!compilers_[i].type.is_valid())
        {
            return i;
        }
    }
    return -1;
}


bool AssetCompilerPipeline::register_compiler(const Type& type, const char* const* supported_file_types, i32 supported_file_type_count, create_function_t&& create_function)
{
    scoped_spinlock_t lock(mutex_);

    if (BEE_FAIL_F(find_compiler_no_lock(type.name) == nullptr, "\"%s\" is already a registered asset compiler", type.name))
    {
        return false;
    }

    // Ensure that no file type is supported by multiple compilers
    for (int ft = 0; ft < supported_file_type_count; ++ft)
    {
        const auto found_filetype = file_type_map_.find(get_hash(supported_file_types[ft]));
        if (BEE_FAIL_F(found_filetype == nullptr, "File type with extension \"%s\" is already supported by asset compiler \"%s\"", supported_file_types[ft], compilers_[found_filetype->value].type.name))
        {
            return false;
        }
    }

    // Find a free compiler (can be free slots if compilers are unregistered)
    auto compiler_idx = get_free_compiler_no_lock();
    if (compiler_idx < 0)
    {
        compilers_.emplace_back(type, supported_file_types, supported_file_type_count, std::forward<create_function_t>(create_function));
        compiler_idx = compilers_.size() - 1;
    }
    else
    {
        new (&compilers_[compiler_idx]) RegisteredCompiler(type, supported_file_types, supported_file_type_count, std::forward<create_function_t>(create_function));
    }

    // Add all the filetype mappings
    for (int ft = 0; ft < supported_file_type_count; ++ft)
    {
        file_type_map_.insert(get_hash(supported_file_types[ft]), compiler_idx);
    }

    return true;
}

void AssetCompilerPipeline::unregister_compiler(const char* name)
{
    scoped_spinlock_t lock(mutex_);

    auto compiler = find_compiler_no_lock(name);
    if (BEE_FAIL_F(compiler != nullptr, "No asset compiler found with a name that matches \"%s\"", name))
    {
        return;
    }

    // Remove all the filetype mappings
    for (const auto& filetype : compiler->file_types)
    {
        file_type_map_.erase(filetype);
    }

    // zeros out the registered compiler to be reused if another gets registered
    destruct(compiler);
}

AssetCompilerPipeline::AssetCompileJob::AssetCompileJob(
    bee::AssetCompilerPipeline::RegisteredCompiler* requested_compiler,
    const bee::AssetCompileRequest& request,
    bee::AssetCompileOperation* dst_operation
) : compiler(requested_compiler),
    platform(request.platform),
    src_path(request.src_path, job_temp_allocator()),
    operation(dst_operation)
{}

void AssetCompilerPipeline::AssetCompileJob::execute()
{
    AssetCompileContext ctx(platform, &src_path);
    ctx.temp_allocator = job_temp_allocator();
    ctx.stream = &operation->data;

    auto instance = compiler->instances[get_local_job_worker_id()];
    if (instance == nullptr)
    {
        instance = compiler->create(system_allocator());
    }

    operation->result = instance->compile(&ctx);
}


void AssetCompilerPipeline::compile_assets(JobGroup* group, const i32 count, const AssetCompileRequest* requests, AssetCompileOperation* operations)
{
    scoped_spinlock_t lock(mutex_);

    for (int req = 0; req < count; ++req)
    {
        const auto extension = path_get_extension(requests[req].src_path);
        auto compiler = file_type_map_.find(get_hash(extension));
        if (compiler == nullptr)
        {
            log_error("Cannot compile asset at path: %s. No registered asset compiler found that supports file types with extension \"%" BEE_PRIsv "\"", requests[req].src_path, BEE_FMT_SV(extension));
            continue;
        }

        operations[req].job = allocate_job<AssetCompileJob>(&compilers_[compiler->value], requests[req], &operations[req]);
        job_schedule(group, operations[req].job);
    }
}




} // namespace bee