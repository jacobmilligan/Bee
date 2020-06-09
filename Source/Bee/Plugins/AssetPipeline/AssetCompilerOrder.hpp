/*
 *  AssetCompiler.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {


enum class AssetCompilerOrder : u32
{
    none    = limits::min<u32>(),
    first   = none + 1,
    last    = limits::max<u32>()
};

constexpr AssetCompilerOrder operator+(const AssetCompilerOrder lhs, const u32 rhs)
{
    return static_cast<AssetCompilerOrder>(static_cast<u32>(lhs) + rhs);
}

constexpr AssetCompilerOrder operator+(const u32 lhs, const AssetCompilerOrder rhs)
{
    return rhs + lhs;
}

constexpr AssetCompilerOrder operator-(const AssetCompilerOrder lhs, const u32 rhs)
{
    return static_cast<AssetCompilerOrder>(static_cast<u32>(lhs) - rhs);
}

constexpr AssetCompilerOrder operator-(const u32 lhs, const AssetCompilerOrder rhs)
{
    return rhs - lhs;
}


} // namespace bee