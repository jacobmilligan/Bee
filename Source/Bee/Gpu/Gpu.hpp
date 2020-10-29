/*
 *  GPU.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Hash.hpp"

#include "Bee/Platform/Platform.hpp"


namespace bee {


/*
 ********************************************************
 *
 * # GPU Device limits
 *
 * Limits used for storing GPU resources and objects.
 * Can be overriden as a macro in user code
 *
 ********************************************************
 */

#ifndef BEE_GPU_MAX_FRAMES_IN_FLIGHT
    #define BEE_GPU_MAX_FRAMES_IN_FLIGHT 3u
#endif // BEE_GPU_MAX_FRAMES_IN_FLIGHT

#ifndef BEE_GPU_MAX_PHYSICAL_DEVICES
    #define BEE_GPU_MAX_PHYSICAL_DEVICES 4u
#endif // BEE_GPU_MAX_PHYSICAL_DEVICES

#ifndef BEE_GPU_MAX_DEVICES
    #define BEE_GPU_MAX_DEVICES 1u
#endif // BEE_GPU_MAX_DEVICES

// based off the metal feature set - seems to be the lowest of the API's
// see: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
#define BEE_GPU_MAX_ATTACHMENTS 8u

#ifndef BEE_GPU_MAX_RESOURCE_LAYOUTS
    #define BEE_GPU_MAX_RESOURCE_LAYOUTS 8u
#endif // BEE_GPU_MAX_RESOURCE_LAYOUTS

#ifndef BEE_GPU_MAX_RESOURCE_BINDINGS
    #define BEE_GPU_MAX_RESOURCE_BINDINGS 16u
#endif // BEE_GPU_MAX_RESOURCE_BINDINGS

#ifndef BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS
    #define BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS 8u
#endif // BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS

#ifndef BEE_GPU_INITIAL_STAGING_SIZE
    #define BEE_GPU_INITIAL_STAGING_SIZE 16 * 1024 * 1024 // 16mb
#endif // BEE_GPU_INITIAL_STAGING_SIZE

#ifndef BEE_GPU_MAX_COMMAND_BUFFERS_PER_THREAD
    #define BEE_GPU_MAX_COMMAND_BUFFERS_PER_THREAD 8
#endif // BEE_GPU_MAX_COMMAND_BUFFERS_PER_THREAD

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
BEE_SPLIT_HANDLE(GpuObjectHandle, bee::u32, 24u, 8u, value, thread) BEE_REFLECT();

#define BEE_GPU_HANDLE(Name)                                                                        \
struct Name                                                                                         \
{                                                                                                   \
    explicit Name(const GpuObjectHandle object_handle) : id(object_handle.id) {}                    \
    inline Name& operator=(const GpuObjectHandle& other) { id = other.id; return *this; }           \
    inline operator GpuObjectHandle() const { return GpuObjectHandle { id }; }                      \
    inline constexpr bool operator==(const GpuObjectHandle& other) const { return id == other.id; } \
    inline constexpr bool operator!=(const GpuObjectHandle& other) const { return id != other.id; } \
    BEE_SPLIT_HANDLE_BODY(Name, bee::u32, 24u, 8u, value, thread)                                   \
}

BEE_RAW_HANDLE_U32(DeviceHandle) BEE_REFLECT();
BEE_RAW_HANDLE_U32(SwapchainHandle) BEE_REFLECT();
BEE_GPU_HANDLE(TextureHandle) BEE_REFLECT();
BEE_GPU_HANDLE(TextureViewHandle) BEE_REFLECT();
BEE_GPU_HANDLE(BufferHandle) BEE_REFLECT();
BEE_GPU_HANDLE(BufferViewHandle) BEE_REFLECT();
BEE_GPU_HANDLE(RenderPassHandle) BEE_REFLECT();
BEE_GPU_HANDLE(ShaderHandle) BEE_REFLECT();
BEE_GPU_HANDLE(PipelineStateHandle) BEE_REFLECT();
BEE_GPU_HANDLE(FenceHandle) BEE_REFLECT();
BEE_GPU_HANDLE(SamplerHandle) BEE_REFLECT();
BEE_GPU_HANDLE(ResourceBindingHandle) BEE_REFLECT();
struct RawCommandBuffer;

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
enum class GpuApi
{
    none,
    vulkan,
    other
};

BEE_FLAGS(QueueType, u32)
{
    none        = 0u,
    graphics    = 1u << 0u,
    compute     = 1u << 1u,
    transfer    = 1u << 2u,
    all         = graphics | compute | transfer
};

enum class BEE_REFLECT(serializable) FillMode : u32
{
    wireframe = 0,
    solid,
    unknown
};

enum class BEE_REFLECT(serializable) CullMode : u32
{
    none = 0,
    front,
    back,
    unknown
};

enum class BEE_REFLECT(serializable) CompareFunc : u32
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

enum class BEE_REFLECT(serializable) StencilOp : u32
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

enum class BEE_REFLECT(serializable) BlendFactor : u32
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

enum class BEE_REFLECT(serializable) BlendOperation : u32
{
    add = 0,
    subtract,
    reverse_subtract,
    min,
    max,
    unknown
};

// TODO(Jacob): packed formats for 16, 32, 64 bit formats
enum class BEE_REFLECT(serializable) PixelFormat : u32
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

BEE_REFLECTED_FLAGS(ColorWriteMask, u32, serializable, version = 1)
{
    none    = 0u,
    alpha   = 1u << 0u,
    blue    = 1u << 1u,
    green   = 1u << 2u,
    red     = 1u << 3u,
    all     = none | alpha | blue | green | red,
    unknown
};

enum class BEE_REFLECT(serializable) PrimitiveType : u32
{
    point = 0,
    line,
    line_strip,
    triangle,
    triangle_strip,
    unknown
};

enum class BEE_REFLECT(serializable) AttachmentType
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
    uint32,
    unknown
};

enum class BEE_REFLECT(serializable) VertexFormat : u32
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

enum class BEE_REFLECT(serializable) StepFunction : u32
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

enum class BEE_REFLECT(serializable) LoadOp : u32
{
    load = 0,
    clear,
    dont_care,
    unknown
};

enum class BEE_REFLECT(serializable) StoreOp : u32
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

BEE_REFLECTED_FLAGS(ShaderStageFlags, u32, serializable, version = 1)
{
    unknown     = 0u,
    vertex      = 1u << 0u,
    fragment    = 1u << 1u,
    geometry    = 1u << 2u,
    compute     = 1u << 3u,
    graphics    = vertex | fragment | geometry,
    all         = graphics | compute
};

enum class BEE_REFLECT(serializable, version = 1) ShaderStageIndex : i32
{
    vertex = 0,
    fragment,
    geometry,
    compute,
    count
};

static constexpr i32 gpu_shader_stage_count = static_cast<i32>(ShaderStageIndex::count);

constexpr ShaderStageFlags gpu_shader_index_to_flag(const ShaderStageIndex index)
{
    return static_cast<ShaderStageFlags>(1u << static_cast<u32>(index));
}


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

enum class BEE_REFLECT(serializable) MinMagFilter
{
    nearest,
    linear,
    unknown
};

enum class BEE_REFLECT(serializable) MipMapMode
{
    none,
    nearest,
    linear,
    unknown
};

enum class BEE_REFLECT(serializable) AddressMode
{
    repeat,
    mirrored_repeat,
    clamp_to_edge,
    clamp_to_border,
    mirror_clamp_to_edge,
    unknown
};

enum class BEE_REFLECT(serializable) BorderColor
{
    transparent_black,
    opaque_black,
    opaque_white,
    unknown
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

enum class BEE_REFLECT(serializable) ResourceBindingType
{
    /// controls an images filtering parameters
    sampler,
    /// Combined image and sampler in a single descriptor
    combined_texture_sampler,
    /// an image that can be sampled by a separate sampler object
    sampled_texture,
    /// non-filtered (i.e. no sampler) and loads on image memory
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
    // also acts as a count for static arrays
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

enum class BEE_REFLECT(serializable) ResourceBindingUpdateFrequency
{
    per_draw,
    per_frame,
    persistent
};


BEE_FLAGS(TextureBlitOptions, u32)
{
    none    = 1u << 0u,
    color   = 1u << 1u,
    depth   = 1u << 2u,
    stencil = 1u << 3u,
    unknown = 0
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
struct Offset
{
    i32 x { 0 };
    i32 y { 0 };
    i32 z { 0 };

    Offset() = default;

    Offset(const i32 x_offset, const i32 y_offset, const i32 z_offset)
        : x(x_offset),
          y(y_offset),
          z(z_offset)
    {}
};

struct Extent
{
    u32 width { 0 };
    u32 height { 0 };
    u32 depth { 0 };

    Extent() = default;

    Extent(const u32 size_width, const u32 size_height, const u32 size_depth = 1)
        : width(size_width),
          height(size_height),
          depth(size_depth)
    {}
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
    bool    enable_sampler_anisotropy { false };
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
    const char*         debug_name { nullptr };
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

struct BEE_REFLECT(serializable) SamplerCreateInfo
{
    MinMagFilter    mag_filter { MinMagFilter::nearest };
    MinMagFilter    min_filter { MinMagFilter::nearest };
    MipMapMode      mip_mode { MipMapMode::none };
    AddressMode     u_address { AddressMode::clamp_to_edge };
    AddressMode     v_address { AddressMode::clamp_to_edge };
    AddressMode     w_address { AddressMode::clamp_to_edge };
    float           lod_bias { 0.0f };
    float           lod_min { 0.0f };
    float           lod_max { limits::max<float>() };
    bool            anisotropy_enabled { false };
    float           anisotropy_max { 1.0f };
    bool            compare_enabled { false };
    CompareFunc     compare_func { CompareFunc::never };
    BorderColor     border_color { BorderColor::transparent_black };
    bool            normalized_coordinates { true };
};


struct BEE_REFLECT(serializable) AttachmentDescriptor
{
    AttachmentType      type { AttachmentType::undefined };
    PixelFormat         format { PixelFormat::invalid };
    LoadOp              load_op { LoadOp::dont_care };
    StoreOp             store_op { StoreOp::dont_care };
    u32                 samples { 1 };
};


struct BEE_REFLECT(serializable) SubPassDescriptor
{
    u32 input_attachment_count { 0 };
    u32 input_attachments[BEE_GPU_MAX_ATTACHMENTS];
    u32 color_attachment_count { 0 };
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

struct ShaderCreateInfo
{
    const char* entry { nullptr };
    size_t      code_size { 0 };
    const u8*   code { nullptr };
};

// TODO(Jacob): buffers
// TODO(Jacob): render passes

struct BEE_REFLECT(serializable) StencilOpDescriptor
{
    StencilOp       fail_op { StencilOp::keep };
    StencilOp       pass_op { StencilOp::keep };
    StencilOp       depth_fail_op { StencilOp::keep };
    CompareFunc     compare_func { CompareFunc::always };
    u32             read_mask { ~0u };
    u32             write_mask { ~0u };
    u32             reference { 0 };
};


struct BEE_REFLECT(serializable) BlendStateDescriptor
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


struct BEE_REFLECT(serializable) VertexAttributeDescriptor
{
    // If the order of these gets changed, ShaderCompiler needs to be updated accordingly
    // at the line with the comment 'Reflect vertex descriptor' otherwise all shaders will be busted
    VertexFormat    format { VertexFormat::invalid };
    u32             offset { 0 };
    u32             location { 0 };
    u32             layout { 0 };
};


// Describes the layout of a single vertex buffer
struct BEE_REFLECT(serializable) VertexLayoutDescriptor
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
struct BEE_REFLECT(serializable) VertexDescriptor
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


struct BEE_REFLECT(serializable) RasterStateDescriptor
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


struct BEE_REFLECT(serializable) DepthStencilStateDescriptor
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

struct BEE_REFLECT(serializable) MultisampleStateDescriptor
{
    u32     sample_count { 1 };
    bool    sample_shading_enabled { false }; // enables sample shading only if the platform supports it
    float   sample_shading { 0.0f };
    u32     sample_mask { 0 };
    bool    alpha_to_coverage_enabled { false };
    bool    alpha_to_one_enabled { false };
};

struct BEE_REFLECT(serializable) ResourceDescriptor
{
    u32                 binding { 0 };
    ResourceBindingType type { ResourceBindingType::unknown };
    u32                 element_count { 1 };
    ShaderStageFlags    shader_stages { ShaderStageFlags::unknown };
};

inline bool operator==(const ResourceDescriptor& lhs, const ResourceDescriptor& rhs)
{
    return lhs.binding == rhs.binding
        && lhs.type == rhs.type
        && lhs.element_count == rhs.element_count
        && lhs.shader_stages == rhs.shader_stages;
}

inline bool operator!=(const ResourceDescriptor& lhs, const ResourceDescriptor& rhs)
{
    return !(lhs == rhs);
}

struct BEE_REFLECT(serializable) ResourceLayoutDescriptor
{
    u32                 resource_count { 0 };
    ResourceDescriptor  resources[BEE_GPU_MAX_RESOURCE_BINDINGS];
};

inline bool operator==(const ResourceLayoutDescriptor& lhs, const ResourceLayoutDescriptor& rhs)
{
    if (lhs.resource_count != rhs.resource_count)
    {
        return false;
    }

    for (u32 i = 0; i < lhs.resource_count; ++i)
    {
        if (lhs.resources[i] != rhs.resources[i])
        {
            return false;
        }
    }

    return true;
}

inline bool operator!=(const ResourceLayoutDescriptor& lhs, const ResourceLayoutDescriptor& rhs)
{
    return !(lhs == rhs);
}

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

struct ResourceBindingCreateInfo
{
    ResourceBindingUpdateFrequency  update_frequency { ResourceBindingUpdateFrequency::persistent };
    const ResourceLayoutDescriptor* layout { nullptr };
};

struct ResourceBindingUpdate
{
    ResourceBindingType type { ResourceBindingType::unknown };
    u32                 binding { 0 };
    u32                 element { 0 };

    struct TextureWrite
    {
        SamplerHandle       sampler;
        TextureViewHandle   texture;
    };

    struct BufferWrite
    {
        BufferHandle    buffer;
        size_t          offset { 0 };
        size_t          size { 0 };
    };

    union
    {
        TextureWrite        texture;
        BufferWrite         buffer;
        BufferViewHandle    texel_buffer;
    };
};

struct BEE_REFLECT(serializable) PushConstantRange
{
    ShaderStageFlags    shader_stages { ShaderStageFlags::unknown };
    u32                 offset { 0 };
    u32                 size { 0 };
};

inline bool operator==(const PushConstantRange& lhs, const PushConstantRange& rhs)
{
    return lhs.shader_stages == rhs.shader_stages
        && lhs.offset == rhs.offset
        && lhs.size == rhs.size;
}

inline bool operator!=(const PushConstantRange& lhs, const PushConstantRange& rhs)
{
    return !(lhs == rhs);
}

struct BEE_REFLECT(serializable, version = 1) PipelineStateCreateInfo
{
    // Primitive type to use when rendering while this pipeline state is used
    BEE_REFLECT(id = 0, added = 1)
    PrimitiveType                   primitive_type { PrimitiveType::triangle };

    // A render pass that describes a compatible set of color & depth attachments
    BEE_REFLECT(ignored)
    RenderPassHandle                compatible_render_pass;

    // Index into the render passes subpass array that this pipeline uses (see: RenderPassCreateInfo::subpasses)
    BEE_REFLECT(id = 1, added = 1)
    u32                             subpass_index { 0 };

    // Describes the vertex layouts and input attributes used by vertex buffers
    BEE_REFLECT(id = 2, added = 1)
    VertexDescriptor                vertex_description;

    // Shaders used at the various pipeline stages
    BEE_REFLECT(ignored)
    ShaderHandle                    vertex_stage;

    BEE_REFLECT(ignored)
    ShaderHandle                    fragment_stage;

    // Render state
    BEE_REFLECT(id = 3, added = 1)
    RasterStateDescriptor           raster_state;

    BEE_REFLECT(id = 4, added = 1)
    MultisampleStateDescriptor      multisample_state;

    BEE_REFLECT(id = 5, added = 1)
    DepthStencilStateDescriptor     depth_stencil_state;

    // Blend state
    BEE_REFLECT(id = 6, added = 1)
    u32                             color_blend_state_count { 1 };

    BEE_REFLECT(id = 7, added = 1)
    BlendStateDescriptor            color_blend_states[BEE_GPU_MAX_ATTACHMENTS];

    // Resource binding layout the pipeline is expecting
    BEE_REFLECT(id = 8, added = 1)
    u32                             resource_layout_count { 0 };

    BEE_REFLECT(id = 9, added = 1)
    ResourceLayoutDescriptor        resource_layouts[BEE_GPU_MAX_RESOURCE_LAYOUTS];

    // Ranges of push constants the pipeline can use
    BEE_REFLECT(id = 10, added = 1)
    u32                             push_constant_range_count { 0 };

    BEE_REFLECT(id = 11, added = 1)
    PushConstantRange               push_constant_ranges[gpu_shader_stage_count]; // vulkan etc. can only have one push constant range per shader stage
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


struct SubmitInfo
{
    u32                         command_buffer_count { 0 };
    RawCommandBuffer* const*    command_buffers {nullptr };
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

static constexpr u64 gpu_timeout_max = limits::max<u64>();

enum class CommandBufferState
{
    invalid,
    initial,
    recording,
    executable,
    pending
};


struct GpuCommandBuffer
{
    RawCommandBuffer* instance {nullptr };
    /*
     ***********************************
     *
     * Command functions
     *
     ***********************************
     */
    void (*begin)(RawCommandBuffer* cmd, const CommandBufferUsage usage) {nullptr };

    void (*end)(RawCommandBuffer* cmd) {nullptr };

    void (*reset)(RawCommandBuffer* cmd, const CommandStreamReset hint) {nullptr };

    CommandBufferState (*get_state)(RawCommandBuffer* cmd) {nullptr };

    /*
     ********************
     *
     * Render commands
     *
     ********************
     */
    void (*begin_render_pass)(
        RawCommandBuffer*              cmd,
        const RenderPassHandle&     pass_handle,
        const u32                   attachment_count,
        const TextureViewHandle*    attachments,
        const RenderRect&           render_area,
        const u32                   clear_value_count,
        const ClearValue*           clear_values
    ) { nullptr };

    void (*end_render_pass)(RawCommandBuffer* cmd) {nullptr };

    void (*bind_pipeline_state)(RawCommandBuffer* cmd, const PipelineStateHandle& pipeline_handle) {nullptr };

    void (*bind_vertex_buffer)(RawCommandBuffer* cmd, const BufferHandle& buffer_handle, const u32 binding, const u64 offset) {nullptr };

    void (*bind_vertex_buffers)(
        RawCommandBuffer*      cmd,
        const u32           first_binding,
        const u32           count,
        const BufferHandle* buffers,
        const u64*          offsets
    ) { nullptr };

    void (*bind_index_buffer)(RawCommandBuffer* cmd, const BufferHandle& buffer_handle, const u64 offset, const IndexFormat index_format) {nullptr };

    void (*copy_buffer)(
        RawCommandBuffer*      cmd,
        const BufferHandle& src_handle,
        const i32           src_offset,
        const BufferHandle& dst_handle,
        const i32           dst_offset,
        const i32           size
    ) { nullptr };

    void (*draw)(
        RawCommandBuffer* cmd,
        const u32 vertex_count,
        const u32 instance_count,
        const u32 first_vertex,
        const u32 first_instance
    ) { nullptr };

    void (*draw_indexed)(
        RawCommandBuffer*  cmd,
        const u32       index_count,
        const u32       instance_count,
        const u32       vertex_offset,
        const u32       first_index,
        const u32       first_instance
    ) { nullptr };

    void (*set_viewport)(RawCommandBuffer* cmd, const Viewport& viewport) {nullptr };

    void (*set_scissor)(RawCommandBuffer* cmd, const RenderRect& scissor) {nullptr };

    void (*transition_resources)(RawCommandBuffer* cmd, const u32 count, const GpuTransition* transitions) {nullptr };

    void (*bind_resources)(RawCommandBuffer* cmd, const u32 layout_index, const ResourceBindingHandle& resource_binding) {nullptr };
};


struct GpuBackend
{
    GpuApi (*get_api)() { nullptr };

    const char* (*get_name)() { nullptr };

    bool (*is_initialized)() { nullptr };

    bool (*init)() { nullptr };

    void (*destroy)() { nullptr };

    i32 (*enumerate_physical_devices)(PhysicalDeviceInfo* dst_buffer, const i32 buffer_size) { nullptr };

    DeviceHandle (*create_device)(const DeviceCreateInfo& create_info) { nullptr };

    void (*destroy_device)(const DeviceHandle& device_handle) { nullptr };

    void (*device_wait)(const DeviceHandle& device_handle) { nullptr };

    void (*submissions_wait)(const DeviceHandle& device_handle) { nullptr };

    SwapchainHandle (*create_swapchain)(const DeviceHandle& device_handle, const SwapchainCreateInfo& create_info) { nullptr };

    void (*destroy_swapchain)(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle) { nullptr };

    TextureHandle (*acquire_swapchain_texture)(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle) { nullptr };

    TextureViewHandle (*get_swapchain_texture_view)(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle) { nullptr };

    Extent (*get_swapchain_extent)(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle) { nullptr };

    PixelFormat (*get_swapchain_texture_format)(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle) { nullptr };

    PixelFormat (*get_texture_format)(const DeviceHandle& device_handle, const TextureHandle& handle) { nullptr };

    void (*submit)(const DeviceHandle& device_handle, const SubmitInfo& info) { nullptr };

    void (*present)(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle) { nullptr };

    void (*commit_frame)(const DeviceHandle& device_handle) { nullptr };

    i32 (*get_current_frame)(const DeviceHandle& device_handle) { nullptr };

    /*
     ***********************************
     *
     * Resource functions
     *
     ***********************************
     */
    bool (*allocate_command_buffer)(const DeviceHandle& device_handle, GpuCommandBuffer* cmd, const QueueType queue) {nullptr };

    RenderPassHandle (*create_render_pass)(const DeviceHandle& device_handle, const RenderPassCreateInfo& create_info) { nullptr };

    void (*destroy_render_pass)(const DeviceHandle& device_handle, const RenderPassHandle& handle) { nullptr };

    ShaderHandle (*create_shader)(const DeviceHandle& device_handle, const ShaderCreateInfo& info) { nullptr };

    void (*destroy_shader)(const DeviceHandle& device_handle, const ShaderHandle& shader_handle) { nullptr };

    PipelineStateHandle (*create_pipeline_state)(const DeviceHandle& device_handle, const PipelineStateCreateInfo& create_info) { nullptr };

    void (*destroy_pipeline_state)(const DeviceHandle& device, const PipelineStateHandle& pipeline_handle) { nullptr };

    BufferHandle (*create_buffer)(const DeviceHandle& device, const BufferCreateInfo& create_info) { nullptr };

    void (*destroy_buffer)(const DeviceHandle& device, const BufferHandle& buffer) { nullptr };

    void (*update_buffer)(const DeviceHandle& device_handle, const BufferHandle& buffer_handle, const void* data, const size_t offset, const size_t size) { nullptr };

    TextureHandle (*create_texture)(const DeviceHandle& device_handle, const TextureCreateInfo& create_info) { nullptr };

    void (*destroy_texture)(const DeviceHandle& device_handle, const TextureHandle& texture_handle) { nullptr };

    void (*update_texture)(const DeviceHandle& device_handle, const TextureHandle& texture_handle, const void* data, const Offset& offset, const Extent& extent, const u32 mip_level, const u32 element) { nullptr };

    TextureViewHandle (*create_texture_view)(const DeviceHandle& device_handle, const TextureViewCreateInfo& create_info) { nullptr };

    void (*destroy_texture_view)(const DeviceHandle& device_handle, const TextureViewHandle& texture_view_handle) { nullptr };

    FenceHandle (*create_fence)(const DeviceHandle& device_handle, const FenceState initial_state) { nullptr };

    void (*destroy_fence)(const DeviceHandle& device_handle, const FenceHandle& fence) { nullptr };

    ResourceBindingHandle (*create_resource_binding)(const DeviceHandle& device_handle, const ResourceBindingCreateInfo& create_info) { nullptr };

    void (*destroy_resource_binding)(const DeviceHandle& device_handle, const ResourceBindingHandle& binding_handle) { nullptr };

    SamplerHandle (*create_sampler)(const DeviceHandle& device_handle, const SamplerCreateInfo& info) { nullptr };

    void (*destroy_sampler)(const DeviceHandle& device_handle, const SamplerHandle& sampler_handle) { nullptr };
};

/*
 *******************************************************
 *
 * # Gpu module
 *
 * manages different GPU backends
 *
 *******************************************************
 */
#define BEE_GPU_MODULE_NAME "BEE_GPU_MODULE"

struct GpuModule
{
    void (*register_backend)(GpuBackend* backend) { nullptr };

    void (*unregister_backend)(const GpuBackend* backend) { nullptr };

    i32 (*enumerate_available_backends)(GpuBackend** dst) { nullptr };

    GpuBackend* (*get_default_backend)(const GpuApi api) { nullptr };

    GpuBackend* (*get_backend)(const char* name) { nullptr };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.Gpu/Gpu.generated.inl"
#endif // BEE_ENABLE_REFLECTION