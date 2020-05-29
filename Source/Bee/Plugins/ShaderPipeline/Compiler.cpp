/*
 *  Compile.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include <dxc/Support/WinIncludes.h>

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/Graphics/Mesh.hpp"
#include "Bee/Plugins/ShaderPipeline/Compiler.hpp"
#include "Bee/Plugins/ShaderPipeline/Parse.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"

#include <dxc/dxcapi.h>
#include <spirv_reflect.h>
#include <spirv-tools/libspirv.h>
#include <algorithm>


namespace bee {


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

struct AssetCompilerData
{
    FixedArray<IDxcCompiler*>   dxc_compilers {nullptr };
    FixedArray<IDxcLibrary*>    dxc_libraries {nullptr };
    FixedArray<BscParser>       bsc_parsers;
    DynamicLibrary              dxc_dll;
};


BscTarget platform_to_target(const AssetPlatform platform)
{
    return BscTarget::none;
}


BEE_TRANSLATION_TABLE(shader_type_short_str, ShaderStageIndex, const wchar_t*, ShaderStageIndex::count,
    L"vs",  // vertex
    L"ps",  // fragment
    L"gs",  // geometry
    L"cs"   // compute
)

//BEE_TRANSLATION_TABLE(shader_type_execution_model, BscShaderType, spv::ExecutionModel, BscShaderType::count,
//  spv::ExecutionModelVertex,    // vertex
//  spv::ExecutionModelFragment   // fragment
//)

/*
 ********************************************************
 *
 * `sprv_reflect` - utility function implementations
 *
 ********************************************************
 */

bool spv_reflect_check(const SpvReflectResult result, const char* error_msg)
{
    if (BEE_FAIL_F(result == SPV_REFLECT_RESULT_SUCCESS, "ShaderCompiler: SPIR-V reflection failed with error: %d: %s", result, error_msg))
    {
        return false;
    }
    return true;
}

VertexFormat translate_vertex_format(const SpvReflectFormat format)
{
    switch (format)
    {
        case SPV_REFLECT_FORMAT_UNDEFINED:
            return VertexFormat::unknown;
        case SPV_REFLECT_FORMAT_R32_UINT:
            return VertexFormat::uint1;
        case SPV_REFLECT_FORMAT_R32_SINT:
            return VertexFormat::int1;
        case SPV_REFLECT_FORMAT_R32_SFLOAT:
            return VertexFormat::float1;
        case SPV_REFLECT_FORMAT_R32G32_UINT:
            return VertexFormat::uint2;
        case SPV_REFLECT_FORMAT_R32G32_SINT:
            return VertexFormat::int2;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
            return VertexFormat::float2;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT:
            return VertexFormat::uint3;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT:
            return VertexFormat::int3;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
            return VertexFormat::float3;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
            return VertexFormat::uint4;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
            return VertexFormat::int4;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
            return VertexFormat::float4;
        default:
            break;
    }

    return VertexFormat::invalid;
}


/*
 ****************************
 *
 * Reflection interface
 *
 ****************************
 */
bool reflect_vertex_description(
    SpvReflectShaderModule* reflect_module,
    VertexDescriptor* vertex_desc,
    Allocator* allocator
)
{
    using spv_invar_t = SpvReflectInterfaceVariable*;

    BEE_ASSERT(reflect_module != nullptr);

    // Get the vertex input count
    auto spv_reflect_success = spv_reflect_check(spvReflectEnumerateInputVariables(
        reflect_module,
        &vertex_desc->attribute_count,
        nullptr), "Failed to get vertex input count");

    if (!spv_reflect_success)
    {
        return false;
    }

    // Reflect the actual vertex input data
    auto vertex_inputs = FixedArray<spv_invar_t>::with_size(sign_cast<i32>(vertex_desc->attribute_count), allocator);
    spv_reflect_success = spv_reflect_check(spvReflectEnumerateInputVariables(
        reflect_module,
        &vertex_desc->attribute_count,
        vertex_inputs.data()), "Failed to get vertex input count");

    if (!spv_reflect_success)
    {
        return false;
    }

    /*
     * Sort the vertex inputs by the order defined in the VertexInputAttribute enum and then remap according to sorted index.
     * This ensures that if vertex inputs are moved around in the HLSL code the spirv output always has the same
     * vertex layout (as long as the attributes are the same)
     */
    std::sort(vertex_inputs.begin(), vertex_inputs.end(), [](const spv_invar_t& lhs, const spv_invar_t& rhs)
    {
        return semantic_to_mesh_attribute(lhs->semantic) < semantic_to_mesh_attribute(rhs->semantic);
    });

    // Get input variables
    vertex_desc->layout_count = 1;
    vertex_desc->layouts[0].stride = 0;

    // remap the inputs
    SpvReflectResult reflect_result{};
    const auto location_count = sign_cast<u32>(vertex_inputs.size());
    for (u32 location = 0; location < location_count; ++location)
    {
        spv_reflect_success = spv_reflect_check(spvReflectChangeInputVariableLocation(
            reflect_module,
            vertex_inputs[location],
            location), "Failed to remap vertex input location");

        if (!spv_reflect_success)
        {
            break;
        }

        auto& attr = vertex_desc->attributes[location];
        BEE_ASSERT_F(
            spvReflectGetInputVariableByLocation(reflect_module, location, &reflect_result)->location == location,
            "Vertex input has mismatched location after being remapped"
        );

        attr.layout = 0;
        attr.location = location;
        attr.format = translate_vertex_format(vertex_inputs[location]->format);
        attr.offset = vertex_desc->layouts[0].stride;
        if (attr.format == VertexFormat::invalid)
        {
            log_error("ShaderCompiler: Unsupported input type detected");
            return false;
        }

        if (attr.format == VertexFormat::unknown)
        {
            log_error("ShaderCompiler: Unable to convert vertex format of input to a valid Skyrocket format");
            return false;
        }

        vertex_desc->layouts[0].stride += vertex_format_size(attr.format);
    }

    return true;
}


bool reflect_resources(Shader::SubShader* subshader, SpvReflectShaderModule* reflect_module, Allocator* allocator)
{
    u32 count = 0;
    auto success = spv_reflect_check(spvReflectEnumerateDescriptorBindings(
        reflect_module, &count, nullptr
    ), "Failed to reflect resources");

    if (!success)
    {
        return false;
    }

    SpvReflectDescriptorBinding** bindings = nullptr;

    if (count > 0)
    {
        bindings = BEE_ALLOCA_ARRAY(SpvReflectDescriptorBinding*, count);

        success = spv_reflect_check(spvReflectEnumerateDescriptorBindings(
            reflect_module, &count, bindings
        ), "Failed to reflect resources");

        if (!success)
        {
            return false;
        }
    }

    if (bindings == nullptr)
    {
        return true;
    }

//    for (u32 i = 0; i < count; ++i)
//    {
//        subshader->resource_layout_count
//    }
    return true;
}

Shader::Range reflect_subshader(Shader* shader, const i32 subshader_index, VertexDescriptor* reflected_vertex_descriptor, const i32 stage_index, const Span<const u8>& spirv, Allocator* allocator = system_allocator())
{
    BEE_ASSERT(shader != nullptr);

    SpvReflectShaderModule reflect_module{};
    auto spv_reflect_success = spv_reflect_check(spvReflectCreateShaderModule(
        spirv.size(), spirv.data(), &reflect_module
    ), "Failed to create shader module");

    BEE_ASSERT_F(stage_index >= 0 && stage_index < gpu_shader_stage_count, "Failed to reflect shader: Invalid shader type");

    if (!spv_reflect_success)
    {
        return Shader::Range{};
    }

    if (!reflect_resources(&shader->subshaders[subshader_index], &reflect_module, allocator))
    {
        return Shader::Range{};
    }

    // Reflect vertex inputs if we're reflecting a vertex shader
    if (stage_index == static_cast<i32>(ShaderStageIndex::vertex))
    {
        const auto success = reflect_vertex_description(&reflect_module, reflected_vertex_descriptor, allocator);
        if (!success)
        {
            return Shader::Range{};
        }
    }

    // spvReflectGetCode returns a *word* array but spvReflectGetCodeSize returns the size in *bytes*
    const auto spv_code_size = sign_cast<i32>(spvReflectGetCodeSize(&reflect_module));
    const auto* spv_code = reinterpret_cast<const u8*>(spvReflectGetCode(&reflect_module));

    // Copy reflected to result
    const auto range = shader->add_code(spv_code, spv_code_size);

    spvReflectDestroyShaderModule(&reflect_module);

    return range;
}

/*
 **********************************************
 *
 * Compiles a single subshader - a single
 * `Shader` structure specified within a
 * larger .bsc file module
 *
 **********************************************
 */
AssetCompilerStatus compile_subshader(AssetCompilerContext* ctx, IDxcCompiler* compiler, IDxcLibrary* library, Shader* shader, const i32 subshader_index, const StringView& code, VertexDescriptor* reflected_vertex_descriptor)
{
    auto& subshader = shader->subshaders[subshader_index];

    CComPtr<IDxcBlobEncoding> source_blob = nullptr;
    library->CreateBlobWithEncodingOnHeapCopy(
        code.c_str(),
        static_cast<UINT32>(code.size()),
        CP_UTF8,
        &source_blob
    );

    auto module_name = str::to_wchar(subshader.name.view(), ctx->temp_allocator());

    for (int stage_index = 0; stage_index < gpu_shader_stage_count; ++stage_index)
    {
        // empty name == unused stage
        if (subshader.stage_entries[stage_index].empty())
        {
            subshader.stage_code_ranges[stage_index] = Shader::Range{};
            continue;
        }

        const auto* profile_str = shader_type_short_str(static_cast<ShaderStageIndex>(stage_index));

        // Create a base wchar array for shader profile string without explicit stage specified
        wchar_t shader_profile_str[8];
        swprintf(shader_profile_str, static_array_length(shader_profile_str), L"%ls_6_0", profile_str);

        auto entry_name = str::to_wchar(subshader.stage_entries[stage_index].view(), ctx->temp_allocator());

        LPCWSTR dxc_args[] = {
            L"-T", shader_profile_str,
            L"-E", entry_name.data(),
            L"-spirv",
            L"-fvk-use-dx-layout",
            L"-fspv-reflect"
        };

        DxcDefine dxc_defines[] = {
            { L"BEE_BINDING(b, s)", L"[[vk::binding(b, s)]]" }
        };

        CComPtr<IDxcOperationResult> compilation_result = nullptr;

        // Compile the HLSL to SPIRV
        compiler->Compile(
            source_blob,                                // source data
            module_name.data(),                         // name of the source file
            entry_name.data(),                          // the shader stages entry function
            shader_profile_str,                         // shader profile
            dxc_args,                                   // argv
            static_array_length(dxc_args),              // argc
            dxc_defines,                                // #defines array
            static_array_length(dxc_defines),           // #define array count
            nullptr,                                    // #include handler
            &compilation_result
        );

        HRESULT compilation_status = 0;
        compilation_result->GetStatus(&compilation_status);

        if (!SUCCEEDED(compilation_status))
        {
            CComPtr<IDxcBlobEncoding> error_blob = nullptr;
            compilation_result->GetErrorBuffer(&error_blob);

            auto error_message = StringView(
                static_cast<const char*>(error_blob->GetBufferPointer()),
                sign_cast<i32>(error_blob->GetBufferSize())
            );

            log_error("DXC error: %" BEE_PRIsv "", BEE_FMT_SV(error_message));

            return AssetCompilerStatus::fatal_error;
        }

        // get spirv data
        CComPtr<IDxcBlob> spirv_blob = nullptr;
        compilation_result->GetResult(&spirv_blob);

        if (spirv_blob == nullptr)
        {
            return AssetCompilerStatus::fatal_error;
        }

        subshader.stage_code_ranges[stage_index] = reflect_subshader(
            shader,
            subshader_index,
            reflected_vertex_descriptor,
            stage_index,
            { static_cast<u8*>(spirv_blob->GetBufferPointer()), sign_cast<i32>(spirv_blob->GetBufferSize()) },
            ctx->temp_allocator()
        );

        if (subshader.stage_code_ranges[stage_index].size < 0)
        {
            log_error("ShaderCompiler: failed to reflect shader");
            return AssetCompilerStatus::fatal_error;
        }
    }

    return AssetCompilerStatus::success;
}

/*
 ***********************************
 *
 * Asset compiler - implementation
 *
 ***********************************
 */
void init_shader_compiler(AssetCompilerData* data, const i32 thread_count)
{
    data->dxc_compilers.resize(thread_count);
    data->dxc_libraries.resize(thread_count);
    data->bsc_parsers.resize(thread_count);

    auto dxc_path = fs::get_root_dirs().binaries_root.join("dxcompiler");
#if BEE_OS_WINDOWS == 1
    dxc_path.set_extension(".dll");
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1

    data->dxc_dll = load_library(dxc_path.c_str());

    auto fp_DxcCreateInstance_ = (DxcCreateInstanceProc)get_library_symbol(data->dxc_dll, "DxcCreateInstance");
    BEE_ASSERT(fp_DxcCreateInstance_ != nullptr);

    // Create one DXC context per thread for asset compile jobs
    for (int i = 0; i < thread_count; ++i)
    {
        IDxcCompiler* compiler = nullptr;
        IDxcLibrary* library = nullptr;
        fp_DxcCreateInstance_(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&compiler);
        fp_DxcCreateInstance_(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&library);

        BEE_ASSERT(compiler != nullptr && library != nullptr);

        data->dxc_compilers[i] = compiler;
        data->dxc_libraries[i] = library;
    }
}

void destroy_shader_compiler(AssetCompilerData* data)
{
    for (int i = 0; i < data->dxc_compilers.size(); ++i)
    {
        auto& compiler = data->dxc_compilers[i];
        auto& library = data->dxc_libraries[i];

        if (compiler != nullptr)
        {
            compiler->Release();
            compiler = nullptr;
        }

        if (library != nullptr)
        {
            library->Release();
            library = nullptr;
        }
    }

    data->dxc_compilers.clear();
    data->dxc_libraries.clear();
    data->bsc_parsers.clear();

    if (data->dxc_dll.handle != nullptr)
    {
        unload_library(data->dxc_dll);
        data->dxc_dll.handle = nullptr;
    }
}

AssetCompilerStatus compile_shader(AssetCompilerData* data, const i32 thread_index, AssetCompilerContext* ctx)
{
    const auto src_path = Path(ctx->location(), ctx->temp_allocator());
    auto& bsc = data->bsc_parsers[thread_index];
    auto file_contents = fs::read(src_path, ctx->temp_allocator());

    // Parse the file into a BscModule
    BscModule asset(ctx->temp_allocator());
    if (!bsc.parse(file_contents.view(), &asset))
    {
        const auto error = bsc.get_error().to_string(ctx->temp_allocator());
        log_error("%s", error.c_str());
        return AssetCompilerStatus::invalid_source_format;
    }

    Shader result(ctx->temp_allocator());
    const auto resolve_error = bsc_resolve_module(asset, &result);
    if (!resolve_error)
    {
        log_error("%s", resolve_error.to_string(ctx->temp_allocator()).c_str());
        return AssetCompilerStatus::invalid_source_format;
    }

    BscTarget targets[static_cast<i32>(BscTarget::none)];
    int target_count = 0;

    // figure out which backends we need to target based off the asset platform
    for_each_flag(ctx->platform(), [&](const AssetPlatform platform)
    {
        auto target = BscTarget::none;

        switch (platform)
        {
            case AssetPlatform::metal:
            {
                target = BscTarget::msl;
                break;
            }
            case AssetPlatform::vulkan:
            {
                target = BscTarget::spirv;
                break;
            }
            default: break;
        }

        if (target != BscTarget::none)
        {
            targets[target_count++] = target;
        }
    });

    auto reflected_vertex_descs = FixedArray<VertexDescriptor>::with_size(result.subshaders.size(), ctx->temp_allocator());

    for (int index = 0; index < result.subshaders.size(); ++index)
    {
        const auto status = compile_subshader(
            ctx,
            data->dxc_compilers[thread_index],
            data->dxc_libraries[thread_index],
            &result,
            index,
            asset.shaders[index].data.code,
            &reflected_vertex_descs[index]
        );

        if (status != AssetCompilerStatus::success)
        {
            return status;
        }

        ++index;
    }

    auto& shader_artifact = ctx->add_artifact<Shader>();
    BinarySerializer serializer(&shader_artifact);
    serialize(SerializerMode::writing, &serializer, &result, ctx->temp_allocator());

//    const auto& options = ctx->options<ShaderCompilerOptions>();
    if (true /*options.output_debug_artifacts*/)
    {
        auto* spv_context = spvContextCreate(SPV_ENV_VULKAN_1_1);
        spv_text spv_text_dest = nullptr;
        spv_diagnostic spv_diagnostic_dest = nullptr;

        String debug_output(ctx->temp_allocator());
        io::StringStream debug_stream(&debug_output);

        debug_stream.write_fmt("// original file: %s\n\n", src_path.c_str());

        for (auto& subshader : result.subshaders)
        {
            debug_stream.write_fmt("// Subshader %s\n\n", subshader.name.c_str());

            for (int stage = 0; stage < static_array_length(subshader.stage_entries); ++stage)
            {
                auto& entry = subshader.stage_entries[stage];
                if (entry.empty())
                {
                    continue;
                }

                const auto& code_range = subshader.stage_code_ranges[stage];
                // Translate the SPIR-V to string text format for debugging
                auto spv_error = spvBinaryToText(
                    spv_context,
                    reinterpret_cast<const u32*>(result.code.data() + code_range.offset),
                    static_cast<size_t>(code_range.size / sizeof(u32)),
                    SPV_BINARY_TO_TEXT_OPTION_NONE | SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES,
                    &spv_text_dest,
                    &spv_diagnostic_dest
                );

                debug_stream.write("// Stage: ");
                enum_to_string(&debug_stream, static_cast<ShaderStageIndex>(stage));
                debug_stream.write("\n\n");

                if (spv_error != SPV_SUCCESS)
                {
                    log_error("ShaderCompiler failed to convert spirv IR to text: %s", spv_diagnostic_dest->error);
                }
                else
                {
                    debug_stream.write(spv_text_dest->str, spv_text_dest->length);
                    debug_stream.write("\n\n");
                }
            }
        }

        auto& debug_artifact = ctx->add_artifact<String>();
        serializer.reset(&debug_artifact);
        serialize(SerializerMode::writing, &serializer, &debug_output);
    }

    ctx->set_main(shader_artifact);
    return AssetCompilerStatus::success;
}

const char* get_shader_compiler_name()
{
    return "Bee Shader Compiler";
}

TypeRef shader_compiler_settings_type()
{
    return get_type<ShaderCompilerOptions>();
}

Span<const char* const> shader_compiler_file_type()
{
    static constexpr const char* filetypes[] = { ".bsc" };
    return Span<const char* const>(filetypes);
}


static AssetCompiler g_compiler{};

void load_compiler(bee::PluginRegistry* registry, const bee::PluginState state)
{
    if (!registry->has_module(BEE_ASSET_PIPELINE_MODULE_NAME))
    {
        return;
    }

    g_compiler.data = registry->get_or_create_persistent<AssetCompilerData>("BeeShaderCompilerData");
    g_compiler.init = init_shader_compiler;
    g_compiler.destroy = destroy_shader_compiler;
    g_compiler.compile = compile_shader;
    g_compiler.get_name = get_shader_compiler_name;
    g_compiler.settings_type = shader_compiler_settings_type;
    g_compiler.supported_file_types = shader_compiler_file_type;

    auto* asset_pipeline = registry->get_module<AssetPipelineModule>(BEE_ASSET_PIPELINE_MODULE_NAME);

    if (asset_pipeline->register_compiler == nullptr)
    {
        return;
    }

    if (state == bee::PluginState::loading)
    {
        asset_pipeline->register_compiler(&g_compiler);
    }
    else
    {
        asset_pipeline->unregister_compiler(&g_compiler);
    }
}


} // namespace bee


