/*
 *  AssetPlatform.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"

namespace bee {


BEE_REFLECTED_FLAGS(AssetPlatform, u32, serializable)
{
    unknown = 0u,
    windows = 1u << 0u,
    macos   = 1u << 1u,
    linux   = 1u << 2u,
    metal   = 1u << 3u,
    vulkan  = 1u << 4u,
};

constexpr AssetPlatform current_asset_os()
{
#if BEE_OS_WINDOWS == 1
    return AssetPlatform::windows;
#elif BEE_OS_MACOS == 1
    return AssetPlatform::macos;
#elif BEE_OS_LINUX == 1
    return AssetPlatform::linux;
#endif // BEE_OS_*
}

constexpr AssetPlatform current_asset_gfx_backend()
{
#if BEE_CONFIG_METAL_BACKEND == 1
    return AssetPlatform::metal;
#elif BEE_CONFIG_VULKAN_BACKEND == 1
    return AssetPlatform::vulkan;
#else
    return AssetPlatform::unknown
#endif // BEE_CONFIG_*_BACKEND
}

constexpr AssetPlatform default_asset_platform = current_asset_os() | current_asset_gfx_backend();


} // namespace bee