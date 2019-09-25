/*
 *  AssetPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/AssetPipeline/AssetDatabase.hpp"


namespace bee {


struct AssetPipelineInitInfo
{
    const char* asset_source_root { nullptr };
    const char* assetdb_location { nullptr };
    const char* assetdb_name { nullptr };
};


BEE_DEVELOP_API AssetPlatform asset_pipeline_default_platform();


class BEE_DEVELOP_API AssetPipeline
{
public:
    bool init(const AssetPipelineInitInfo& info);

    template <typename CompilerType>
    inline bool register_asset_compiler()
    {
        return compiler_pipeline_.register_compiler<CompilerType>();
    }

    inline void unregister_compiler(const char* name)
    {
        compiler_pipeline_.unregister_compiler(name);
    }

    void import_assets(JobGroup* group, const i32 asset_count, const char* const* paths, AssetPlatform dst_platform = asset_pipeline_default_platform());
private:
    Path                    assets_root_;
    AssetDB                 assetdb_;
    AssetCompilerPipeline   compiler_pipeline_;
};


} // namespace bee