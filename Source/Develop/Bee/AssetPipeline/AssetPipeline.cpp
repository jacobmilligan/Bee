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
    runtime_registry_.assetdb = &assetdb_;
    return true;
}

AssetPlatform asset_platform_default()
{
    return AssetPlatform::unknown
        | get_flag_if_true(BEE_OS_WINDOWS == 1, AssetPlatform::windows)
        | get_flag_if_true(BEE_OS_MACOS == 1, AssetPlatform::macos)
        | get_flag_if_true(BEE_OS_LINUX == 1, AssetPlatform::linux)
        | get_flag_if_true(BEE_CONFIG_METAL_BACKEND == 1, AssetPlatform::metal)
        | get_flag_if_true(BEE_CONFIG_VULKAN_BACKEND == 1, AssetPlatform::vulkan);
}


void log_asset_compiler_result(const AssetPlatform platform, const char* location, const AssetCompilerResult& result)
{
    if (result.status == AssetCompilerStatus::success)
    {
        log_info("Successfully compiled asset at %s", location);
        return;
    }

    switch (result.status)
    {
        case AssetCompilerStatus::fatal_error:
        {
            log_write(LogVerbosity::error, "Failed to compile asset at %s: fatal error", location);
            break;
        }
        case AssetCompilerStatus::unsupported_platform:
        {
            log_write(LogVerbosity::error, "Failed to compile asset at %s: unsupported platform: %s", location, asset_platform_to_string(platform));
            break;
        }
        case AssetCompilerStatus::invalid_source_format:
            log_write(LogVerbosity::error, "Failed to compile asset at %s: invalid source format", location);
            break;
        case AssetCompilerStatus::unknown:
        {
            log_write(LogVerbosity::error, "Failed to compile asset at %s: unknown error", location);
            break;
        }
        default: break;
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
    FixedArray<AssetCompileRequest>     requests;
    AssetDB*                            assetdb { nullptr };
    AssetCompilerPipeline*              compiler_pipeline { nullptr };

    explicit AssetImportJob(const Path& assets_root, const i32 count, const AssetCompileRequest* new_requests, AssetDB* assetdb_ptr, AssetCompilerPipeline* compiler_pipeline_ptr)
        : assetdb(assetdb_ptr),
          compiler_pipeline(compiler_pipeline_ptr)
    {
        paths = FixedArray<Path>::with_size(count, job_temp_allocator());
        meta_paths = FixedArray<Path>::with_size(count, job_temp_allocator());
        requests = FixedArray<AssetCompileRequest>::with_size(count, job_temp_allocator());

        for (int p = 0; p < count; ++p)
        {
            paths[p] = Path(assets_root.view(), job_temp_allocator()).append(new_requests[p].src_path);
            meta_paths[p] = Path(paths[p].view(), job_temp_allocator()).append_extension(BEE_ASSET_META_EXTENSION);
            requests[p] = new_requests[p];
            requests[p].src_path = paths[p].c_str();
        }
    }

    void execute() override
    {
        auto import_ops = FixedArray<ImportOperation>::with_size(paths.size(), job_temp_allocator());
        auto compile_ops = FixedArray<AssetCompileOperation>::with_size(paths.size(), job_temp_allocator());

        int op_count = 0;

        for (int req = 0; req < paths.size(); ++req)
        {
            auto& meta_path = meta_paths[req];
            auto& op = import_ops[op_count];
            auto& compile_op = compile_ops[op_count];
            auto settings = &requests[op_count].settings;

            // Has this already been imported?
            if (meta_path.exists())
            {
                asset_meta_serialize(SerializerMode::reading, meta_path, &op.meta, settings, job_temp_allocator());
            }
            else
            {
                // Write out the new meta file with new GUID if not imported
                op.meta.guid = generate_guid();
                io::write(&settings->json, R"({ "": { "bee::type": null } })");
                asset_meta_serialize(SerializerMode::writing, meta_path, &op.meta, settings, job_temp_allocator());
            }

            op.artifact_path = Path(job_temp_allocator());

            if (!assetdb->get_artifact_path(op.meta.guid, &op.artifact_path))
            {
                log_error("Failed to get asset artifact path");
                continue;
            }

            op.meta_path = &meta_path;
            op.buffer = DynamicArray<u8>(job_temp_allocator());
            compile_op.reset(&op.buffer);
            ++op_count;
        }

        JobGroup group{};
        compiler_pipeline->compile_assets(&group, op_count, requests.data(), compile_ops.data());
        job_wait(&group);

        for (int op = 0; op < op_count; ++op)
        {
            auto& import_op = import_ops[op];
            auto& compile_op = compile_ops[op];
            auto& compile_req = requests[op];
            auto settings = &requests[op].settings;

            import_op.meta.type = compile_op.result.compiled_type;

            // Serialize meta to file first to ensure integrity if any of the following functions fail
            asset_meta_serialize(SerializerMode::writing, *import_op.meta_path, &import_op.meta, settings, job_temp_allocator());

            // Update the assetdb before writing the artifacts
            import_op.meta.type = (compile_op.result.compiled_type);
            assetdb->put_asset(import_op.meta, compile_req.src_path);

            // Don't write out any artifacts if compilation failed - we still want to update assetdb and meta to maintain GUID's and types
            if (compile_op.result.status != AssetCompilerStatus::success)
            {
                log_asset_compiler_result(compile_req.platform, compile_req.src_path, compile_op.result);
                continue;
            }

            const auto dir = import_op.artifact_path.parent(job_temp_allocator());
            if (!dir.exists())
            {
                fs::mkdir(dir);
            }

            fs::write(import_op.artifact_path, import_op.buffer.const_span());
        }
    }
};


void AssetPipeline::import_assets(JobGroup* group, const i32 asset_count, const AssetCompileRequest* requests)
{
    auto job = allocate_job<AssetImportJob>(assets_root_, asset_count, requests, &assetdb_, &compiler_pipeline_);
    job_schedule(group, job);
}


bool AssetPipeline::AssetDBRegistry::locate_asset(const GUID& guid, io::FileStream* dst_stream)
{
    Path location(job_temp_allocator());
    if (!assetdb->get_artifact_path(guid, &location))
    {
        return false;
    }

    if (!location.exists())
    {
        return false;
    }

    dst_stream->reopen(location, "rb");
    return true;
}


} // namespace bee