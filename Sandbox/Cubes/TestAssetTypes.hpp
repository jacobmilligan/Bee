/*
 *  TestAssetTypes.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include <Bee/AssetPipeline/AssetCompiler.hpp>

namespace bee {


struct BEE_REFLECT(serializable) Texture
{
    BEE_REFLECT(nonserialized)
    bool loaded { false };
};

struct BEE_REFLECT(serializable) TextureOptions
{
    bool mipmap { false };
};

struct BEE_REFLECT(options = bee::TextureOptions, ext = "png", ext = "jpg", ext = "jpeg", ext = "tga")
TextureCompiler final : public AssetCompiler
{
    AssetCompilerStatus compile(const i32 thread_index, AssetCompilerContext* ctx) override
    {
        log_info("compiling texture...");

        auto& options = ctx->options<TextureOptions>();
        auto stream = ctx->add_artifact();

        stream.write(&options.mipmap, sizeof(bool));

        return AssetCompilerStatus::success;
    }
};


} // namespace bee