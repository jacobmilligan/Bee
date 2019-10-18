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


struct ShaderCompilerSettings
{
    bool output_debug_artifacts { false };
};


BEE_SERIALIZE(1, ShaderCompilerSettings)
{
    BEE_ADD_FIELD(1, output_debug_artifacts);
}


class BEE_DEVELOP_API ShaderCompiler final : public AssetCompiler
{
public:
    static constexpr const char* supported_file_types[] = { ".bsc" };

    ShaderCompiler();

    ~ShaderCompiler() override;

    AssetCompilerResult compile(AssetCompileContext* ctx) override;

private:
    DynamicLibrary  dxc_dll_;
    IDxcCompiler*   dxc_compiler_ { nullptr };
    IDxcLibrary*    dxc_library_ { nullptr };
    Path            debug_location_;
};


} // namespace bee