/*
 *  AssetPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/AssetPipeline/AssetDatabase.hpp"
#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


enum class DeleteAssetKind
{
    asset_only,
    asset_and_source
};

class AssetCompilerRegistry;
class AssetPipeline;

#define BEE_ASSET_PIPELINE_MODULE_NAME "BEE_ASSET_PIPELINE_MODULE"

struct AssetPipelineModule
{
    void (*import_assets)(AssetPipeline* pipeline) { nullptr };
    void (*delete_assets)(AssetPipeline* pipeline) { nullptr };
    void (*register_compilers)(AssetCompilerRegistry* registry) { nullptr };
    void (*unregister_compilers)(AssetCompilerRegistry* registry) { nullptr };
};

struct AssetPipelineInitInfo
{
    AssetPlatform   platform { AssetPlatform::unknown };
    Path            assets_source_root;
    Path            asset_database_directory;
    const char*     asset_database_name { nullptr };
};


class BEE_DEVELOP_API AssetPipeline
{
public:
    ~AssetPipeline();

    void init(const AssetPipelineInitInfo& info);

    void destroy();

    void set_platform(const AssetPlatform platform);

    void import_asset(const Path& source_path, const Path& dst_path, const StringView& name);

    void delete_asset(const GUID& guid, const DeleteAssetKind kind = DeleteAssetKind::asset_only);

    void delete_asset(const StringView& name, const DeleteAssetKind kind = DeleteAssetKind::asset_only);

    inline bool is_initialized() const
    {
        return !assets_root_.empty();
    }
private:
    AssetPlatform           platform_ { AssetPlatform::unknown };
    Path                    assets_root_;
    Path                    cache_root_;
    AssetDatabase           db_;
    AssetCompilerRegistry   compilers_;
    JobGroup                import_jobs_;

    static void module_observer(const PluginEventType type, const char* plugin_name, void* inteface, void* user_data);

    static void import_job(AssetPipeline* pipeline, const Path& src, const Path& dst, const StringView& name);
};


} // namespace bee