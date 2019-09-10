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


enum class BackendAPI
{
    none,
    vulkan
};

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
    alpha   = (1u << 0u),
    blue    = (1u << 1u),
    green   = (1u << 2u),
    red     = (1u << 3u),
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
    vertex_buffer   = (1u << 0u),
    index_buffer    = (1u << 1u),
    uniform_buffer  = (1u << 2u),
    transfer_dst    = (1u << 3u),
    transfer_src    = (1u << 4u),
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
    vertex      = (1u << 0u),
    fragment    = (1u << 1u),
    geometry    = (1u << 2u),
    compute     = (1u << 3u),
    graphics    = vertex | fragment | geometry,
    all         = graphics | compute
};

enum class ShaderType : u32
{
    string_src,
    byte_src,
    unknown
};

BEE_FLAGS(TextureUsage, u32)
{
    unknown                     = 0u,
    transfer_src                = (1u << 0u),
    transfer_dst                = (1u << 1u),
    color_attachment            = (1u << 2u),
    depth_stencil_attachment    = (1u << 3u),
    sampled                     = (1u << 4u),
    storage                     = (1u << 5u),
    input_attachment            = (1u << 6u)
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

enum class ResourceType
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

constexpr bool is_buffer_binding(const ResourceType type)
{
    return type >= ResourceType::uniform_buffer && type < ResourceType::input_attachment;
}

constexpr bool is_texture_binding(const ResourceType type)
{
    return type < ResourceType::uniform_buffer;
}

constexpr bool is_sampler_binding(const ResourceType type)
{
    return type < ResourceType::sampled_texture;
}

BEE_FLAGS(TextureBlitOptions, u32)
{
    none = (1u << 0),
    color = (1u << 1),
    depth = (1u << 2),
    stencil = (1u << 3),
    unknown = 0
};


enum class ResourceState
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

enum class BarrierType
{
    texture,
    buffer,
    memory,
    unknown
};


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
BEE_DEFINE_RAW_HANDLE_U32(Device);
BEE_DEFINE_VERSIONED_HANDLE(Swapchain);
BEE_DEFINE_VERSIONED_HANDLE(Texture);
BEE_DEFINE_VERSIONED_HANDLE(TextureView);
BEE_DEFINE_VERSIONED_HANDLE(Buffer);
BEE_DEFINE_VERSIONED_HANDLE(BufferView);
BEE_DEFINE_VERSIONED_HANDLE(RenderPass);
BEE_DEFINE_VERSIONED_HANDLE(Shader);
BEE_DEFINE_VERSIONED_HANDLE(PipelineState);

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

/**
 * Contains information about a given physical GPU
 */
struct PhysicalDeviceInfo
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


struct TextureCreateInfo
{
    TextureType         type { TextureType::unknown };
    TextureUsage        usage { TextureUsage::unknown };
    ResourceState       initial_state { ResourceState::unknown };
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
};


struct BlendStateDescriptor
{
    bool            blend_enabled { false };
    PixelFormat     format { PixelFormat::invalid };
    ColorWriteMask  write_mask { ColorWriteMask::all };
    BlendOperation  alpha_blend_op { BlendOperation::add };
    BlendOperation  rgb_blend_op { BlendOperation::add };
    BlendFactor     src_blend_alpha { BlendFactor::one };
    BlendFactor     src_blend_rgb { BlendFactor::one };
    BlendFactor     dst_blend_alpha { BlendFactor::zero };
    BlendFactor     dst_blend_rgb { BlendFactor::zero };
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
    u32             buffer_index { 0 };
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
    bool                    stencil_test_enabled{ false };
    StencilOpDescriptor     front_face_stencil;
    StencilOpDescriptor     back_face_stencil;
};


struct PipelineStateDescriptor
{
    // Primitive type to use when rendering while this pipeline state is used
    PrimitiveType               primitive_type { PrimitiveType::triangle };
    // A render pass that describes a compatible set of color & depth attachments
    RenderPassHandle            compatible_render_pass;
    // Index into the render passes subpass array that this pipeline uses (see: RenderPassCreateInfo::subpasses)
    u32                         subpass_index { 0 };
    // Describes the vertex layouts and input attributes used by vertex buffers
    VertexDescriptor            vertex_description;
    // Shaders used at the various pipeline stages
    ShaderHandle                vertex_stage;

    ShaderHandle                fragment_stage;

    // Render state
    RasterStateDescriptor       raster_state;
    DepthStencilStateDescriptor depth_stencil_state;

    // Multisampling
    u32                         sample_count { 1 };

    // Blend state
    u32                         color_blend_state_count { 1 };
    BlendStateDescriptor        color_blend_states[BEE_GPU_MAX_ATTACHMENTS];

    // Resource binding layout the pipeline is expecting
//    ResourceLayoutDescriptor    resource_layout;
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
BEE_API bool gpu_init();

BEE_API void gpu_destroy();

BEE_API i32 gpu_enumerate_physical_devices(PhysicalDeviceInfo* dst_buffer, const i32 buffer_size);

BEE_API DeviceHandle gpu_create_device(const DeviceCreateInfo& create_info);

BEE_API void gpu_destroy_device(const DeviceHandle& handle);

BEE_API void gpu_device_wait(const DeviceHandle& handle);

BEE_API SwapchainHandle gpu_create_swapchain(const DeviceHandle& device_handle, const SwapchainCreateInfo& create_info);

BEE_API void gpu_destroy_swapchain(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle);

BEE_API TextureHandle gpu_create_texture(const DeviceHandle& device_handle, const TextureCreateInfo& create_info);

BEE_API void gpu_destroy_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle);

BEE_API TextureViewHandle gpu_create_texture_view(const DeviceHandle& device_handle, const TextureViewCreateInfo& create_info);

BEE_API void gpu_destroy_texture_view(const DeviceHandle& device_handle, const TextureViewHandle& texture_view_handle);

} // namespace bee