/*
 *  GPU.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Graphics/GPULimits.hpp"
#include "Bee/Application/Platform.hpp"


namespace bee {


/*
 ********************************************************
 *
 * # GPU Handles
 *
 * Handles to GPU objects. Most of these are versioned
 * integer handles used to index into an array in the
 * backend.
 *
 ********************************************************
 */
BEE_RAW_HANDLE_U32(DeviceHandle);
BEE_VERSIONED_HANDLE_32(SwapchainHandle);
BEE_VERSIONED_HANDLE_32(TextureHandle);
BEE_VERSIONED_HANDLE_32(TextureViewHandle);
BEE_VERSIONED_HANDLE_32(BufferHandle);
BEE_VERSIONED_HANDLE_32(BufferViewHandle);
BEE_VERSIONED_HANDLE_32(RenderPassHandle);
BEE_VERSIONED_HANDLE_32(ShaderHandle);
BEE_VERSIONED_HANDLE_32(PipelineStateHandle);
BEE_VERSIONED_HANDLE_32(CommandPoolHandle);
BEE_VERSIONED_HANDLE_32(CommandStreamHandle);
BEE_VERSIONED_HANDLE_32(FenceHandle);
struct GpuCommandBuffer;

/*
 ********************************************************
 *
 * # GPU enums
 *
 * Mostly maps very closely to vulkan with a few
 * alterations to make dealing with low-level operations
 * a little simpler
 *
 ********************************************************
 */
enum class GpuBackend
{
    none,
    vulkan
};

BEE_FLAGS(QueueType, u32)
{
    none        = 0u,
    graphics    = 1u << 0u,
    compute     = 1u << 1u,
    transfer    = 1u << 2u,
    all         = graphics | compute | transfer
};

enum class FillMode : u32
{
    wireframe = 0,
    solid,
    unknown
};

enum class CullMode : u32
{
    none = 0,
    front,
    back,
    unknown
};

enum class CompareFunc : u32
{
    never = 0,
    less,
    equal,
    less_equal,
    greater,
    not_equal,
    greater_equal,
    always,
    unknown
};

enum class StencilOp : u32
{
    keep,
    zero,
    replace,
    increment_and_clamp,
    decrement_and_clamp,
    invert,
    increment_and_wrap,
    decrement_and_wrap,
    unknown
};

enum class BlendFactor : u32
{
    zero = 0,
    one,
    src_color,
    one_minus_src_color,
    src_alpha,
    one_minus_src_alpha,
    dst_color,
    one_minus_dst_color,
    dst_alpha,
    one_minus_dst_alpha,
    src_alpha_saturated,
    blend_color,
    one_minus_blend_color,
    blend_alpha,
    one_minus_blend_alpha,
    unknown
};

enum class BlendOperation : u32
{
    add = 0,
    subtract,
    reverse_subtract,
    min,
    max,
    unknown
};

// TODO(Jacob): packed formats for 16, 32, 64 bit formats
enum class PixelFormat : u32
{
    // Ordinary 8 bit formats
    a8 = 0,
    r8,
    r8i,
    r8u,
    r8s,

    // Ordinary 16 bit formats
    r16,
    r16i,
    r16u,
    r16s,
    r16f,
    rg8,
    rg8i,
    rg8u,
    rg8s,

    // Ordinary 32 bit formats
    r32u,
    r32i,
    r32f,
    rg16,
    rg16i,
    rg16u,
    rg16s,
    rg16f,
    rgba8,
    rgba8i,
    rgba8u,
    rgba8s,
    bgra8,

    // Ordinary 64 bit formats
    rg32u,
    rg32s,
    rg32f,
    rgba16,
    rgba16i,
    rgba16u,
    rgba16s,
    rgba16f,

    // Ordinary 128 bit formats
    rgba32u,
    rgba32i,
    rgba32f,

    // Depth and stencil formats
    d16,
    d32f,
    s8,
    d24s8,
    d32s8,

    invalid,
    unknown
};

constexpr inline bool is_depth_format(const PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::d16:
        case PixelFormat::d32f:
        case PixelFormat::d24s8:
        case PixelFormat::d32s8:
        {
            return true;
        }
        default: break;
    }

    return false;
}

constexpr inline bool is_depth_stencil_format(const PixelFormat format)
{
    return is_depth_format(format) || format == PixelFormat::s8;
}

BEE_FLAGS(ColorWriteMask, u32)
{
    none    = 0u,
    alpha   = 1u << 0u,
    blue    = 1u << 1u,
    green   = 1u << 2u,
    red     = 1u << 3u,
    all     = none | alpha | blue | green | red,
    unknown
};

enum class PrimitiveType : u32
{
    point = 0,
    line,
    line_strip,
    triangle,
    triangle_strip,
    unknown
};

enum class AttachmentType
{
    undefined,
    color,
    depth_stencil,
    present
};

enum class IndexFormat : u32
{
    none,
    uint16,
    uint32
};

enum class VertexFormat : u32
{
    float1 = 0,
    float2,
    float3,
    float4,
    byte1,
    byte2,
    byte3,
    byte4,
    ubyte1,
    ubyte2,
    ubyte3,
    ubyte4,
    short1,
    short2,
    short3,
    short4,
    ushort1,
    ushort2,
    ushort3,
    ushort4,
    half1,
    half2,
    half3,
    half4,
    int1,
    int2,
    int3,
    int4,
    uint1,
    uint2,
    uint3,
    uint4,
    invalid,
    unknown
};

inline BEE_TRANSLATION_TABLE(vertex_format_component_count, VertexFormat, u32, VertexFormat::unknown,
    1,  // float1
    2,  // float2
    3,  // float3
    4,  // float4
    1,  // byte1
    2,  // byte2
    3,  // byte3
    4,  // byte4
    1,  // ubyte1
    2,  // ubyte2
    3,  // ubyte3
    4,  // ubyte4
    1,  // short1
    2,  // short2
    3,  // short3
    4,  // short4
    1,  // ushort1
    2,  // ushort2
    3,  // ushort3
    4,  // ushort4
    1,  // half1
    2,  // half2
    3,  // half3
    4,  // half4
    1,  // int1
    2,  // int2
    3,  // int3
    4,  // int4
    1,  // uint1
    2,  // uint2
    3,  // uint3
    4,  // uint4
    0   // invalid
)

inline BEE_TRANSLATION_TABLE(vertex_format_size, VertexFormat, u32, VertexFormat::unknown,
    sizeof(float) * 1,  // float1
    sizeof(float) * 2,  // float2
    sizeof(float) * 3,  // float3
    sizeof(float) * 4,  // float4
    sizeof(i8) * 1,     // byte1
    sizeof(i8) * 2,     // byte2
    sizeof(i8) * 3,     // byte3
    sizeof(i8) * 4,     // byte4
    sizeof(u8) * 1,     // ubyte1
    sizeof(u8) * 2,     // ubyte2
    sizeof(u8) * 3,     // ubyte3
    sizeof(u8) * 4,     // ubyte4
    sizeof(i16) * 1,    // short1
    sizeof(i16) * 2,    // short2
    sizeof(i16) * 3,    // short3
    sizeof(i16) * 4,    // short4
    sizeof(u16) * 1,    // ushort1
    sizeof(u16) * 2,    // ushort2
    sizeof(u16) * 3,    // ushort3
    sizeof(u16) * 4,    // ushort4
    sizeof(u16) * 1,    // half1
    sizeof(u16) * 2,    // half2
    sizeof(u16) * 3,    // half3
    sizeof(u16) * 4,    // half4
    sizeof(i32) * 1,    // int1
    sizeof(i32) * 2,    // int2
    sizeof(i32) * 3,    // int3
    sizeof(i32) * 4,    // int4
    sizeof(u32) * 1,    // uint1
    sizeof(u32) * 2,    // uint2
    sizeof(u32) * 3,    // uint3
    sizeof(u32) * 4,    // uint4
    0                   // invalid
)

inline BEE_TRANSLATION_TABLE(vertex_format_string, VertexFormat, const char*, VertexFormat::unknown,
    "float1",   // float1
    "float2",   // float2
    "float3",   // float3
    "float4",   // float4
    "byte1",    // byte1
    "byte2",    // byte2
    "byte3",    // byte3
    "byte4",    // byte4
    "ubyte1",   // ubyte1
    "ubyte2",   // ubyte2
    "ubyte3",   // ubyte3
    "ubyte4",   // ubyte4
    "short1",   // short1
    "short2",   // short2
    "short3",   // short3
    "short4",   // short4
    "ushort1",  // ushort1
    "ushort2",  // ushort2
    "ushort3",  // ushort3
    "ushort4",  // ushort4
    "half1",    // half1
    "half2",    // half2
    "half3",    // half3
    "half4",    // half4
    "int1",     // int1
    "int2",     // int2
    "int3",     // int3
    "int4",     // int4
    "uint1",    // uint1
    "uint2",    // uint2
    "uint3",    // uint3
    "uint4",    // uint4
    "invalid"   // invalid
)

enum class StepFunction : u32
{
    per_vertex = 0,
    per_instance,
    unknown
};

enum class DeviceMemoryUsage : u32
{
    gpu_only,
    cpu_only,
    cpu_to_gpu,
    gpu_to_cpu,
    unknown
};

BEE_FLAGS(BufferType, u32)
{
    unknown         = 0u,
    vertex_buffer   = 1u << 0u,
    index_buffer    = 1u << 1u,
    uniform_buffer  = 1u << 2u,
    transfer_dst    = 1u << 3u,
    transfer_src    = 1u << 4u,
    any             = vertex_buffer | index_buffer | uniform_buffer,
};

enum class LoadOp : u32
{
    load = 0,
    clear,
    dont_care,
    unknown
};

enum class StoreOp : u32
{
    store = 0,
    dont_care,
    unknown
};

enum class TextureType : u32
{
    tex1d,
    tex1d_array,
    tex2d,
    tex2d_array,
    tex2d_multisample,
    cube,
    cube_array,
    tex3d,
    unknown
};

BEE_FLAGS(ShaderStage, u32)
{
    unknown     = 0u,
    vertex      = 1u << 0u,
    fragment    = 1u << 1u,
    geometry    = 1u << 2u,
    compute     = 1u << 3u,
    graphics    = vertex | fragment | geometry,
    all         = graphics | compute
};


BEE_FLAGS(TextureUsage, u32)
{
    unknown                     = 0u,
    transfer_src                = 1u << 0u,
    transfer_dst                = 1u << 1u,
    color_attachment            = 1u << 2u,
    depth_stencil_attachment    = 1u << 3u,
    sampled                     = 1u << 4u,
    storage                     = 1u << 5u,
    input_attachment            = 1u << 6u
};

enum class PhysicalDeviceVendor
{
    AMD,
    ImgTec,
    NVIDIA,
    ARM,
    Qualcomm,
    Intel,
    unknown
};

inline BEE_TRANSLATION_TABLE(gpu_vendor_string, PhysicalDeviceVendor, const char*, PhysicalDeviceVendor::unknown,
    "AMD", // AMD
    "ImgTec", // ImgTec
    "NVIDIA", // NVIDIA
    "ARM", // ARM
    "Qualcomm", // Qualcomm
    "Intel", // Intel
)

enum class PhysicalDeviceType
{
    other,
    integrated,
    discrete,
    virtual_gpu,
    unknown
};


inline BEE_TRANSLATION_TABLE(gpu_type_string, PhysicalDeviceType, const char*, PhysicalDeviceType::unknown,
    "Other", // other
    "Integrated", // integrated
    "Discrete", // discrete
    "Virtual GPU", // virtual_gpu
)

enum class MinMagFilter
{
    nearest,
    linear,
    unknown
};

enum class MipMapMode
{
    none,
    nearest,
    linear,
    unknown
};

enum class AddressMode
{
    repeat,
    mirrored_repeat,
    clamp_to_edge,
    clamp_to_border,
    mirror_clamp_to_edge,
    unknown
};

enum class BorderColor
{
    transparent_black,
    opaque_black,
    opaque_white,
    unknown
};

enum class ResourceBindingType
{
    /// controls an images filtering parameters
    sampler,
    /// Combined image and sampler in a single descriptor
    combined_texture_sampler,
    /// an image that can be sampled by a separate sampler object
    sampled_texture,
    /// non-filtered (i.e. no sampler) stpres and loads on image memory
    storage_texture,
    /// texel buffer that allows read-only operations
    uniform_texel_buffer,
    /// texel buffer that allows read-write operations
    storage_texel_buffer,
    /// buffer that allows read-only operations
    uniform_buffer,
    /// buffer that allows read-write operations
    storage_buffer,
    /// uniform buffer that changes usually once per frame
    dynamic_uniform_buffer,
    /// storage buffer that changes usually once per frame
    dynamic_storage_buffer,
    /// Allows loads in framebuffer-local operations in fragment shaders
    input_attachment,
    unknown
};

constexpr bool is_buffer_binding(const ResourceBindingType type)
{
    return type >= ResourceBindingType::uniform_buffer && type < ResourceBindingType::input_attachment;
}

constexpr bool is_texture_binding(const ResourceBindingType type)
{
    return type < ResourceBindingType::uniform_buffer;
}

constexpr bool is_sampler_binding(const ResourceBindingType type)
{
    return type < ResourceBindingType::sampled_texture;
}

BEE_FLAGS(TextureBlitOptions, u32)
{
    none    = 1u << 0u,
    color   = 1u << 1u,
    depth   = 1u << 2u,
    stencil = 1u << 3u,
    unknown = 0
};


enum class GpuResourceState
{
    undefined,
    general,
    color_attachment,
    vertex_buffer,
    uniform_buffer,
    index_buffer,
    depth_read,
    depth_write,
    shader_read_only,
    indirect_argument,
    transfer_src,
    transfer_dst,
    present,
    unknown
};

enum class GpuBarrierType
{
    texture,
    buffer,
    memory,
    unknown
};


enum class CommandPoolHint
{
    transient,
    allow_individual_reset,
    unknown
};

enum class CommandStreamReset
{
    none,
    release_resources,
    unknown
};

enum class FenceState
{
    signaled,
    unsignaled,
    device_lost,
    unknown
};

enum class FenceWaitType
{
    all,
    at_least_one
};

enum class CommandBufferUsage
{
    default_usage,
    submit_once,
    simultaneous_usage,
    unknown
};


/*
 ********************************************************
 *
 * # GPU backend structs
 *
 * Structs used as parameters, descriptors, or info
 * used with GPU backend functions
 *
 ********************************************************
 */
struct Extent
{
    u32 width { 0 };
    u32 height { 0 };
    u32 depth { 0 };

    static inline Extent from_platform_size(const PlatformSize& size)
    {
        return Extent { sign_cast<u32>(size.width), sign_cast<u32>(size.height), 0 };
    }
};

union ClearValue
{
    float  color[4];

    struct DepthStencilClearValue
    {
        float   depth;
        u32     stencil;
    } depth_stencil { 0.0f, 0 };

    ClearValue() = default;

    ClearValue(const float r, const float g, const float b, const float a)
    {
        color[0] = r;
        color[1] = g;
        color[2] = b;
        color[3] = a;
    }

    ClearValue(const float d, const u32 s)
    {
        depth_stencil.depth = d;
        depth_stencil.stencil = s;
    }
};

struct Viewport
{
    float x { 0.0f };
    float y { 0.0f };
    float width { 0.0f };
    float height { 0.0f };
    float min_depth { 0.0f };
    float max_depth { 0.0f };

    Viewport() = default;

    Viewport(const float new_x, const float new_y, const float new_width, const float new_height)
        : x(new_x),
          y(new_y),
          width(new_width),
          height(new_height)
    {}

    Viewport(const float new_x, const float new_y, const float new_width, const float new_height, const float new_min_depth, const float new_max_depth)
        : Viewport(new_x, new_y, new_width, new_height)
    {
        min_depth = new_min_depth;
        max_depth = new_max_depth;
    }
};

struct RenderRect
{
    i32 x_offset { 0 };
    i32 y_offset { 0 };
    u32 width { 0 };
    u32 height { 0 };

    RenderRect() = default;

    RenderRect(const i32 in_x_offset, const i32 in_y_offset, const u32 in_width, const u32 in_height)
        : x_offset(in_x_offset),
          y_offset(in_y_offset),
          width(in_width),
          height(in_height)
    {}

    static RenderRect from_platform_size(const PlatformSize& size)
    {
        return { 0, 0, sign_cast<u32>(size.width), sign_cast<u32>(size.height) };
    }
};

/**
 * Contains information about a given physical GPU
 */
struct PhysicalDeviceInfo // NOLINT
{
    static constexpr i32    max_name_size = 256;

    i32                     id { -1 };
    char                    name[max_name_size];
    char                    api_version[max_name_size];
    PhysicalDeviceVendor    vendor;
    PhysicalDeviceType      type;
};


struct DeviceCreateInfo
{
    i32     physical_device_id { -1 };
    bool    enable_depth_clamp { false };
    bool    enable_sampler_anisotropy { true };
    bool    enable_sample_rate_shading { false };
};


struct SwapchainCreateInfo
{
    PixelFormat     texture_format { PixelFormat::unknown };
    Extent          texture_extent;
    TextureUsage    texture_usage { TextureUsage::color_attachment };
    u32             texture_array_layers { 1 };
    bool            vsync { false };
    WindowHandle    window;
    const char*     debug_name { nullptr };
};


struct BufferCreateInfo
{
    u32                 size { 0 };
    BufferType          type { BufferType::unknown };
    DeviceMemoryUsage   memory_usage { DeviceMemoryUsage::unknown };
};

template <>
struct Hash<BufferCreateInfo>
{
    inline u32 operator()(const BufferCreateInfo& key)
    {
        HashState state;
        state.add(key.size);
        state.add(key.type);
        state.add(key.memory_usage);
        return state.end();
    }
};


struct TextureCreateInfo
{
    TextureType         type { TextureType::unknown };
    TextureUsage        usage { TextureUsage::unknown };
    PixelFormat         format { PixelFormat::bgra8 };
    DeviceMemoryUsage   memory_usage { DeviceMemoryUsage::unknown };
    u32                 width { 0 };
    u32                 height { 0 };
    u32                 depth { 1 };
    u32                 mip_count { 1 };
    u32                 array_element_count { 1 };
    u32                 sample_count { 1 };
    const char*         debug_name { nullptr };
};

template <>
struct Hash<TextureCreateInfo>
{
    inline u32 operator()(const TextureCreateInfo& key)
    {
        HashState state;
        state.add(key.type);
        state.add(key.usage);
        state.add(key.format);
        state.add(key.width);
        state.add(key.height);
        state.add(key.depth);
        state.add(key.mip_count);
        state.add(key.array_element_count);
        state.add(key.sample_count);
        return state.end();
    }
};


struct TextureViewCreateInfo
{
    TextureHandle   texture;
    TextureType     type { TextureType::unknown };
    PixelFormat     format { PixelFormat::unknown };
    u32             mip_level_offset { 0 };
    u32             mip_level_count { 1 };
    u32             array_element_offset { 0 };
    u32             array_element_count { 1 };
    const char*     debug_name { nullptr };
};


struct AttachmentDescriptor
{
    AttachmentType      type { AttachmentType::undefined };
    PixelFormat         format { PixelFormat::invalid };
    LoadOp              load_op { LoadOp::dont_care };
    StoreOp             store_op { StoreOp::dont_care };
    u32                 samples { 1 };
};


struct SubPassDescriptor // NOLINT
{
    u32 input_attachment_count { 0 };
    u32 input_attachments[BEE_GPU_MAX_ATTACHMENTS];
    u32 color_attachment_count { 1 };
    u32 color_attachments[BEE_GPU_MAX_ATTACHMENTS];
    u32 resolve_attachment_count { 0 };
    u32 resolve_attachments[BEE_GPU_MAX_ATTACHMENTS];
    u32 preserve_attachment_count { 0 };
    u32 preserve_attachments[BEE_GPU_MAX_ATTACHMENTS];
    u32 depth_stencil { BEE_GPU_MAX_ATTACHMENTS };
};


struct RenderPassCreateInfo
{
    u32                         attachment_count { 0 };
    AttachmentDescriptor        attachments[BEE_GPU_MAX_ATTACHMENTS];

    u32                         subpass_count { 0 };
    const SubPassDescriptor*    subpasses { nullptr };
};


template <>
struct Hash<RenderPassCreateInfo>
{
    inline u32 operator()(const RenderPassCreateInfo& key)
    {
        HashState hash;

        hash.add(key.attachment_count);
        hash.add(key.attachments, sizeof(AttachmentDescriptor) * key.attachment_count);
        hash.add(key.subpass_count);

        for (u32 sp = 0; sp < key.subpass_count; ++sp)
        {
            hash.add(key.subpasses[sp].input_attachment_count);
            hash.add(key.subpasses[sp].input_attachments, sizeof(u32) * key.subpasses[sp].input_attachment_count);
            hash.add(key.subpasses[sp].color_attachment_count);
            hash.add(key.subpasses[sp].color_attachments, sizeof(u32) * key.subpasses[sp].color_attachment_count);
            hash.add(key.subpasses[sp].resolve_attachment_count);
            hash.add(key.subpasses[sp].resolve_attachments, sizeof(u32) * key.subpasses[sp].resolve_attachment_count);
            hash.add(key.subpasses[sp].preserve_attachment_count);
            hash.add(key.subpasses[sp].preserve_attachments, sizeof(u32) * key.subpasses[sp].preserve_attachment_count);
            hash.add(key.subpasses[sp].depth_stencil);
        }

        return hash.end();
    }
};

// TODO(Jacob): buffers
// TODO(Jacob): render passes

struct StencilOpDescriptor
{
    StencilOp       fail_op { StencilOp::keep };
    StencilOp       pass_op { StencilOp::keep };
    StencilOp       depth_fail_op { StencilOp::keep };
    CompareFunc     compare_func { CompareFunc::always };
    u32             read_mask { ~0u };
    u32             write_mask { ~0u };
    u32             reference { 0 };
};


struct BlendStateDescriptor
{
    bool            blend_enabled { false };
    PixelFormat     format { PixelFormat::invalid };
    ColorWriteMask  color_write_mask { ColorWriteMask::all };
    BlendOperation  alpha_blend_op { BlendOperation::add };
    BlendOperation  color_blend_op { BlendOperation::add };
    BlendFactor     src_blend_alpha { BlendFactor::one };
    BlendFactor     src_blend_color { BlendFactor::one };
    BlendFactor     dst_blend_alpha { BlendFactor::zero };
    BlendFactor     dst_blend_color { BlendFactor::zero };
};


struct VertexAttributeDescriptor
{
    // If the order of these gets changed, ShaderCompiler needs to be updated accordingly
    // at the line with the comment 'Reflect vertex descriptor' otherwise all shaders will be busted
    VertexFormat    format { VertexFormat::invalid };
    u32             offset { 0 };
    u32             location { 0 };
    u32             layout { 0 };
};


// Describes the layout of a single vertex buffer
struct VertexLayoutDescriptor
{
    // If the order of these gets changed, ShaderCompiler needs to be updated accordingly
    // at the line with the comment 'Reflect vertex descriptor' otherwise all shaders will be busted
    u32             index { 0 };
    u32             stride { 0 };
    StepFunction    step_function { StepFunction::per_vertex };
};


/// Describes the layout of vertices for all vertex buffers bound for a given pipeline state.
///
/// Example:
/// A pipeline state may require 2 vertex buffers bound at a time - 1 with per-vertex data
/// (pos, color, uv etc.) and one with per-instance data (matrix data, group color etc.). These
/// would be bound at slot 0 & 1 respectively and therefore require `layout_count = 2` and the
/// driver would expect 2 elements in the `layouts` array to be filled in. Each of these layouts
/// would specify attributes for each layout
struct VertexDescriptor
{
    static constexpr u8 max_attributes = 32; // 8 unique attributes per layout
    static constexpr u8 max_layouts = 4;

    // If the order of these gets changed, ShaderCompiler needs to be updated accordingly
    // at the line with the comment 'Reflect vertex descriptor' otherwise all shaders will be busted
    u32                         layout_count { 0 };
    u32                         attribute_count { 0 };
    VertexLayoutDescriptor      layouts[max_layouts];
    VertexAttributeDescriptor   attributes[max_attributes];
};


struct RasterStateDescriptor
{
    FillMode                fill_mode { FillMode::solid };
    CullMode                cull_mode { CullMode::back };
    float                   line_width { 1.0f }; // this is more of a hint and isn't supported by all backends
    bool                    front_face_ccw { false };
    // ignored if depth-clipping/clamping isn't supported by the backend or !defined(SKY_CONFIG_ALLOW_DEPTH_CLAMP)
    bool                    depth_clamp_enabled { false };
    bool                    depth_bias_enabled { false };
    float                   depth_bias { 0.0f };
    float                   depth_slope_factor { 0.0f };
    float                   depth_bias_clamp { 0.0f };
};


struct DepthStencilStateDescriptor
{
    CompareFunc             depth_compare_func { CompareFunc::less };
    bool                    depth_test_enabled{ false };
    bool                    depth_write_enabled{ false };
    bool                    depth_bounds_test_enabled { false };
    bool                    stencil_test_enabled{ false };
    StencilOpDescriptor     front_face_stencil;
    StencilOpDescriptor     back_face_stencil;
    float                   min_depth_bounds { 0.0f };
    float                   max_depth_bounds { 0.0f };
};

struct MultisampleStateDescriptor
{
    u32     sample_count { 1 };
    bool    sample_shading_enabled { false }; // enables sample shading only if the platform supports it
    float   sample_shading { 0.0f };
    u32     sample_mask { 0 };
    bool    alpha_to_coverage_enabled { false };
    bool    alpha_to_one_enabled { false };
};

struct ResourceDescriptor
{
    u32                 binding { 0 };
    ResourceBindingType type { ResourceBindingType::unknown };
    u32                 element_count { 0 };
    ShaderStage         shader_stages { ShaderStage::unknown };
};

struct ResourceLayoutDescriptor
{
    u32                         resource_count { 0 };
    const ResourceDescriptor*   resources { nullptr };
};


template<>
struct Hash<ResourceLayoutDescriptor>
{
    inline u32 operator()(const ResourceLayoutDescriptor& key) const
    {
        HashState hash;
        hash.add(key.resource_count);
        hash.add(key.resources, sizeof(ResourceDescriptor) * key.resource_count);
        return hash.end();
    }
};


struct PushConstantRange
{
    ShaderStage shader_stages { ShaderStage::unknown };
    u32         offset { 0 };
    u32         size { 0 };
};

struct PipelineStateCreateInfo
{
    // Primitive type to use when rendering while this pipeline state is used
    PrimitiveType                   primitive_type { PrimitiveType::triangle };

    // A render pass that describes a compatible set of color & depth attachments
    RenderPassHandle                compatible_render_pass;

    // Index into the render passes subpass array that this pipeline uses (see: RenderPassCreateInfo::subpasses)
    u32                             subpass_index { 0 };

    // Describes the vertex layouts and input attributes used by vertex buffers
    VertexDescriptor                vertex_description;

    // Shaders used at the various pipeline stages
    ShaderHandle                    vertex_stage;
    ShaderHandle                    fragment_stage;

    // Render state
    RasterStateDescriptor           raster_state;
    MultisampleStateDescriptor      multisample_state;
    DepthStencilStateDescriptor     depth_stencil_state;

    // Blend state
    u32                             color_blend_state_count { 1 };
    BlendStateDescriptor            color_blend_states[BEE_GPU_MAX_ATTACHMENTS];

    // Resource binding layout the pipeline is expecting
    u32                             resource_layout_count { 0 };
    const ResourceLayoutDescriptor* resource_layouts { nullptr };

    // Ranges of push constants the pipeline can use
    u32                             push_constant_range_count { 0 };
    const PushConstantRange*        push_constant_ranges { nullptr };
};


struct CommandPoolCreateInfo
{
    /**
     * Hint for specifying to the backend which queues the command pool will be using to allocate command buffers.
     */
    QueueType       used_queues_hint { QueueType::none };
    /// Hint for specifying which command buffer allocation strategy to use - may not be used by the backend API
    CommandPoolHint pool_hint { CommandPoolHint::allow_individual_reset };
};


class CommandBuffer;
class JobGroup;


struct SubmitInfo
{
    FenceHandle                     fence;
    u32                             command_buffer_count { 0 };
    const GpuCommandBuffer* const*  command_buffers {nullptr };
};


struct GpuBufferBarrier
{
    BufferHandle    handle;
    u32             offset { 0 };
    u32             size { 0 }; // if size is zero the barrier range will be buffer_size - barrier.offset
};

union GpuBarrier
{
    GpuBufferBarrier    buffer;
    TextureHandle       texture;
};

struct GpuTransition
{
    GpuBarrierType      barrier_type { GpuBarrierType::unknown };
    GpuBarrier          barrier;
    GpuResourceState    old_state { GpuResourceState::unknown };
    GpuResourceState    new_state { GpuResourceState::unknown };
};


/*
 ********************************************************
 *
 * # GPU backend API
 *
 * cross-platform graphics API that abstracts
 * Vulkan/D3D12/Metal etc.
 *
 ********************************************************
 */
BEE_RUNTIME_API bool gpu_init();

BEE_RUNTIME_API void gpu_destroy();

BEE_RUNTIME_API i32 gpu_enumerate_physical_devices(PhysicalDeviceInfo* dst_buffer, const i32 buffer_size);

BEE_RUNTIME_API DeviceHandle gpu_create_device(const DeviceCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_device(const DeviceHandle& handle);

BEE_RUNTIME_API void gpu_device_wait(const DeviceHandle& handle);

BEE_RUNTIME_API SwapchainHandle gpu_create_swapchain(const DeviceHandle& device_handle, const SwapchainCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_swapchain(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle);

BEE_RUNTIME_API TextureHandle gpu_acquire_swapchain_texture(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle);

BEE_RUNTIME_API TextureViewHandle gpu_get_swapchain_texture_view(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle);

BEE_RUNTIME_API Extent gpu_get_swapchain_extent(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle);

BEE_RUNTIME_API RenderPassHandle gpu_create_render_pass(const DeviceHandle& device_handle, const RenderPassCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_render_pass(const DeviceHandle& device_handle, const RenderPassHandle& handle);

BEE_RUNTIME_API PipelineStateHandle gpu_create_pipeline_state(const DeviceHandle& device_handle, const PipelineStateCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_pipeline_state(const DeviceHandle& device, const PipelineStateHandle& pipeline_handle);

BEE_RUNTIME_API BufferHandle gpu_create_buffer(const DeviceHandle& device, const BufferCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_buffer(const DeviceHandle& device, const BufferHandle& buffer);

BEE_RUNTIME_API TextureHandle gpu_create_texture(const DeviceHandle& device_handle, const TextureCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle);

BEE_RUNTIME_API TextureViewHandle gpu_create_texture_view(const DeviceHandle& device_handle, const TextureViewCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_texture_view(const DeviceHandle& device_handle, const TextureViewHandle& texture_view_handle);

BEE_RUNTIME_API CommandPoolHandle gpu_create_command_pool(const DeviceHandle& device_handle, const CommandPoolCreateInfo& create_info);

BEE_RUNTIME_API void gpu_destroy_command_pool(const DeviceHandle& device_handle, const CommandPoolHandle& handle);

BEE_RUNTIME_API void gpu_reset_command_pool(const DeviceHandle& device_handle, const CommandPoolHandle& handle);

BEE_RUNTIME_API GpuCommandBuffer* gpu_create_command_buffer(const DeviceHandle& device_handle, const CommandPoolHandle& pool_handle, const QueueType required_queue_type);

BEE_RUNTIME_API void gpu_destroy_command_buffer(GpuCommandBuffer* command_buffer);

BEE_RUNTIME_API void gpu_reset_command_buffer(GpuCommandBuffer* command_buffer, const CommandStreamReset hint = CommandStreamReset::none);

BEE_RUNTIME_API void gpu_begin_command_buffer(GpuCommandBuffer* command_buffer, const CommandBufferUsage usage = CommandBufferUsage::default_usage);

BEE_RUNTIME_API void gpu_end_command_buffer(GpuCommandBuffer* command_buffer);

BEE_RUNTIME_API FenceHandle gpu_create_fence(const DeviceHandle& device_handle, const FenceState initial_state = FenceState::unsignaled);

BEE_RUNTIME_API void gpu_destroy_fence(const DeviceHandle& device_handle, const FenceHandle& fence);

BEE_RUNTIME_API void gpu_wait_for_fence(const DeviceHandle& device_handle, const FenceHandle& fence, const u64 timeout = limits::max<u64>());

BEE_RUNTIME_API bool gpu_wait_for_fences(const DeviceHandle& device_handle, const u32 count, const FenceHandle* fence_handles, const FenceWaitType wait_type, const u64 timeout = limits::max<u64>());

BEE_RUNTIME_API void gpu_reset_fences(const DeviceHandle& device_handle, const u32 count, const FenceHandle* fence_handles);

BEE_RUNTIME_API void gpu_reset_fence(const DeviceHandle& device_handle, const FenceHandle& fence_handle);

BEE_RUNTIME_API FenceState gpu_get_fence_state(const DeviceHandle& device_handle, const FenceHandle& fence_handle);

BEE_RUNTIME_API void gpu_submit(JobGroup* wait_handle, const DeviceHandle& device_handle, const SubmitInfo& info);

BEE_RUNTIME_API void gpu_submit(const DeviceHandle& device_handle, const SubmitInfo& info);

BEE_RUNTIME_API void gpu_present(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle);

BEE_RUNTIME_API void gpu_commit_frame(const DeviceHandle& device_handle);

BEE_RUNTIME_API i32 gpu_get_current_frame(const DeviceHandle& device_handle);


/*
 ********************************************************
 *
 * # GPU Command buffer API
 *
 ********************************************************
 */

enum class GpuCommandType
{
    begin_render_pass,
    end_render_pass,
    copy_buffer,
    copy_texture,
    draw,
    draw_indexed,
    transition_resources,
    begin,
    end,
    unknown
};


struct GpuCommandHeader
{
    u64             sort_key { 0 };
    GpuCommandType  type {GpuCommandType::unknown };
    QueueType       queue_type { QueueType::none };
    void*           data { nullptr };
};

template <GpuCommandType T, QueueType Q>
struct GpuCmd
{
    static constexpr GpuCommandType command_type = T;
    static constexpr QueueType      queue_type = Q;
};


struct DrawItem // NOLINT
{
    PipelineStateHandle pipeline;

    u32                 index_count { 0 };

    u32                 vertex_buffer_count { 0 };
    u32                 vertex_buffer_offsets[BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS];
    BufferHandle        vertex_buffers[BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS];
    u32                 vertex_buffer_bindings[BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS];

    u32                 index_buffer_offset { 0 };
    IndexFormat         index_buffer_format { IndexFormat::none };
    BufferHandle        index_buffer;

    bool                set_viewport { false };
    Viewport            viewport;

    bool                set_scissor { false };
    RenderRect          scissor;
};


struct CmdBeginRenderPass final : public GpuCmd<GpuCommandType::begin_render_pass, QueueType::graphics>
{
    RenderPassHandle    pass;
    RenderRect          render_area;
    u32                 attachment_count { 0 };
    u32                 clear_value_count { 0 };
    TextureViewHandle   attachments[BEE_GPU_MAX_ATTACHMENTS];
    ClearValue          clear_values[BEE_GPU_MAX_ATTACHMENTS];
};

struct CmdEndRenderPass final : public GpuCmd<GpuCommandType::end_render_pass, QueueType::graphics> {};

struct CmdCopyBuffer final : public GpuCmd<GpuCommandType::copy_buffer, QueueType::transfer>
{
    BufferHandle    src;
    BufferHandle    dst;
    u32             src_offset { 0 };
    u32             dst_offset { 0 };
    u32             size { 0 };
};

struct CmdDraw final : public GpuCmd<GpuCommandType::draw, QueueType::graphics>
{
    u32         first_vertex { 0 };
    u32         vertex_count { 0 };
    u32         first_instance { 0 };
    u32         instance_count { 0 };
    DrawItem    item;
};

struct CmdDrawIndexed final : public GpuCmd<GpuCommandType::draw_indexed, QueueType::graphics>
{
    u32         first_index { 0 };
    u32         index_count { 0 };
    u32         vertex_offset { 0 };
    u32         first_instance { 0 };
    u32         instance_count { 0 };
    DrawItem    item;
};

struct CmdTransitionResources final : public GpuCmd<GpuCommandType::transition_resources, QueueType::all>
{
    u32                     count { 0 };
    const GpuTransition*    transitions { nullptr };
};


struct CmdEnd final : public GpuCmd<GpuCommandType::end, QueueType::all> {};

/*
 * Low-level raw GPU command recording with native command buffer
 */

BEE_RUNTIME_API void gpu_record_command(GpuCommandBuffer* cmd, const CmdBeginRenderPass& data);
BEE_RUNTIME_API void gpu_record_command(GpuCommandBuffer* cmd, const CmdEndRenderPass& data = CmdEndRenderPass{});
BEE_RUNTIME_API void gpu_record_command(GpuCommandBuffer* cmd, const CmdCopyBuffer& data);
BEE_RUNTIME_API void gpu_record_command(GpuCommandBuffer* cmd, const CmdDraw& data);
BEE_RUNTIME_API void gpu_record_command(GpuCommandBuffer* cmd, const CmdDrawIndexed& data);
BEE_RUNTIME_API void gpu_record_command(GpuCommandBuffer* cmd, const CmdTransitionResources& data);


} // namespace bee