/*
 *  ShaderCompiler.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/Core/DynamicLibrary.hpp"

struct IDxcCompiler;
struct IDxcLibrary;

namespace bee {


struct BEE_REFLECT(serializable) ShaderCompilerOptions
{
    bool output_debug_artifacts { false };
};


class BEE_REFLECT(options = bee::ShaderCompilerOptions, ext = ".bsc")
BEE_DEVELOP_API ShaderCompiler final : public AssetCompiler
{
public:
    ShaderCompiler();

    ~ShaderCompiler() override;

    AssetCompilerStatus compile(AssetCompilerContext* ctx) override;

private:
    DynamicLibrary  dxc_dll_;
    IDxcCompiler*   dxc_compiler_ { nullptr };
    IDxcLibrary*    dxc_library_ { nullptr };
    Path            debug_location_;
};


} // namespace bee