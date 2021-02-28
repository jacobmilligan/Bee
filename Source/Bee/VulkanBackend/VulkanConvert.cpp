/*
 *  VulkanConvert.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/VulkanBackend/VulkanConvert.hpp"
#include "Bee/Core/Bit.hpp"
#include "Bee/Core/TypeTraits.hpp"


namespace bee {


PhysicalDeviceType convert_device_type(VkPhysicalDeviceType type)
{
    switch (type)
    {
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return PhysicalDeviceType::other;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return PhysicalDeviceType::integrated;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return PhysicalDeviceType::discrete;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return PhysicalDeviceType::virtual_gpu;
        default:
            break;
    }
    return PhysicalDeviceType::unknown;
}

PhysicalDeviceVendor convert_vendor(const u32 id)
{
    switch (id)
    {
        case 0x1002:
            return PhysicalDeviceVendor::AMD;
        case 0x1010:
            return PhysicalDeviceVendor::ImgTec;
        case 0x10DE:
            return PhysicalDeviceVendor::NVIDIA;
        case 0x13B5:
            return PhysicalDeviceVendor::ARM;
        case 0x5143:
            return PhysicalDeviceVendor::Qualcomm;
        case 0x8086:
            return PhysicalDeviceVendor::Intel;
        default:
            break;
    }
    return PhysicalDeviceVendor::unknown;
}

VkImageAspectFlags select_access_mask_from_format(const PixelFormat format)
{
    switch(format)
    {
        case PixelFormat::d16:
        case PixelFormat::d32f:
        {
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        case PixelFormat::d24s8:
        case PixelFormat::d32s8:
        {
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        case PixelFormat::s8:
        {
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        case PixelFormat::invalid:
        case PixelFormat::unknown:
        {
            return 0;
        }
        default:
        {
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }
}

VkPipelineStageFlags select_pipeline_stage_from_access(const VkAccessFlags access)
{
    VkPipelineStageFlags result = 0;

    for_each_flag(access, [&](const VkAccessFlags flag)
    {
        switch (flag)
        {
            case VK_ACCESS_INDIRECT_COMMAND_READ_BIT:
            {
                result |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
                break;
            }
            case VK_ACCESS_INDEX_READ_BIT:
            case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
            {
                result |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
                break;
            }
            case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
            {
                result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;
            }
            case VK_ACCESS_UNIFORM_READ_BIT:
            case VK_ACCESS_SHADER_READ_BIT:
            case VK_ACCESS_SHADER_WRITE_BIT:
            {
                result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                    | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
#if 0
                // TODO(Jacob): geometry shaders?
                result |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                    | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                    | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
#endif // 0
                break;
            }
            case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT:
            case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
            {
                result |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;
            }
            case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT:
            case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
            {
                result |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;
            }
            case VK_ACCESS_TRANSFER_READ_BIT:
            case VK_ACCESS_TRANSFER_WRITE_BIT:
            {
                result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            }
            case VK_ACCESS_MEMORY_READ_BIT:
            case VK_ACCESS_MEMORY_WRITE_BIT:
            {
                result |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Invalid access type");
            }
        }
    });

    return result;
}

VkColorComponentFlags decode_color_write_mask(const ColorWriteMask mask)
{
    return 0u
        | decode_flag(mask, ColorWriteMask::alpha, VK_COLOR_COMPONENT_A_BIT)
        | decode_flag(mask, ColorWriteMask::blue, VK_COLOR_COMPONENT_B_BIT)
        | decode_flag(mask, ColorWriteMask::green, VK_COLOR_COMPONENT_G_BIT)
        | decode_flag(mask, ColorWriteMask::red, VK_COLOR_COMPONENT_R_BIT);
}

VkBufferUsageFlags decode_buffer_type(const BufferType type)
{
    return 0u
        | decode_flag(type, BufferType::vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        | decode_flag(type, BufferType::index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        | decode_flag(type, BufferType::uniform_buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        | decode_flag(type, BufferType::transfer_dst, VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        | decode_flag(type, BufferType::transfer_src, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

VkShaderStageFlags decode_shader_stage(const ShaderStageFlags stages)
{
    auto vk_flags = 0u
        | decode_flag(stages, ShaderStageFlags::vertex, VK_SHADER_STAGE_VERTEX_BIT)
        | decode_flag(stages, ShaderStageFlags::fragment, VK_SHADER_STAGE_FRAGMENT_BIT)
        | decode_flag(stages, ShaderStageFlags::geometry, VK_SHADER_STAGE_GEOMETRY_BIT)
        | decode_flag(stages, ShaderStageFlags::compute, VK_SHADER_STAGE_COMPUTE_BIT);

    if (underlying_t(stages & ShaderStageFlags::graphics) - 1 >= underlying_t(ShaderStageFlags::graphics) - 1)
    {
        vk_flags |= VK_SHADER_STAGE_ALL_GRAPHICS;
    }

    if (underlying_t(stages & ShaderStageFlags::all) - 1 >= underlying_t(ShaderStageFlags::all) - 1)
    {
        vk_flags |= VK_SHADER_STAGE_ALL;
    }

    return vk_flags;
}

VkAccessFlags decode_buffer_access(const BufferType& type, const bool is_read)
{
    return 0u
        | decode_flag(type, BufferType::index_buffer, (is_read) ? VK_ACCESS_INDEX_READ_BIT : VK_ACCESS_MEMORY_WRITE_BIT)
        | decode_flag(type, BufferType::vertex_buffer, (is_read) ? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT : VK_ACCESS_MEMORY_WRITE_BIT)
        | decode_flag(type, BufferType::uniform_buffer, (is_read) ? VK_ACCESS_UNIFORM_READ_BIT : VK_ACCESS_MEMORY_WRITE_BIT)
        | decode_flag(type, BufferType::transfer_dst, (is_read) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT)
        | decode_flag(type, BufferType::transfer_src, (is_read) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT);
}

VkImageUsageFlags decode_image_usage(const TextureUsage& usage)
{
    return 0u
        | decode_flag(usage, TextureUsage::transfer_src, VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        | decode_flag(usage, TextureUsage::transfer_dst, VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        | decode_flag(usage, TextureUsage::color_attachment, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        | decode_flag(usage, TextureUsage::depth_stencil_attachment, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        | decode_flag(usage, TextureUsage::sampled, VK_IMAGE_USAGE_SAMPLED_BIT)
        | decode_flag(usage, TextureUsage::storage, VK_IMAGE_USAGE_STORAGE_BIT)
        | decode_flag(usage, TextureUsage::input_attachment, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        | decode_flag(usage, TextureUsage::transient, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
}

VkSampleCountFlagBits decode_sample_count(const u32 samples)
{
    switch (samples)
    {
        case 0u:
        case 1u:
        case 2u:
        case 4u:
        case 8u:
        case 16u:
        case 32u:
        case 64u:
        {
            return static_cast<VkSampleCountFlagBits>(samples);
        }
        default: break;
    }

    BEE_UNREACHABLE("Invalid sample count (%u) must be power of two <= 64u", samples);
}

BEE_TRANSLATION_TABLE_FUNC(convert_pixel_format, PixelFormat, VkFormat, PixelFormat::unknown,
    // Ordinary 8 bit formats
    VK_FORMAT_R8_UNORM,             // a8
    VK_FORMAT_R8_UNORM,             // r8
    VK_FORMAT_R8_SINT,              // r8i
    VK_FORMAT_R8_UINT,              // r8u
    VK_FORMAT_R8_SNORM,             // r8s

    // Ordinary 16 bit formats
    VK_FORMAT_R16_UNORM,            // r16
    VK_FORMAT_R16_SINT,             // r16i
    VK_FORMAT_R16_UINT,             // r16u
    VK_FORMAT_R16_SNORM,            // r16s
    VK_FORMAT_R16_SFLOAT,           // r16f
    VK_FORMAT_R8G8_UNORM,           // rg8
    VK_FORMAT_R8G8_SINT,            // rg8i
    VK_FORMAT_R8G8_UINT,            // rg8u
    VK_FORMAT_R8G8_SNORM,           // rg8s

    // Ordinary 32 bit formats
    VK_FORMAT_R32_UINT,             // r32u
    VK_FORMAT_R32_SINT,             // r32i
    VK_FORMAT_R32_SFLOAT,           // r32f
    VK_FORMAT_R16G16_UNORM,         // rg16
    VK_FORMAT_R16G16_SINT,          // rg16i
    VK_FORMAT_R16G16_UINT,          // rg16u
    VK_FORMAT_R16G16_SNORM,         // rg16s
    VK_FORMAT_R16G16_SFLOAT,        // rg16f
    VK_FORMAT_R8G8B8A8_UNORM,       // rgba8
    VK_FORMAT_R8G8B8A8_SINT,        // rgba8i
    VK_FORMAT_R8G8B8A8_UINT,        // rgba8u
    VK_FORMAT_R8G8B8A8_SNORM,       // rgba8s
    VK_FORMAT_B8G8R8A8_UNORM,       // bgra8

    // Ordinary 64 bit formats
    VK_FORMAT_R32G32_UINT,          // rg32u
    VK_FORMAT_R32G32_SINT,          // rg32s
    VK_FORMAT_R32G32_SFLOAT,        // rg32f
    VK_FORMAT_R16G16B16A16_UNORM,   // rgba16
    VK_FORMAT_R16G16B16A16_SINT,    // rgba16i
    VK_FORMAT_R16G16B16A16_UINT,    // rgba16u
    VK_FORMAT_R16G16B16A16_SNORM,   // rgba16s
    VK_FORMAT_R16G16B16A16_SFLOAT,  // rgba16f

    // Ordinary 128 bit formats
    VK_FORMAT_R32G32B32A32_UINT,    // rgba32u
    VK_FORMAT_R32G32B32A32_SINT,    // rgba32s
    VK_FORMAT_R32G32B32A32_SFLOAT,  // rgba32f

    // Depth and stencil formats
    VK_FORMAT_D16_UNORM,            // d16
    VK_FORMAT_D32_SFLOAT,           // d32f
    VK_FORMAT_S8_UINT,              // s8
    VK_FORMAT_D24_UNORM_S8_UINT,    // d24s8
    VK_FORMAT_D32_SFLOAT_S8_UINT,   // d32s8
    VK_FORMAT_UNDEFINED             // invalid
)

PixelFormat convert_vk_format(const VkFormat format)
{
#define BEE_VK_FMT(VK, PF) case VK: return PixelFormat::PF;
    switch (format)
    {
        // Ordinary 8 bit formats
        BEE_VK_FMT(VK_FORMAT_R8_UNORM, r8)
        BEE_VK_FMT(VK_FORMAT_R8_SINT, r8i)
        BEE_VK_FMT(VK_FORMAT_R8_UINT, r8u)
        BEE_VK_FMT(VK_FORMAT_R8_SNORM, r8s)

        // Ordinary 16 bit formats
        BEE_VK_FMT(VK_FORMAT_R16_UNORM, r16)
        BEE_VK_FMT(VK_FORMAT_R16_SINT, r16i)
        BEE_VK_FMT(VK_FORMAT_R16_UINT, r16u)
        BEE_VK_FMT(VK_FORMAT_R16_SNORM, r16s)
        BEE_VK_FMT(VK_FORMAT_R16_SFLOAT, r16f)
        BEE_VK_FMT(VK_FORMAT_R8G8_UNORM, rg8)
        BEE_VK_FMT(VK_FORMAT_R8G8_SINT, rg8i)
        BEE_VK_FMT(VK_FORMAT_R8G8_UINT, rg8u)
        BEE_VK_FMT(VK_FORMAT_R8G8_SNORM, rg8s)

        // Ordinary 32 bit formats
        BEE_VK_FMT(VK_FORMAT_R32_UINT, r32u)
        BEE_VK_FMT(VK_FORMAT_R32_SINT, r32i)
        BEE_VK_FMT(VK_FORMAT_R32_SFLOAT, r32f)
        BEE_VK_FMT(VK_FORMAT_R16G16_UNORM, rg16)
        BEE_VK_FMT(VK_FORMAT_R16G16_SINT, rg16i)
        BEE_VK_FMT(VK_FORMAT_R16G16_UINT, rg16u)
        BEE_VK_FMT(VK_FORMAT_R16G16_SNORM, rg16s)
        BEE_VK_FMT(VK_FORMAT_R16G16_SFLOAT, rg16f)
        BEE_VK_FMT(VK_FORMAT_R8G8B8A8_UNORM, rgba8)
        BEE_VK_FMT(VK_FORMAT_R8G8B8A8_SINT, rgba8i)
        BEE_VK_FMT(VK_FORMAT_R8G8B8A8_UINT, rgba8u)
        BEE_VK_FMT(VK_FORMAT_R8G8B8A8_SNORM, rgba8s)
        BEE_VK_FMT(VK_FORMAT_B8G8R8A8_UNORM, bgra8)

        // Ordinary 64 bit formats
        BEE_VK_FMT(VK_FORMAT_R32G32_UINT, rg32u)
        BEE_VK_FMT(VK_FORMAT_R32G32_SINT, rg32s)
        BEE_VK_FMT(VK_FORMAT_R32G32_SFLOAT, rg32f)
        BEE_VK_FMT(VK_FORMAT_R16G16B16A16_UNORM, rgba16)
        BEE_VK_FMT(VK_FORMAT_R16G16B16A16_SINT, rgba16i)
        BEE_VK_FMT(VK_FORMAT_R16G16B16A16_UINT, rgba16u)
        BEE_VK_FMT(VK_FORMAT_R16G16B16A16_SNORM, rgba16s)
        BEE_VK_FMT(VK_FORMAT_R16G16B16A16_SFLOAT, rgba16f)

        // Ordinary 128 bit formats
        BEE_VK_FMT(VK_FORMAT_R32G32B32A32_UINT, rgba32u)
        BEE_VK_FMT(VK_FORMAT_R32G32B32A32_SINT, rgba32i)
        BEE_VK_FMT(VK_FORMAT_R32G32B32A32_SFLOAT, rgba32f)

        // Depth and stencil formats
        BEE_VK_FMT(VK_FORMAT_D16_UNORM, d16)
        BEE_VK_FMT(VK_FORMAT_D32_SFLOAT, d32f)
        BEE_VK_FMT(VK_FORMAT_S8_UINT, s8)
        BEE_VK_FMT(VK_FORMAT_D24_UNORM_S8_UINT, d24s8)
        BEE_VK_FMT(VK_FORMAT_D32_SFLOAT_S8_UINT, d32s8)

        default: break;
    }
#undef BEE_VK_FMT

    return PixelFormat::invalid;
}

BEE_TRANSLATION_TABLE_FUNC(convert_load_op, LoadOp, VkAttachmentLoadOp, LoadOp::unknown,
    VK_ATTACHMENT_LOAD_OP_LOAD,         // load
    VK_ATTACHMENT_LOAD_OP_CLEAR,        // clear
    VK_ATTACHMENT_LOAD_OP_DONT_CARE,    // dont_care
)

BEE_TRANSLATION_TABLE_FUNC(convert_store_op, StoreOp, VkAttachmentStoreOp, StoreOp::unknown,
    VK_ATTACHMENT_STORE_OP_STORE,       // store
    VK_ATTACHMENT_STORE_OP_DONT_CARE    // dont_care
)

BEE_TRANSLATION_TABLE_FUNC(convert_step_function, StepFunction, VkVertexInputRate, StepFunction::unknown,
    VK_VERTEX_INPUT_RATE_VERTEX,    // per_vertex
    VK_VERTEX_INPUT_RATE_INSTANCE   // per_instance
)

BEE_TRANSLATION_TABLE_FUNC(convert_vertex_format, VertexFormat, VkFormat, VertexFormat::unknown,
    VK_FORMAT_R32_SFLOAT,             // float1
    VK_FORMAT_R32G32_SFLOAT,          // float2
    VK_FORMAT_R32G32B32_SFLOAT,       // float3
    VK_FORMAT_R32G32B32A32_SFLOAT,    // float4
    VK_FORMAT_R8_SINT,                // byte1
    VK_FORMAT_R8G8_SINT,              // byte2
    VK_FORMAT_R8G8B8_SINT,            // byte3
    VK_FORMAT_R8G8B8A8_SINT,          // byte4
    VK_FORMAT_R8_UINT,                // ubyte1
    VK_FORMAT_R8G8_UINT,              // ubyte2
    VK_FORMAT_R8G8B8_UINT,            // ubyte3
    VK_FORMAT_R8G8B8A8_UINT,          // ubyte4
    VK_FORMAT_R8_UNORM,               // unormbyte1
    VK_FORMAT_R8G8_UNORM,             // unormbyte2
    VK_FORMAT_R8G8B8_UNORM,           // unormbyte3
    VK_FORMAT_R8G8B8A8_UNORM,         // unormbyte4
    VK_FORMAT_R16_SINT,               // short1
    VK_FORMAT_R16G16_SINT,            // short2
    VK_FORMAT_R16G16B16_SINT,         // short3
    VK_FORMAT_R16G16B16A16_SINT,      // short4
    VK_FORMAT_R16_UINT,               // ushort1
    VK_FORMAT_R16G16_UINT,            // ushort2
    VK_FORMAT_R16G16B16_UINT,         // ushort3
    VK_FORMAT_R16G16B16A16_UINT,      // ushort4
    VK_FORMAT_R16_SFLOAT,             // half1
    VK_FORMAT_R16G16_SFLOAT,          // half2
    VK_FORMAT_R16G16B16_SFLOAT,       // half3
    VK_FORMAT_R16G16B16A16_SFLOAT,    // half4
    VK_FORMAT_R32_SINT,               // int1
    VK_FORMAT_R32G32_SINT,            // int2
    VK_FORMAT_R32G32B32_SINT,         // int3
    VK_FORMAT_R32G32B32A32_SINT,      // int4
    VK_FORMAT_R32_UINT,               // uint1
    VK_FORMAT_R32G32_UINT,            // uint2
    VK_FORMAT_R32G32B32_UINT,         // uint3
    VK_FORMAT_R32G32B32A32_UINT,      // uint4
    VK_FORMAT_UNDEFINED               // invalid
)

BEE_TRANSLATION_TABLE_FUNC(convert_primitive_type, PrimitiveType, VkPrimitiveTopology, PrimitiveType::unknown,
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,       // point
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,        // line
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,       // line_strip
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,    // triangle
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP    // triangle_strip
)

BEE_TRANSLATION_TABLE_FUNC(convert_fill_mode, FillMode, VkPolygonMode, FillMode::unknown,
    VK_POLYGON_MODE_LINE,   // wireframe
    VK_POLYGON_MODE_FILL    // solid
)

BEE_TRANSLATION_TABLE_FUNC(convert_cull_mode, CullMode, VkCullModeFlagBits, CullMode::unknown,
    VK_CULL_MODE_NONE,      // none
    VK_CULL_MODE_FRONT_BIT, // back
    VK_CULL_MODE_BACK_BIT   // back
)

BEE_TRANSLATION_TABLE_FUNC(convert_blend_factor, BlendFactor, VkBlendFactor, BlendFactor::unknown,
    VK_BLEND_FACTOR_ZERO,                     // zero
    VK_BLEND_FACTOR_ONE,                      // one
    VK_BLEND_FACTOR_SRC_COLOR,                // src_color
    VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,      // one_minus_src_color
    VK_BLEND_FACTOR_SRC_ALPHA,                // src_alpha
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,      // one_minus_src_alpha
    VK_BLEND_FACTOR_DST_COLOR,                // dst_color
    VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,      // one_minus_dst_color
    VK_BLEND_FACTOR_DST_ALPHA,                // dst_alpha
    VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,      // one_minus_dst_alpha
    VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,       // src_alpha_saturated
    VK_BLEND_FACTOR_CONSTANT_COLOR,           // blend_color
    VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR, // one_minus_blend_color
    VK_BLEND_FACTOR_CONSTANT_ALPHA,           // blend_alpha
    VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA  // one_minus_blend_alpha
)

BEE_TRANSLATION_TABLE_FUNC(convert_blend_op, BlendOperation, VkBlendOp, BlendOperation::unknown,
    VK_BLEND_OP_ADD,                // add
    VK_BLEND_OP_SUBTRACT,           // subtract
    VK_BLEND_OP_REVERSE_SUBTRACT,   // reverse_subtract
    VK_BLEND_OP_MIN,                // min
    VK_BLEND_OP_MAX,                // max
)

BEE_TRANSLATION_TABLE_FUNC(convert_resource_binding_type, ResourceBindingType, VkDescriptorType, ResourceBindingType::unknown,
    VK_DESCRIPTOR_TYPE_SAMPLER,                   // sampler
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    // combined_texture_sampler
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             // sampled_texture
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             // storage_texture
    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      // uniform_texel_buffer
    VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      // storage_texel_buffer
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            // uniform_buffer
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            // storage_buffer
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,    // dynamic_uniform_buffer
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,    // dynamic_storage_buffer
    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT           // input_attachment
)

BEE_TRANSLATION_TABLE_FUNC(convert_memory_usage, DeviceMemoryUsage, VmaMemoryUsage, DeviceMemoryUsage::unknown,
    VMA_MEMORY_USAGE_GPU_ONLY,        // gpu_only
    VMA_MEMORY_USAGE_CPU_ONLY,        // cpu_only
    VMA_MEMORY_USAGE_CPU_TO_GPU,      // cpu_to_gpu
    VMA_MEMORY_USAGE_GPU_TO_CPU       // gpu_to_cpu
)

BEE_TRANSLATION_TABLE_FUNC(convert_compare_func, CompareFunc, VkCompareOp, CompareFunc::unknown,
    VK_COMPARE_OP_NEVER,              // never
    VK_COMPARE_OP_LESS,               // less
    VK_COMPARE_OP_EQUAL,              // equal
    VK_COMPARE_OP_LESS_OR_EQUAL,      // less_equal
    VK_COMPARE_OP_GREATER,            // greater
    VK_COMPARE_OP_NOT_EQUAL,          // not_equal
    VK_COMPARE_OP_GREATER_OR_EQUAL,   // greater_equal
    VK_COMPARE_OP_ALWAYS,             // always
)

BEE_TRANSLATION_TABLE_FUNC(convert_stencil_op, StencilOp, VkStencilOp, StencilOp::unknown,
    VK_STENCIL_OP_KEEP,                   // keep
    VK_STENCIL_OP_ZERO,                   // zero
    VK_STENCIL_OP_REPLACE,                // replace
    VK_STENCIL_OP_INCREMENT_AND_CLAMP,    // increment_and_clamp
    VK_STENCIL_OP_DECREMENT_AND_CLAMP,    // decrement_and_clamp
    VK_STENCIL_OP_INVERT,                 // invert
    VK_STENCIL_OP_INCREMENT_AND_WRAP,     // increment_and_wrap
    VK_STENCIL_OP_DECREMENT_AND_CLAMP     // decrement_and_wrap
)

BEE_TRANSLATION_TABLE_FUNC(convert_image_type, TextureType, VkImageType, TextureType::unknown,
    VK_IMAGE_TYPE_1D,     // tex1d
    VK_IMAGE_TYPE_1D,     // tex1d_array
    VK_IMAGE_TYPE_2D,     // tex2d
    VK_IMAGE_TYPE_2D,     // tex2d_array
    VK_IMAGE_TYPE_2D,     // tex2d_multisample
    VK_IMAGE_TYPE_2D,     // cube
    VK_IMAGE_TYPE_2D,     // cube_array
    VK_IMAGE_TYPE_3D      // tex3d
)

BEE_TRANSLATION_TABLE_FUNC(convert_image_view_type, TextureType, VkImageViewType, TextureType::unknown,
    VK_IMAGE_VIEW_TYPE_1D,            // tex1d
    VK_IMAGE_VIEW_TYPE_1D_ARRAY,      // tex1d_array
    VK_IMAGE_VIEW_TYPE_2D,            // tex2d
    VK_IMAGE_VIEW_TYPE_2D_ARRAY,      // tex2d_array
    VK_IMAGE_VIEW_TYPE_2D,            // tex2d_multisample
    VK_IMAGE_VIEW_TYPE_CUBE,          // cube
    VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,    // cube_array
    VK_IMAGE_VIEW_TYPE_3D             // tex3d
)

BEE_TRANSLATION_TABLE_FUNC(convert_filter, MinMagFilter, VkFilter, MinMagFilter::unknown,
    VK_FILTER_NEAREST,  // nearest
    VK_FILTER_LINEAR    // linear
)

BEE_TRANSLATION_TABLE_FUNC(convert_mip_map_mode, MipMapMode, VkSamplerMipmapMode, MipMapMode::unknown,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,  // none
    VK_SAMPLER_MIPMAP_MODE_NEAREST, // nearest
    VK_SAMPLER_MIPMAP_MODE_LINEAR   // linear
)

BEE_TRANSLATION_TABLE_FUNC(convert_address_mode, AddressMode, VkSamplerAddressMode, AddressMode::unknown,
    VK_SAMPLER_ADDRESS_MODE_REPEAT,                 // repeat
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,        // mirrored_repeat
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,          // clamp_to_edge
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,        // clamp_to_border
    VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE    // mirror_clamp_to_edge,
)

BEE_TRANSLATION_TABLE_FUNC(convert_border_color, BorderColor, VkBorderColor, BorderColor::unknown,
    VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,    // transparent_black
    VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,         // opaque_black
    VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE          // opaque_white
)

BEE_TRANSLATION_TABLE_FUNC(convert_command_pool_hint, CommandPoolHint, VkCommandPoolCreateFlags, CommandPoolHint::unknown,
    VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,               // transient,
    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT     // allow_individual_reset
)

BEE_TRANSLATION_TABLE_FUNC(convert_command_buffer_reset_hint, CommandStreamReset, VkCommandBufferResetFlags, CommandStreamReset::unknown,
    0u,                                             // none,
    VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT   // release_resources
)

BEE_TRANSLATION_TABLE_FUNC(convert_command_buffer_usage, CommandBufferUsage, VkCommandBufferUsageFlags, CommandBufferUsage::unknown,
    0u,                                             // default_usage
    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,    // submit_once
    VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT    // simultaneous_usage
)

BEE_TRANSLATION_TABLE_FUNC(convert_access_mask, GpuResourceState, VkAccessFlags, GpuResourceState::unknown,
    0,                                                                          // undefined
    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,                     // general
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // color_attachment
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,                                        // vertex_buffer
    VK_ACCESS_UNIFORM_READ_BIT,                                                 // uniform_buffer
    VK_ACCESS_INDEX_READ_BIT,                                                   // index_buffer
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,                                // depth_read
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,                               // depth_write
    VK_ACCESS_SHADER_READ_BIT,                                                  // shader_read_only
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,                                        // indirect_argument
    VK_ACCESS_TRANSFER_READ_BIT,                                                // transfer_src
    VK_ACCESS_TRANSFER_WRITE_BIT,                                               // transfer_dst
    0                                                                           // present
)

BEE_TRANSLATION_TABLE_FUNC(convert_image_layout, GpuResourceState, VkImageLayout, GpuResourceState::unknown,
    VK_IMAGE_LAYOUT_UNDEFINED,                          // undefined
    VK_IMAGE_LAYOUT_GENERAL,                            // general
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,           // color_attachment
    VK_IMAGE_LAYOUT_UNDEFINED,                          // vertex_buffer
    VK_IMAGE_LAYOUT_UNDEFINED,                          // uniform_buffer
    VK_IMAGE_LAYOUT_UNDEFINED,                          // index_buffer
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,    // depth_read
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,   // depth_write
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,           // shader_read_only
    VK_IMAGE_LAYOUT_UNDEFINED,                          // indirect_argument
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,               // transfer_src
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,               // transfer_dst
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR                     // present
)

BEE_TRANSLATION_TABLE_FUNC(convert_index_type, IndexFormat, VkIndexType, IndexFormat::unknown,
    VK_INDEX_TYPE_NONE_NV,  // none
    VK_INDEX_TYPE_UINT16,   // uint16
    VK_INDEX_TYPE_UINT32    // uint32
)


} // namespace bee