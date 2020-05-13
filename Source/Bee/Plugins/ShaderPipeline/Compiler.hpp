/*
 *  Compile.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"

namespace bee {


struct BEE_REFLECT(serializable) ShaderCompilerOptions
{
    bool output_debug_artifacts { false };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Compiler.generated.inl"
#endif // BEE_ENABLE_REFLECTION