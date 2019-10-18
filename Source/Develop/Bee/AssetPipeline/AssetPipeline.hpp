/*
 *  AssetPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/AssetPipeline/AssetDatabase.hpp"
#include "Bee/Asset/AssetSystem.hpp"


namespace bee {


#ifndef BEE_ASSET_META_EXTENSION
    #define BEE_ASSET_META_EXTENSION ".meta"
#endif // BEE_ASSET_META_EXTENSION


struct AssetPipelineInitInfo
{
    const char*     asset_source_root { nullptr };
    const char*     assetdb_location { nullptr };
    const char*     assetdb_name { nullptr };
};


class BEE_DEVELOP_API AssetPipeline
{
public:
    ~AssetPipeline();

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

    void import_assets(JobGroup* group, const i32 asset_count, const AssetCompileRequest* requests);
private:
    struct AssetDBRegistry final : public AssetRegistry
    {
        AssetDB* assetdb {nullptr };

        bool locate_asset(const GUID& guid, io::FileStream* dst_stream) override;
    };

    Path                    assets_root_;
    AssetDB                 assetdb_;
    AssetDBRegistry         runtime_registry_;
    AssetCompilerPipeline   compiler_pipeline_;
};


BEE_DEVELOP_API AssetPlatform asset_platform_default();


} // namespace bee