/*
 *  AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Path.hpp>
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"

namespace bee {


bool AssetPipeline::init(const AssetPipelineInitInfo& info)
{
    if (BEE_FAIL(assetdb_.open(info.asset_source_root, info.assetdb_location, info.assetdb_name)))
    {
        return false;
    }

    assets_root_ = info.asset_source_root;
    return true;
}

AssetPlatform asset_pipeline_default_platform()
{
    return AssetPlatform::unknown
        | get_flag_if_true(BEE_OS_WINDOWS == 1, AssetPlatform::windows)
        | get_flag_if_true(BEE_OS_MACOS == 1, AssetPlatform::macos)
        | get_flag_if_true(BEE_OS_LINUX == 1, AssetPlatform::linux)
        | get_flag_if_true(BEE_CONFIG_GRAPHICS_API_METAL == 1, AssetPlatform::metal)
        | get_flag_if_true(BEE_CONFIG_GRAPHICS_API_VULKAN == 1, AssetPlatform::vulkan);
}

void meta_serialize(const SerializerMode mode, const Path& path, AssetMeta* meta, Allocator* allocator)
{
    if (mode == SerializerMode::reading)
    {
        auto src = fs::read(path, allocator);
        JSONReader reader(&src, allocator);
        serialize(mode, &reader, meta);
    }
    else
    {
        JSONWriter writer(allocator);
        serialize(mode, &writer, meta);
        fs::write(path, writer.c_str());
    }
}


struct AssetImportJob final : public Job
{
    struct ImportOperation
    {
        const Path*             meta_path { nullptr };
        Path                    artifact_path;
        AssetMeta               meta;
        DynamicArray<u8>        buffer;
    };

    FixedArray<Path>                    meta_paths;
    FixedArray<Path>                    paths;
    AssetPlatform                       platform;
    AssetDB*                            assetdb { nullptr };
    AssetCompilerPipeline*              compiler_pipeline { nullptr };

    explicit AssetImportJob(const Path& assets_root, const i32 count, const char* const* src_paths, const AssetPlatform dst_platform, AssetDB* assetdb_ptr, AssetCompilerPipeline* compiler_pipeline_ptr)
        : platform(dst_platform),
          assetdb(assetdb_ptr),
          compiler_pipeline(compiler_pipeline_ptr)
    {
        paths = FixedArray<Path>::with_size(count, job_temp_allocator());
        meta_paths = FixedArray<Path>::with_size(count, job_temp_allocator());
        for (int p = 0; p < paths.size(); ++p)
        {
            paths[p] = Path(assets_root.view(), job_temp_allocator()).append(src_paths[p]);
            meta_paths[p] = Path(paths[p].view(), job_temp_allocator()).append_extension(".meta");
        }
    }

    void execute() override
    {
        auto import_ops = FixedArray<ImportOperation>::with_size(paths.size(), job_temp_allocator());
        auto compile_reqs = FixedArray<AssetCompileRequest>::with_size(paths.size(), job_temp_allocator());
        auto compile_ops = FixedArray<AssetCompileOperation>::with_size(paths.size(), job_temp_allocator());

        int op_count = 0;

        for (int req = 0; req < paths.size(); ++req)
        {
            auto& src_path = paths[req];
            auto& meta_path = meta_paths[req];
            auto& op = import_ops[op_count];
            auto& compile_op = compile_ops[op_count];
            auto& compile_req = compile_reqs[op_count];

            // Has this already been imported?
            if (meta_path.exists())
            {
                meta_serialize(SerializerMode::reading, meta_path, &op.meta, job_temp_allocator());

                if (assetdb->asset_exists(op.meta.guid))
                {
                    log_info("Asset already exists - reimport?");
                    continue;
                }

                // Get the latest DB version of the info as we'll update the .meta file later
                assetdb->get_asset(op.meta.guid, &op.meta);
            }
            else
            {
                // Write out the new meta file
                op.meta.guid = generate_guid();
                meta_serialize(SerializerMode::writing, meta_path, &op.meta, job_temp_allocator());
                assetdb->put_asset(op.meta.guid, src_path.c_str());
            }

            op.meta_path = &meta_path;
            op.buffer = DynamicArray<u8>(job_temp_allocator());
            op.artifact_path = Path(job_temp_allocator());
            assetdb->get_paths(op.meta.guid, nullptr, &op.artifact_path);

            compile_req.src_path = src_path.c_str();
            compile_req.platform = platform;
            compile_op.reset(&op.buffer);

            ++op_count;
        }

        auto compile_job = compiler_pipeline->compile_assets(op_count, compile_reqs.data(), compile_ops.data());
        schedule_job(compile_job);
        job_wait(compile_job);

        for (int op = 0; op < op_count; ++op)
        {
            auto& import_op = import_ops[op];
            auto& compile_op = compile_ops[op];
            auto& compile_req = compile_reqs[op];

            if (compile_op.result.status != AssetCompilerStatus::success)
            {
                log_error("Failed to compile asset: %d", compile_op.result.status);
                continue;
            }

            // Serialize to file first
            meta_serialize(SerializerMode::writing, *import_op.meta_path, &import_op.meta, job_temp_allocator());

            // Update the assetdb first before writing the artifacts
            import_op.meta.type = (compile_op.result.compiled_type);
            assetdb->put_asset(import_op.meta, compile_req.src_path);

            fs::write(import_op.artifact_path, import_op.buffer.const_span());
        }
    }
};


Job* AssetPipeline::import_assets(const i32 asset_count, const char* const* paths, AssetPlatform dst_platform)
{
    auto job = allocate_job<AssetImportJob>(assets_root_, asset_count, paths, dst_platform, &assetdb_, &compiler_pipeline_);
    schedule_job(job);
    return job;
}


} // namespace bee