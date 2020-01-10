/*
 *  TestAssetTypes.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include <Bee/AssetPipelineV2/AssetCompilerV2.hpp>

namespace bee {


struct BEE_REFLECT(serializable) Texture
{
    BEE_REFLECT(nonserialized)
    bool loaded { false };
};

struct BEE_REFLECT(serializable, ext = "png", ext = "jpg", ext = "tga")
TextureCompiler final : public bee::AssetCompiler
{
    bool mipmap { false };

    AssetCompilerStatus compile(AssetCompilerContext* ctx) override
    {
        log_info("compiling texture...");
        auto stream = ctx->add_artifact();
        stream.write(&mipmap, sizeof(bool));
        return AssetCompilerStatus::success;
    }
};


} // namespace bee