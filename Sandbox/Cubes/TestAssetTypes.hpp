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

struct BEE_REFLECT(serializable) TextureCompilerOptions
{
    bool mipmap { false };
};

struct BEE_REFLECT(options = bee::TextureCompilerOptions, ext = "png", ext = "jpg", ext = "jpeg", ext = "tga")
TextureCompiler final : public bee::AssetCompiler
{
    AssetCompilerStatus compile(AssetCompilerContext* ctx) override
    {
        auto& options = ctx->options<TextureCompilerOptions>();
        log_info("compiling texture...");
        auto stream = ctx->add_artifact();
        stream.write(&options.mipmap, sizeof(bool));
        return AssetCompilerStatus::success;
    }
};


} // namespace bee