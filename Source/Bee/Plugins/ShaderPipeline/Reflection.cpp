/*
 *  Reflection.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderCompiler/Reflection.hpp"
#include "Bee/Graphics/Mesh.hpp"

#include <spirv_reflect.h>

#include <algorithm>


namespace bee {


/*
 **************************************
 *
 * `sprv_reflect` - utility functions
 *
 **************************************
 */

using spv_invar_t = SpvReflectInterfaceVariable*;

// Executes a spirv_reflect function and checks the result - logging an error and returning false if the call failed
bool spv_reflect_check(const SpvReflectResult result, const char* error_msg);

// Translates a SpvReflect vertex format into a Skyrocket one
VertexFormat translate_vertex_format(const SpvReflectFormat format);


/*
 **************************************
 *
 * `reflect_stage` - implementation
 *
 **************************************
 */
bool reflect_vertex_description(
    SpvReflectShaderModule* reflect_module,
    VertexDescriptor* vertex_desc,
    Allocator* allocator
);

//bool reflect_resources(
//    ShaderStage stage,
//    SpvReflectShaderModule* reflect_module,
//    DynamicArray<ReflectedResource>* descriptors,
//    Allocator* allocator
//);

bool reflect_shader(BscModule* module, BscShaderType type, const u8* spirv, i32 spirv_length, Allocator* allocator)
{
    BEE_ASSERT(module != nullptr);

    SpvReflectShaderModule reflect_module{};
    auto spv_reflect_success = spv_reflect_check(spvReflectCreateShaderModule(
        spirv_length, spirv, &reflect_module
    ), "Failed to create shader module");

    const auto shader_idx = static_cast<i32>(type);

    BEE_ASSERT_F(shader_idx >= 0 && shader_idx < static_array_length(module->shaders), "Failed to reflect shader: Invalid shader type");

    if (!spv_reflect_success)
    {
        return false;
    }

    auto& shader = module->shaders[shader_idx];

    // Reflect vertex inputs if we're reflecting a vertex shader
    if (type == BscShaderType::vertex)
    {
        const auto success = reflect_vertex_description(&reflect_module, &module->pipeline_state.vertex_description, allocator);
        if (!success)
        {
            return false;
        }
    }

//    const auto success = reflect_resources(type, &reflect_module, &result->resources, allocator);
//    if (!success)
//    {
//        return false;
//    }

    /*
     * spvReflectGetCode returns a *word* array but spvReflectGetCodeSize returns the size in *bytes*
     */
    const auto byte_size = sign_cast<i32>(spvReflectGetCodeSize(&reflect_module));
    const auto spv_code = reinterpret_cast<const u8*>(spvReflectGetCode(&reflect_module));

    // Copy reflected to result
    shader.binary = FixedArray<u8>::with_size(sign_cast<i32>(spvReflectGetCodeSize(&reflect_module)), allocator);
    shader.binary.copy(0, spv_code, spv_code + byte_size);

    spvReflectDestroyShaderModule(&reflect_module);
    return true;
}


bool reflect_vertex_description(
    SpvReflectShaderModule* reflect_module,
    VertexDescriptor* vertex_desc,
    Allocator* allocator
)
{
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
    SpvReflectResult reflect_result;
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
            BEE_ERROR("ShaderCompiler", "Unsupported input type detected");
            return false;
        }

        if (attr.format == VertexFormat::unknown)
        {
            BEE_ERROR("ShaderCompiler", "Unable to convert vertex format of input to a valid Skyrocket format");
            return false;
        }

        vertex_desc->layouts[0].stride += vertex_format_size(attr.format);
    }

    return true;
}


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


} // namespace bee