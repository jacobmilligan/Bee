/*
 *  Shader.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Graphics/BSC.hpp"
#include "Bee/Core/JSON/JSON.hpp"
#include "Bee/Core/Filesystem.hpp"

#include <type_traits>

namespace bee {


String bsc_target_to_string(const BSCTarget target, Allocator* allocator)
{
    switch (target)
    {
        case BSCTarget::MSL:
            return String("MSL", allocator);
        case BSCTarget::HLSL:
            return String("HLSL", allocator);
        case BSCTarget::SPIRV:
            return String("SPIR-V", allocator);
        default:
            break;
    }

    return String("Invalid", allocator);
}

BSCTarget bsc_target_from_string(const StringView& target_string)
{
    if (target_string == "HLSL" || target_string == "hlsl") {
        return BSCTarget::HLSL;
    }

    if (target_string == "MSL" || target_string == "msl") {
        return BSCTarget::MSL;
    }

    if (target_string == "SPIR-V" || target_string == "spir-v") {
        return BSCTarget::SPIRV;
    }

    return BSCTarget::none;
}


/*
 *******************************************************
 *
 * # BSC source file parsing
 *
 ********************************************************
 */
#define BSC_PIPELINE_SECTION(section_member)                                                                    \
    BEE_BEGIN_MACRO_BLOCK                                                                                       \
        if (doc.has_member(json_handle, #section_member))                                                       \
        {                                                                                                       \
            parse_pipeline_section(doc, doc.get_member(json_handle, #section_member), &desc->section_member);   \
        }                                                                                                       \
    BEE_END_MACRO_BLOCK

#define BSC_PIPELINE_SECTION_ARRAY(section_member, member_count_dst)                                            \
    BEE_BEGIN_MACRO_BLOCK                                                                                       \
        if (doc.has_member(json_handle, #section_member))                                                           \
        {                                                                                                           \
            const auto array_handle = doc.get_member(json_handle, #section_member);                                 \
            auto range = doc.get_elements_range(array_handle);                                                      \
            const auto count = range.element_count();                                                               \
            if (count >= static_array_length(desc->section_member))                                                 \
            {                                                                                                       \
                BEE_ERROR("ShaderCompiler", #section_member " array size (%d) exceeds max size (%d)", count, static_array_length(desc->section_member));   \
            }                                                                                                       \
            else                                                                                                    \
            {                                                                                                       \
                desc->member_count_dst = count;                                                                     \
                int index = 0;                                                                                      \
                for (const auto& handle : range)                                                                    \
                {                                                                                                   \
                    parse_pipeline_section(doc, handle, &desc->section_member[index]);                              \
                    ++index;                                                                                        \
                }                                                                                                   \
            }                                                                                                       \
        }                                                                                                           \
    BEE_END_MACRO_BLOCK

#define BSC_PIPELINE_MEMBER(member)                                         \
    BEE_BEGIN_MACRO_BLOCK                                                   \
        if (doc.has_member(json_handle, #member))                           \
        {                                                                   \
            const auto value = doc.get_member_data(json_handle, #member);   \
            parse_pipeline_member(value, &desc->member);                    \
        }                                                                   \
    BEE_END_MACRO_BLOCK

#define BSC_PIPELINE_MEMBER_FLAGS(member, flag_type)                                \
    BEE_BEGIN_MACRO_BLOCK                                                           \
        if (doc.has_member(json_handle, #member))                                   \
        {                                                                           \
            const auto array_handle = doc.get_member(json_handle, #member);         \
            desc->member = static_cast<flag_type>(0);                               \
            for (const auto& handle : doc.get_elements_range(array_handle))         \
            {                                                                       \
                flag_type flag = static_cast<flag_type>(0);                         \
                parse_pipeline_member(doc.get_data(handle), &flag);                 \
                desc->member |= flag;                                               \
            }                                                                       \
        }                                                                           \
    BEE_END_MACRO_BLOCK

#define BSC_PIPELINE_ENUM_VALUE(enum_value)                             \
    BEE_BEGIN_MACRO_BLOCK                                               \
        if (str::compare(value.as_string(), #enum_value) == 0)          \
        {                                                               \
            using enum_type = std::remove_pointer_t<decltype(member)>;  \
            *member = enum_type::enum_value;                            \
            return;                                                     \
        }                                                               \
    BEE_END_MACRO_BLOCK

#define BSC_PARSE_PIPELINE_SECTION(type) \
    void parse_pipeline_section(const json::Document& doc, const json::ValueHandle& json_handle, type* desc)

#define BSC_PARSE_PIPELINE_MEMBER(type) void parse_pipeline_member(const json::ValueData& value, type* member)


static constexpr auto parse_error = "ShaderCompiler: failed to parse source file at %s: %s";


/*
 ***********************************
 *
 * GPU member primitives - parsing
 *
 ***********************************
 */
BSC_PARSE_PIPELINE_MEMBER(float)
{
    *member = static_cast<float>(value.as_number());
}

BSC_PARSE_PIPELINE_MEMBER(bool)
{
    *member = value.as_boolean();
}

BSC_PARSE_PIPELINE_MEMBER(u32)
{
    *member = sign_cast<u32>(value.as_number());
}

/*
 ***********************************
 *
 * GPU Enums - parsing
 *
 ***********************************
 */
BSC_PARSE_PIPELINE_MEMBER(FillMode)
{
    BSC_PIPELINE_ENUM_VALUE(wireframe);
    BSC_PIPELINE_ENUM_VALUE(solid);
}

BSC_PARSE_PIPELINE_MEMBER(CullMode)
{
    BSC_PIPELINE_ENUM_VALUE(none);
    BSC_PIPELINE_ENUM_VALUE(front);
    BSC_PIPELINE_ENUM_VALUE(back);
}

BSC_PARSE_PIPELINE_MEMBER(CompareFunc)
{
    BSC_PIPELINE_ENUM_VALUE(never);
    BSC_PIPELINE_ENUM_VALUE(less);
    BSC_PIPELINE_ENUM_VALUE(equal);
    BSC_PIPELINE_ENUM_VALUE(less_equal);
    BSC_PIPELINE_ENUM_VALUE(greater);
    BSC_PIPELINE_ENUM_VALUE(not_equal);
    BSC_PIPELINE_ENUM_VALUE(greater_equal);
    BSC_PIPELINE_ENUM_VALUE(always);
}

BSC_PARSE_PIPELINE_MEMBER(StencilOp)
{
    BSC_PIPELINE_ENUM_VALUE(keep);
    BSC_PIPELINE_ENUM_VALUE(zero);
    BSC_PIPELINE_ENUM_VALUE(replace);
    BSC_PIPELINE_ENUM_VALUE(increment_and_clamp);
    BSC_PIPELINE_ENUM_VALUE(decrement_and_clamp);
    BSC_PIPELINE_ENUM_VALUE(invert);
    BSC_PIPELINE_ENUM_VALUE(increment_and_wrap);
    BSC_PIPELINE_ENUM_VALUE(decrement_and_wrap);
}

BSC_PARSE_PIPELINE_MEMBER(PixelFormat)
{
    BSC_PIPELINE_ENUM_VALUE(a8);
    BSC_PIPELINE_ENUM_VALUE(r8);
    BSC_PIPELINE_ENUM_VALUE(r8i);
    BSC_PIPELINE_ENUM_VALUE(r8u);
    BSC_PIPELINE_ENUM_VALUE(r8s);
    BSC_PIPELINE_ENUM_VALUE(r16);
    BSC_PIPELINE_ENUM_VALUE(r16i);
    BSC_PIPELINE_ENUM_VALUE(r16u);
    BSC_PIPELINE_ENUM_VALUE(r16s);
    BSC_PIPELINE_ENUM_VALUE(r16f);
    BSC_PIPELINE_ENUM_VALUE(rg8);
    BSC_PIPELINE_ENUM_VALUE(rg8i);
    BSC_PIPELINE_ENUM_VALUE(rg8u);
    BSC_PIPELINE_ENUM_VALUE(rg8s);
    BSC_PIPELINE_ENUM_VALUE(r32u);
    BSC_PIPELINE_ENUM_VALUE(r32i);
    BSC_PIPELINE_ENUM_VALUE(r32f);
    BSC_PIPELINE_ENUM_VALUE(rg16);
    BSC_PIPELINE_ENUM_VALUE(rg16i);
    BSC_PIPELINE_ENUM_VALUE(rg16u);
    BSC_PIPELINE_ENUM_VALUE(rg16s);
    BSC_PIPELINE_ENUM_VALUE(rg16f);
    BSC_PIPELINE_ENUM_VALUE(rgba8);
    BSC_PIPELINE_ENUM_VALUE(rgba8i);
    BSC_PIPELINE_ENUM_VALUE(rgba8u);
    BSC_PIPELINE_ENUM_VALUE(rgba8s);
    BSC_PIPELINE_ENUM_VALUE(bgra8);
    BSC_PIPELINE_ENUM_VALUE(rg32u);
    BSC_PIPELINE_ENUM_VALUE(rg32s);
    BSC_PIPELINE_ENUM_VALUE(rg32f);
    BSC_PIPELINE_ENUM_VALUE(rgba16);
    BSC_PIPELINE_ENUM_VALUE(rgba16i);
    BSC_PIPELINE_ENUM_VALUE(rgba16u);
    BSC_PIPELINE_ENUM_VALUE(rgba16s);
    BSC_PIPELINE_ENUM_VALUE(rgba16f);
    BSC_PIPELINE_ENUM_VALUE(rgba32u);
    BSC_PIPELINE_ENUM_VALUE(rgba32i);
    BSC_PIPELINE_ENUM_VALUE(rgba32f);
    BSC_PIPELINE_ENUM_VALUE(d16);
    BSC_PIPELINE_ENUM_VALUE(d32f);
    BSC_PIPELINE_ENUM_VALUE(s8);
    BSC_PIPELINE_ENUM_VALUE(d24s8);
    BSC_PIPELINE_ENUM_VALUE(d32s8);
}

BSC_PARSE_PIPELINE_MEMBER(BlendOperation)
{
    BSC_PIPELINE_ENUM_VALUE(add);
    BSC_PIPELINE_ENUM_VALUE(subtract);
    BSC_PIPELINE_ENUM_VALUE(reverse_subtract);
    BSC_PIPELINE_ENUM_VALUE(min);
    BSC_PIPELINE_ENUM_VALUE(max);
}

BSC_PARSE_PIPELINE_MEMBER(ColorWriteMask)
{
    BSC_PIPELINE_ENUM_VALUE(none);
    BSC_PIPELINE_ENUM_VALUE(alpha);
    BSC_PIPELINE_ENUM_VALUE(blue);
    BSC_PIPELINE_ENUM_VALUE(green);
    BSC_PIPELINE_ENUM_VALUE(red);
    BSC_PIPELINE_ENUM_VALUE(all);
}

BSC_PARSE_PIPELINE_MEMBER(BlendFactor)
{
    BSC_PIPELINE_ENUM_VALUE(zero);
    BSC_PIPELINE_ENUM_VALUE(one);
    BSC_PIPELINE_ENUM_VALUE(src_color);
    BSC_PIPELINE_ENUM_VALUE(one_minus_src_color);
    BSC_PIPELINE_ENUM_VALUE(src_alpha);
    BSC_PIPELINE_ENUM_VALUE(one_minus_src_alpha);
    BSC_PIPELINE_ENUM_VALUE(dst_color);
    BSC_PIPELINE_ENUM_VALUE(one_minus_dst_color);
    BSC_PIPELINE_ENUM_VALUE(dst_alpha);
    BSC_PIPELINE_ENUM_VALUE(one_minus_dst_alpha);
    BSC_PIPELINE_ENUM_VALUE(src_alpha_saturated);
    BSC_PIPELINE_ENUM_VALUE(blend_color);
    BSC_PIPELINE_ENUM_VALUE(one_minus_blend_color);
    BSC_PIPELINE_ENUM_VALUE(blend_alpha);
    BSC_PIPELINE_ENUM_VALUE(one_minus_blend_alpha);
}

BSC_PARSE_PIPELINE_MEMBER(PrimitiveType)
{
    BSC_PIPELINE_ENUM_VALUE(point);
    BSC_PIPELINE_ENUM_VALUE(line);
    BSC_PIPELINE_ENUM_VALUE(line_strip);
    BSC_PIPELINE_ENUM_VALUE(triangle);
    BSC_PIPELINE_ENUM_VALUE(triangle_strip);
}


/*
 ***********************************
 *
 * Pipeline sections - Parsing operations
 *
 ***********************************
 */
BSC_PARSE_PIPELINE_SECTION(BlendStateDescriptor)
{
    BSC_PIPELINE_MEMBER(blend_enabled);
    BSC_PIPELINE_MEMBER(format);
    BSC_PIPELINE_MEMBER_FLAGS(color_write_mask, ColorWriteMask);
    BSC_PIPELINE_MEMBER(alpha_blend_op);
    BSC_PIPELINE_MEMBER(colorw_blend_op);
    BSC_PIPELINE_MEMBER(src_blend_alpha);
    BSC_PIPELINE_MEMBER(src_blend_colorw);
    BSC_PIPELINE_MEMBER(dst_blend_alpha);
    BSC_PIPELINE_MEMBER(dst_blend_colorw);

}

BSC_PARSE_PIPELINE_SECTION(StencilOpDescriptor)
{
    BSC_PIPELINE_MEMBER(fail_op);
    BSC_PIPELINE_MEMBER(pass_op);
    BSC_PIPELINE_MEMBER(depth_fail_op);
    BSC_PIPELINE_MEMBER(compare_func);
}

BSC_PARSE_PIPELINE_SECTION(RasterStateDescriptor)
{
    BSC_PIPELINE_MEMBER(fill_mode);
    BSC_PIPELINE_MEMBER(cull_mode);
    BSC_PIPELINE_MEMBER(line_width);
    BSC_PIPELINE_MEMBER(depth_clamp_enabled);
    BSC_PIPELINE_MEMBER(depth_bias_enabled);
    BSC_PIPELINE_MEMBER(depth_bias);
    BSC_PIPELINE_MEMBER(depth_slope_factor);
    BSC_PIPELINE_MEMBER(depth_bias_clamp);
}

BSC_PARSE_PIPELINE_SECTION(DepthStencilStateDescriptor)
{
    BSC_PIPELINE_MEMBER(depth_compare_func);
    BSC_PIPELINE_MEMBER(depth_test_enabled);
    BSC_PIPELINE_MEMBER(depth_write_enabled);
    BSC_PIPELINE_MEMBER(stencil_test_enabled);
    BSC_PIPELINE_SECTION(front_face_stencil);
    BSC_PIPELINE_SECTION(back_face_stencil);
}

BSC_PARSE_PIPELINE_SECTION(PipelineStateCreateInfo)
{
    BSC_PIPELINE_MEMBER(primitive_type);
    BSC_PIPELINE_MEMBER(sample_count);
    BSC_PIPELINE_SECTION(raster_state);
    BSC_PIPELINE_SECTION(depth_stencil_state);
    BSC_PIPELINE_SECTION_ARRAY(color_blend_states, color_blend_state_count);
}


BSCTextSource bsc_parse_source(const Path& path, Allocator* allocator)
{
    auto source_text = fs::read(path, allocator);

    json::ParseOptions json_options{};
    json_options.require_commas = false;
    json_options.require_root_element = false;
    json_options.require_string_keys = false;
    json_options.allow_comments = true;
    json_options.allow_multiline_strings = true;
    json_options.allocation_mode = json::AllocationMode::dynamic;

    json::Document json_doc(json_options);
    const auto json_success = json_doc.parse(source_text.data());

    if (BEE_FAIL_F(json_success, parse_error, path.c_str(), json_doc.get_error_string().c_str()))
    {
        return BSCTextSource();
    }

    static auto get_required_member = [&](const json::ValueHandle& root, const char* root_name, const char* key)
    {
        const auto has_member = json_doc.has_member(root, key);
        if (BEE_FAIL_F(has_member, "ShaderCompiler: failed to parse source file at %s: missing required member %s.%s", path.c_str(), root_name, key))
        {
            return json::ValueData{};
        }

        return json_doc.get_member_data(root, key);
    };

    const auto name = get_required_member(json_doc.root(), "", "name");
    const auto pipeline = get_required_member(json_doc.root(), "", "pipeline");
    const auto shader = get_required_member(json_doc.root(), "", "shader");
    const auto has_top_level_elements = name.is_valid() && pipeline.is_valid() && shader.is_valid();

    if (!has_top_level_elements)
    {
        return BSCTextSource();
    }

    const auto pipeline_handle = json_doc.get_member(json_doc.root(), "pipeline");
    const auto vertex_stage = get_required_member(pipeline_handle, "pipeline", "vertex_stage");
    const auto fragment_stage = get_required_member(pipeline_handle, "pipeline", "fragment_stage");

    if (BEE_FAIL_F(vertex_stage.is_valid() && fragment_stage.is_valid(), "Missing required shader stages"))
    {
        return BSCTextSource();
    }

    BSCTextSource source;
    source.name = String(name.as_string(), allocator);
    source.text = String(shader.as_string(), allocator);
    source.shader_count = 2; // vertex & fragment at minimum

    // Get the pipeline shader stage names - these are the only parts of the pipeline not parsed using the macros above
    source.shader_entries[static_cast<i32>(BSCShaderType::vertex)] = String(vertex_stage.as_string(), allocator);
    source.shader_entries[static_cast<i32>(BSCShaderType::fragment)] = String(fragment_stage.as_string(), allocator);

    //... more stages here

    parse_pipeline_section(json_doc, pipeline_handle, &source.pipeline_state);

    return source;
}


} // namespace bee