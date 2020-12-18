/*
 *  Compiler.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include <dxc/Support/WinIncludes.h>

#include "Bee/ShaderPipeline/Compiler.hpp"
#include "Bee/ShaderPipeline/Parser/Parse.hpp"
#include "Bee/ShaderPipeline/Cache.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Bit.hpp"

#include <dxc/dxcapi.h>
#include <spirv_reflect.h>
#include <spirv-tools/libspirv.h>

#include <algorithm>


namespace bee {


struct ThreadData
{
    IDxcCompiler*   compiler {nullptr };
    IDxcLibrary*    library {nullptr };
    BscParser       parser;
    LinearAllocator temp_allocator;

    ThreadData()
        : temp_allocator(megabytes(16))
    {}
};

struct ShaderCompiler
{
    DynamicLibrary          dxc;
    FixedArray<ThreadData>  thread_data;
};

static ShaderCompiler*      g_compiler = nullptr;
extern ShaderCacheModule    g_shader_cache;


/*
 **********************************
 *
 * Shader compiler implementation
 *
 **********************************
 */
struct CompilationContext
{
    using resource_layouts_t = ResourceLayoutDescriptor[BEE_GPU_MAX_RESOURCE_LAYOUTS];

    ThreadData*                     thread { nullptr };
    ShaderFile*                     shader { nullptr };
    FixedArray<VertexDescriptor>    vertex_descriptors;
    FixedArray<resource_layouts_t>  resource_layouts;

    CompilationContext(ThreadData* thread_data, ShaderFile* shader_file)
        : thread(thread_data),
          shader(shader_file)
    {
        vertex_descriptors = FixedArray<VertexDescriptor>::with_size(shader->subshaders.size(), &thread_data->temp_allocator);
        resource_layouts = FixedArray<resource_layouts_t>::with_size(shader->subshaders.size(), &thread_data->temp_allocator);
    }

    inline Allocator* temp_allocator()
    {
        return &thread->temp_allocator;
    }
};

bool init()
{
    g_compiler->thread_data.resize(job_system_worker_count());

    auto dxc_path = fs::get_root_dirs().binaries_root.join("Plugins").join("dxcompiler");
#if BEE_OS_WINDOWS == 1
    dxc_path.set_extension(".dll");
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1

    g_compiler->dxc = load_library(dxc_path.c_str());

    auto fp_DxcCreateInstance_ = (DxcCreateInstanceProc)get_library_symbol(g_compiler->dxc, "DxcCreateInstance");
    BEE_ASSERT(fp_DxcCreateInstance_ != nullptr);

    // Create one DXC context per thread for asset compile jobs
    for (auto& thread : g_compiler->thread_data)
    {
        IDxcCompiler* compiler = nullptr;
        IDxcLibrary* library = nullptr;
        fp_DxcCreateInstance_(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&compiler);
        fp_DxcCreateInstance_(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&library);

        BEE_ASSERT(compiler != nullptr && library != nullptr);

        thread.compiler = compiler;
        thread.library = library;
    }

    return true;
}

void destroy()
{
    for (auto& thread : g_compiler->thread_data)
    {
        if (thread.compiler != nullptr)
        {
            thread.compiler->Release();
            thread.compiler = nullptr;
        }

        if (thread.library != nullptr)
        {
            thread.library->Release();
            thread.library = nullptr;
        }
    }

    g_compiler->thread_data.clear();

    if (g_compiler->dxc.handle != nullptr)
    {
        unload_library(g_compiler->dxc);
        g_compiler->dxc.handle = nullptr;
    }
}

BEE_TRANSLATION_TABLE(shader_type_short_str, ShaderStageIndex, const wchar_t*, ShaderStageIndex::count,
    L"vs",  // vertex
    L"ps",  // fragment
    L"gs",  // geometry
    L"cs"   // compute
)

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
bool reflect_vertex_description(CompilationContext* ctx, SpvReflectShaderModule* reflect_module, const i32 index)
{
    using spv_invar_t = SpvReflectInterfaceVariable*;

    BEE_ASSERT(reflect_module != nullptr);

    auto& vertex_desc = ctx->vertex_descriptors[index];
    // Get the vertex input count
    auto spv_reflect_success = spv_reflect_check(spvReflectEnumerateInputVariables(
        reflect_module,
        &vertex_desc.attributes.size,
        nullptr), "Failed to get vertex input count");

    if (!spv_reflect_success)
    {
        return false;
    }

    // Reflect the actual vertex input data
    auto vertex_inputs = FixedArray<spv_invar_t>::with_size(sign_cast<i32>(vertex_desc.attributes.size), ctx->temp_allocator());
    spv_reflect_success = spv_reflect_check(spvReflectEnumerateInputVariables(
        reflect_module,
        &vertex_desc.attributes.size,
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
//    std::sort(vertex_inputs.begin(), vertex_inputs.end(), [](const spv_invar_t& lhs, const spv_invar_t& rhs)
//    {
//        return semantic_to_mesh_attribute(lhs->semantic) < semantic_to_mesh_attribute(rhs->semantic);
//    });

    // Get input variables
    vertex_desc.layouts.size = vertex_inputs.empty() ? 0 : 1;
    vertex_desc.layouts[0].stride = 0;

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

        auto& attr = vertex_desc.attributes[location];
        BEE_ASSERT_F(
            spvReflectGetInputVariableByLocation(reflect_module, location, &reflect_result)->location == location,
            "Vertex input has mismatched location after being remapped"
        );

        attr.layout = 0;
        attr.location = location;
        attr.format = translate_vertex_format(vertex_inputs[location]->format);
        attr.offset = vertex_desc.layouts[0].stride;
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

        vertex_desc.layouts[0].stride += vertex_format_size(attr.format);
    }

    return true;
}

BEE_TRANSLATION_TABLE(convert_descriptor_type, SpvReflectDescriptorType, ResourceBindingType, SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1,
    ResourceBindingType::sampler,                   // SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER
    ResourceBindingType::combined_texture_sampler,  // SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    ResourceBindingType::sampled_texture,           // SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    ResourceBindingType::storage_texture,           // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE
    ResourceBindingType::uniform_texel_buffer,      // SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
    ResourceBindingType::storage_texel_buffer,      // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
    ResourceBindingType::uniform_buffer,            // SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    ResourceBindingType::storage_buffer,            // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER
    ResourceBindingType::dynamic_uniform_buffer,    // SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    ResourceBindingType::dynamic_storage_buffer,    // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
    ResourceBindingType::input_attachment,          // SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
);


bool reflect_resources(CompilationContext* ctx, SpvReflectShaderModule* reflect_module, const i32 subshader, const ShaderStageIndex stage_index)
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

    auto& resource_layouts = ctx->resource_layouts[subshader];
    for (u32 i = 0; i < count; ++i)
    {
        auto& layout = resource_layouts[bindings[i]->set];
        auto& binding = layout.resources[layout.resources.size];
        binding.binding = bindings[i]->binding;
        binding.type = convert_descriptor_type(bindings[i]->descriptor_type);;
        binding.element_count = bindings[i]->count;
        binding.shader_stages = stage_index.to_flags();
        ++layout.resources.size;
    }

    // sort the descriptors by their binding ID for validation later
    for (auto& layout : resource_layouts)
    {
        if (layout.resources.size > 0)
        {
            std::sort(layout.resources.begin(), layout.resources.end(), [&](const ResourceDescriptor& lhs, const ResourceDescriptor& rhs)
            {
                return lhs.binding < rhs.binding;
            });
        }
    }

    return true;
}

ShaderFile::Range reflect_subshader(CompilationContext* ctx, const i32 subshader_index, const ShaderStageIndex stage_index, const Span<const u8>& spirv)
{
    BEE_ASSERT(ctx != nullptr && ctx->shader != nullptr);

    SpvReflectShaderModule reflect_module{};
    auto spv_reflect_success = spv_reflect_check(spvReflectCreateShaderModule(
        spirv.size(), spirv.data(), &reflect_module
    ), "Failed to create shader module");

    BEE_ASSERT_F(stage_index >= ShaderStageIndex::vertex && stage_index < ShaderStageIndex::count, "Failed to reflect shader: Invalid shader type");

    if (!spv_reflect_success)
    {
        return ShaderFile::Range{};
    }

    if (!reflect_resources(ctx, &reflect_module, subshader_index, stage_index))
    {
        return ShaderFile::Range{};
    }

    // Reflect vertex inputs if we're reflecting a vertex shader
    if (stage_index == ShaderStageIndex::vertex)
    {
        const auto success = reflect_vertex_description(ctx, &reflect_module, subshader_index);
        if (!success)
        {
            return ShaderFile::Range{};
        }
    }

    // spvReflectGetCode returns a *word* array but spvReflectGetCodeSize returns the size in *bytes*
    const auto spv_code_size = sign_cast<i32>(spvReflectGetCodeSize(&reflect_module));
    const auto* spv_code = reinterpret_cast<const u8*>(spvReflectGetCode(&reflect_module));

    // Copy reflected to result
    const auto range = ctx->shader->add_code(spv_code, spv_code_size);

    spvReflectDestroyShaderModule(&reflect_module);

    return range;
}

static bool merge_resource_layouts(const StringView& source_path, PipelineStateCreateInfo* info, const CompilationContext::resource_layouts_t& layouts)
{
    const u32 layout_max = sign_cast<u32>(math::min(sign_cast<i32>(info->resource_layouts.capacity), static_array_length(layouts)));
    for (u32 layout = 0; layout < layout_max; ++layout)
    {
        auto& pipeline_layout = info->resource_layouts[layout];
        const auto& shader_layout = layouts[layout];

        if (shader_layout.resources.size == 0)
        {
            continue;
        }

        bool is_new_layout = false;

        // try and merge each binding - the pipelines resource layout will contain 'unknown' if a slot is unused
        for (u32 i = 0; i < shader_layout.resources.size; ++i)
        {
            const auto& resource = shader_layout.resources[i];
            if (pipeline_layout.resources[resource.binding].type == ResourceBindingType::unknown)
            {
                if (pipeline_layout.resources.size == 0 && !is_new_layout)
                {
                    is_new_layout = true;
                }

                // success we can merge this binding in
                ++pipeline_layout.resources.size;
                pipeline_layout.resources[resource.binding] = shader_layout.resources[i];
            }
            else
            {
                /*
                 * the binding is already assigned so we need to validate that the shaders binding is compatible
                 * with the pipeline states previously assigned binding (from a different shader). This ensures
                 * all shaders in a pipeline are compatible with each other
                 */
                if (pipeline_layout.resources[resource.binding] != shader_layout.resources[i])
                {
                    log_error(
                        "Cannot compile %" BEE_PRIsv ": resources are incompatible at binding %u, layout %u",
                        BEE_FMT_SV(source_path), resource.binding, layout
                    );
                    return false;
                }

                // shader bindings are compatible so continue
            }
        }

        if (is_new_layout)
        {
            info->resource_layouts.size = math::max(info->resource_layouts.size + 1, layout);
        }
    }

    return true;
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
ShaderCompilerResult compile_subshader(CompilationContext* ctx, const i32 subshader_index, const StringView& code)
{
    auto& subshader = ctx->shader->subshaders[subshader_index];

    CComPtr<IDxcBlobEncoding> source_blob = nullptr;
    ctx->thread->library->CreateBlobWithEncodingOnHeapCopy(
        code.c_str(),
        static_cast<UINT32>(code.size()),
        CP_UTF8,
        &source_blob
    );

    auto module_name = str::to_wchar(subshader.name.view(), ctx->temp_allocator());

    for (int stage_index = 0; stage_index < ShaderStageIndex::count; ++stage_index)
    {
        // empty name == unused stage
        if (subshader.stage_entries[stage_index].empty())
        {
            subshader.stage_code_ranges[stage_index] = ShaderFile::Range{};
            continue;
        }

        const auto* profile_str = shader_type_short_str(stage_index);

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
        ctx->thread->compiler->Compile(
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

            return ShaderCompilerResult::dxc_compilation_failed;
        }

        // get spirv data
        CComPtr<IDxcBlob> spirv_blob = nullptr;
        compilation_result->GetResult(&spirv_blob);

        if (spirv_blob == nullptr)
        {
            return ShaderCompilerResult::spirv_failed_to_generate;
        }

        subshader.stage_code_ranges[stage_index] = reflect_subshader(
            ctx,
            subshader_index,
            static_cast<ShaderStageIndex>(stage_index),
            { static_cast<u8*>(spirv_blob->GetBufferPointer()), sign_cast<i32>(spirv_blob->GetBufferSize()) }
        );

        if (subshader.stage_code_ranges[stage_index].size < 0)
        {
            log_error("ShaderCompiler: failed to reflect shader");
            return ShaderCompilerResult::reflection_failed;
        }
    }

    return ShaderCompilerResult::success;
}

static ShaderCompilerResult compile_shader_file(ShaderCache* cache, const StringView& source_path, const StringView& src, const ShaderTarget target_flags, DynamicArray<ShaderPipelineHandle>* dst)
{
    auto& thread = g_compiler->thread_data[job_worker_id()];

    if (dst != nullptr)
    {
        dst->clear();
    }

    // Parse the file into a BscModule
    BscModule asset(&thread.temp_allocator);
    if (!thread.parser.parse(src, &asset))
    {
        const auto error = thread.parser.get_error().to_string(&thread.temp_allocator);
        log_error("%s", error.c_str());
        return ShaderCompilerResult::invalid_source;
    }

    ShaderFile result(&thread.temp_allocator);
    const auto resolve_error = bsc_resolve_module(asset, &result);
    if (!resolve_error)
    {
        log_error("%s", resolve_error.to_string(&thread.temp_allocator).c_str());
        return ShaderCompilerResult::invalid_source;
    }

    CompilationContext ctx(&thread, &result);

    for (int index = 0; index < result.subshaders.size(); ++index)
    {
        const auto status = compile_subshader(&ctx, index, asset.shaders[index].data.code);

        if (status != ShaderCompilerResult::success)
        {
            return status;
        }

        ++index;
    }

    i32 update_freq_validation[BEE_GPU_MAX_RESOURCE_LAYOUTS] { -1 };

    RenderPassCreateInfo render_pass_info{};
    ShaderPipelineDescriptor desc{};

    // Setup each pipelines create info
    for (auto& pipeline : result.pipelines)
    {
        // Assign the reflected vertex description
        const auto vert_index = underlying_t(ShaderStageIndex::vertex);
        pipeline.info.vertex_description = ctx.vertex_descriptors[pipeline.shaders[vert_index]];

        // validate the resource layouts from the shaders and assign to the pipeline
        for (auto& shader_index : pipeline.shaders)
        {
            if (shader_index < 0)
            {
                continue;
            }

            // validate update frequencies
            auto& subshader = result.subshaders[shader_index];
            for (int i = 0; i < subshader.resource_layout_count; ++i)
            {
                const auto freq = underlying_t(subshader.resource_layout_frequencies[i]);
                const auto validation_freq = update_freq_validation[i];
                if (validation_freq >= 0 && validation_freq != freq)
                {
                    log_error(
                        "Cannot compile %" BEE_PRIsv ": shaders have incompatible resource layouts at index %d",
                        BEE_FMT_SV(source_path), i
                    );
                    return ShaderCompilerResult::incompatible_resource_layouts;
                }
            }

            auto& shader_resources = ctx.resource_layouts[shader_index];
            if (!merge_resource_layouts(source_path, &pipeline.info, shader_resources))
            {
                return ShaderCompilerResult::incompatible_resource_layouts;
            }
        }

        // sort the resource layouts so there's no gaps in the bindings
        for (auto& layout : pipeline.info.resource_layouts)
        {
            auto* begin = layout.resources.begin();
            auto* end = layout.resources.end();
            std::sort(begin, end, [&](const ResourceDescriptor& lhs, const ResourceDescriptor& rhs)
            {
                if (lhs.type == ResourceBindingType::unknown || rhs.type == ResourceBindingType::unknown)
                {
                    return lhs.type < rhs.type;
                }
                return lhs.binding < rhs.binding;
            });
        }

        /*
         * Success!
         * Create the shader pipeline object in the cache
         */
        result.get_shader_pipeline_descriptor(pipeline, &desc);
        const auto handle = g_shader_cache.add_shader(cache, desc);
        if (!handle.is_valid())
        {
            return ShaderCompilerResult::shader_cache_create_failed;
        }

        if (dst != nullptr)
        {
            dst->push_back(handle);
        }
    }

#if 0
    if ((target_flags & ShaderTarget::spirv_debug) != ShaderTarget::unknown)
    {
        auto* spv_context = spvContextCreate(SPV_ENV_VULKAN_1_1);
        spv_text spv_text_dest = nullptr;
        spv_diagnostic spv_diagnostic_dest = nullptr;

        String debug_output(&thread.temp_allocator);
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
#endif // 0

    return ShaderCompilerResult::success;
}

ShaderCompilerResult compile_shaders(ShaderCache* cache, const i32 count, const StringView* source_paths, const StringView* sources, const ShaderTarget* targets, DynamicArray<ShaderPipelineHandle>* dst)
{
    auto& thread = g_compiler->thread_data[job_worker_id()];

    for (int i = 0; i < count; ++i)
    {
        const auto result = compile_shader_file(cache, source_paths[i], sources[i], targets[i], dst);

        // reset the temp allocator automatically after each compilation
        thread.temp_allocator.reset();

        if (result != ShaderCompilerResult::success)
        {
            return result;
        }
    }

    return ShaderCompilerResult::success;
}

ShaderCompilerResult compile_shader(ShaderCache* cache, const StringView& source_path, const StringView& source, const ShaderTarget target, DynamicArray<ShaderPipelineHandle>* dst)
{
    return compile_shaders(cache, 1, &source_path, &source, &target, dst);
}



/*
 **********************************
 *
 * Plugin loading
 *
 **********************************
 */
static ShaderCompilerModule g_module{};

void load_compiler_module(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_compiler = loader->get_static<ShaderCompiler>("Bee.ShaderCompiler");

    g_module.init = init;
    g_module.destroy = destroy;
    g_module.compile_shaders = compile_shaders;
    g_module.compile_shader = compile_shader;

    loader->set_module(BEE_SHADER_COMPILER_MODULE_NAME, &g_module, state);
}


} // namespace bee