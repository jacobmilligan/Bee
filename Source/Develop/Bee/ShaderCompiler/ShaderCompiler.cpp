/*
 *  ShaderCompiler.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include <dxc/Support/WinIncludes.h>

#include "Bee/Core/Filesystem.hpp"
#include "Bee/ShaderCompiler/ShaderCompiler.hpp"
#include "Bee/ShaderCompiler/Reflection.hpp"
#include "Bee/Graphics/BSC.hpp"
#include "Bee/Graphics/Shader.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"

#include <dxc/dxcapi.h>

namespace bee {


ShaderCompiler::ShaderCompiler()
    : debug_location_(fs::get_appdata().data_root.join("ShaderCompilerDebug"))
{
    if (!debug_location_.exists())
    {
        fs::mkdir(debug_location_);
    }

    auto dxc_path = fs::get_appdata().binaries_root.join("dxcompiler");
#if BEE_OS_WINDOWS == 1
    dxc_path.set_extension(".dll");
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1

    dxc_dll_ = load_library(dxc_path.c_str());

    auto fp_DxcCreateInstance_ = (DxcCreateInstanceProc)get_library_symbol(dxc_dll_, "DxcCreateInstance");
    BEE_ASSERT(fp_DxcCreateInstance_ != nullptr);

    fp_DxcCreateInstance_(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&dxc_compiler_);
    fp_DxcCreateInstance_(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&dxc_library_);

    BEE_ASSERT(dxc_compiler_ != nullptr && dxc_library_ != nullptr);
}

ShaderCompiler::~ShaderCompiler()
{
    if (dxc_compiler_ != nullptr)
    {
        dxc_compiler_->Release();
        dxc_compiler_ = nullptr;
    }

    if (dxc_library_ != nullptr)
    {
        dxc_library_->Release();
        dxc_library_ = nullptr;
    }
}


BSCTarget platform_to_target(const AssetPlatform platform)
{
    return BSCTarget::none;
}


BEE_TRANSLATION_TABLE(shader_type_short_str, BSCShaderType, const wchar_t*, BSCShaderType::count,
    L"vs",  // vertex
    L"ps"   // fragment
)

BEE_TRANSLATION_TABLE(shader_type_to_gpu_stage, BSCShaderType, ShaderStage, BSCShaderType::count,
    ShaderStage::vertex,  // vertex
    ShaderStage::fragment   // fragment
)

//BEE_TRANSLATION_TABLE(shader_type_execution_model, BSCShaderType, spv::ExecutionModel, BSCShaderType::count,
//  spv::ExecutionModelVertex,    // vertex
//  spv::ExecutionModelFragment   // fragment
//)


AssetCompilerStatus ShaderCompiler::compile(AssetCompilerContext* ctx)
{
    const auto text_src = bsc_parse_source(ctx->location(), ctx->temp_allocator());
    if (text_src.name.empty())
    {
        return AssetCompilerStatus::invalid_source_format;
    }

    BSCTarget targets[static_cast<i32>(BSCTarget::none)];
    int target_count = 0;

    for_each_flag(ctx->platform(), [&](const AssetPlatform platform)
    {
        auto target = BSCTarget::none;

        switch (platform)
        {
            case AssetPlatform::metal:
            {
                target = BSCTarget::MSL;
                break;
            }
            case AssetPlatform::vulkan:
            {
                target = BSCTarget::SPIRV;
                break;
            }
            default: break;
        }

        if (target != BSCTarget::none)
        {
            targets[target_count++] = target;
        }
    });

    BSCModule module{};
    str::format_buffer(module.name, static_array_length(module.name), "%s", text_src.name.c_str());
    module.target = targets[0];
    module.shader_count = text_src.shader_count;

    // Allocate a source blob for parsing with DXC
    CComPtr<IDxcBlobEncoding> source_blob = nullptr;
    dxc_library_->CreateBlobWithEncodingOnHeapCopy(
        text_src.text.c_str(),
        static_cast<UINT32>(text_src.text.size()),
        CP_UTF8,
        &source_blob
    );

    auto module_name = str::to_wchar(text_src.name.view(), ctx->temp_allocator());

    for (int shd = 0; shd < module.shader_count; ++shd)
    {
        const auto shader_type = static_cast<BSCShaderType>(shd);
        const auto profile_str = shader_type_short_str(shader_type);

        // Create a base wchar array for shader profile string without explicit stage specified
        wchar_t shader_profile_str[8];
        swprintf(shader_profile_str, static_array_length(shader_profile_str), L"%ls_6_0", profile_str);

        auto entry_name = str::to_wchar(text_src.shader_entries[shd].view(), ctx->temp_allocator());

        LPCWSTR dxc_args[] = {
            L"-T", shader_profile_str,
            L"-E", entry_name.data(),
            L"-spirv",
            L"-fvk-use-dx-layout",
            L"-fspv-reflect"
        };

        CComPtr<IDxcOperationResult> compilation_result = nullptr;

        // Compile the HLSL to SPIRV
        dxc_compiler_->Compile(
            source_blob,                                // source data
            module_name.data(),                         // name of the source file
            entry_name.data(),                          // the shader stages entry function
            shader_profile_str,                         // shader profile
            dxc_args,                                   // command line args
            static_array_length(dxc_args),
            nullptr,                          // #defines array
            0,                             // #define array count
            nullptr,                   // #include handler
            &compilation_result
        );

        HRESULT compilation_status;
        compilation_result->GetStatus(&compilation_status);

        if (!SUCCEEDED(compilation_status))
        {
            CComPtr<IDxcBlobEncoding> error_blob = nullptr;
            compilation_result->GetErrorBuffer(&error_blob);

            auto error_message = StringView(
                static_cast<const char*>(error_blob->GetBufferPointer()),
                sign_cast<i32>(error_blob->GetBufferSize())
            );

            log_error("ShaderCompiler: DXC: %" BEE_PRIsv "", BEE_FMT_SV(error_message));

            return AssetCompilerStatus::fatal_error;
        }

        // get spirv data
        CComPtr<IDxcBlob> spirv_blob = nullptr;
        compilation_result->GetResult(&spirv_blob);

        if (spirv_blob == nullptr)
        {
            return AssetCompilerStatus::fatal_error;
        }

        const auto reflect_success = reflect_shader(
            &module,
            shader_type,
            static_cast<u8*>(spirv_blob->GetBufferPointer()),
            sign_cast<i32>(spirv_blob->GetBufferSize()),
            ctx->temp_allocator()
        );

        if (!reflect_success)
        {
            log_error("ShaderCompiler: failed to reflect shader");
            return AssetCompilerStatus::fatal_error;
        }
    }

    auto output_stream = ctx->add_artifact();
    StreamSerializer serializer(&output_stream);
    serialize(SerializerMode::writing, &serializer, &module);

    auto options = ctx->options<ShaderCompilerOptions>();
    if (options.output_debug_artifacts)
    {
        const auto debug_location = debug_location_.join(module.name, ctx->temp_allocator()).append_extension("json");
        JSONSerializer debug_serializer(ctx->temp_allocator());
        serialize(SerializerMode::writing, &debug_serializer, &module);
        fs::write(debug_location, debug_serializer.c_str());
    }

    return AssetCompilerStatus::success;
}


} // namespace bee


