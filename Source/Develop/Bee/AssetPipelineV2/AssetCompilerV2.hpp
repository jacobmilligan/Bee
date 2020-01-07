/*
 *  AssetCompilerV2.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"

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
    invalid_source_format,
    unknown
};


struct AssetCompilerResult
{
    AssetCompilerStatus status { AssetCompilerStatus::unknown };
    const Type*         compiled_type { nullptr };
};



struct BEE_DEVELOP_API AssetCompiler
{
    virtual ~AssetCompiler() = default;
    virtual AssetCompilerResult compile()
};


} // namespace bee