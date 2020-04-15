/*
 *  Compile.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/JSON/Reader.hpp"
#include "Bee/Plugins/DefaultAssets/ShaderCompiler/Parse.hpp"

struct IDxcCompiler;
struct IDxcLibrary;

namespace bee {


struct BEE_REFLECT(serializable) ShaderCompilerOptions
{
    bool output_debug_artifacts { false };
};


class BEE_REFLECT(options = bee::ShaderCompilerOptions, ext = ".bsc")
ShaderCompiler final : public AssetCompiler
{
public:
    void init(const i32 thread_count) override;

    void destroy() override;

    AssetCompilerStatus compile(const i32 thread_index, AssetCompilerContext* ctx) override;

private:
    struct PerThread
    {
        IDxcCompiler*   compiler {nullptr };
        IDxcLibrary*    library {nullptr };
        BscParser       parser;

        PerThread(IDxcCompiler* new_compiler, IDxcLibrary* new_library, Allocator* json_allocator)
            : compiler(new_compiler),
              library(new_library)
        {}
    };

    DynamicLibrary          dxc_dll_;
    FixedArray<PerThread>   per_thread_;
};


} // namespace bee