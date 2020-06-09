/*
 *  Compile.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"

#include "Bee/Plugins/AssetPipeline/AssetCompilerOrder.hpp"


namespace bee {


static constexpr auto shader_compiler_order = AssetCompilerOrder::first;
static constexpr auto material_compiler_order = shader_compiler_order + 1;


struct BEE_REFLECT(serializable) ShaderCompilerSettings
{
    bool output_debug_artifacts { false };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Compiler.generated.inl"
#endif // BEE_ENABLE_REFLECTION