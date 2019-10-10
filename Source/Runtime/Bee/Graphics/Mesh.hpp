/*
 *  Mesh.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Math/Math.hpp"

namespace bee {


#define BEE_MESH_ATTRIBUTES                                         \
    BEE_DEFINE_MESH_ATTRIBUTE("POSITION",   position,       float4) \
    BEE_DEFINE_MESH_ATTRIBUTE("COLOR",      color,          float4) \
    BEE_DEFINE_MESH_ATTRIBUTE("NORMAL",     normal,         float4) \
    BEE_DEFINE_MESH_ATTRIBUTE("TANGENT",    tangent,        float4) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD0",  texcoord0,      float2) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD1",  texcoord1,      float2) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD2",  texcoord2,      float2) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD3",  texcoord3,      float2) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD4",  texcoord4,      float2) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD5",  texcoord5,      float2) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD6",  texcoord6,      float2) \
    BEE_DEFINE_MESH_ATTRIBUTE("TEXCOORD7",  texcoord7,      float2)


enum class MeshAttribute : i32
{
#define BEE_DEFINE_MESH_ATTRIBUTE(semantic_string, enum_type, stored_type) enum_type,
    BEE_MESH_ATTRIBUTES
    count,
    unknown
#undef BEE_DEFINE_MESH_ATTRIBUTE
};

/**
 * An enumeration of all supported mesh attributes as flags. These are declared in the order in which their shader
 * vertex input locations are mapped to.
 */
BEE_FLAGS(MeshAttributeFlags, u32)
{
    none = 0u,

// Declare the valid mesh location types
#define BEE_DEFINE_MESH_ATTRIBUTE(semantic_string, enum_type, stored_type) enum_type = 1u << static_cast<u32>(MeshAttribute::enum_type),
    BEE_MESH_ATTRIBUTES
#undef BEE_DEFINE_MESH_ATTRIBUTE

// Declare MeshAttribute::all as all the previous flags OR'd together
#define BEE_DEFINE_MESH_ATTRIBUTE(semantic_string, enum_type, stored_type) enum_type |
    all = BEE_MESH_ATTRIBUTES 0u,
#undef BEE_DEFINE_MESH_ATTRIBUTE

    unknown
};


/**
 * Converts a HLSL semantic string to a MeshAttribute enum value - if the semantic string is not defined by
 * BEE_MESH_ATTRIBUTES, this function will return MeshAttribute::unknown
 */
inline MeshAttribute semantic_to_mesh_attribute(const char* semantic)
{
#define BEE_DEFINE_MESH_ATTRIBUTE(semantic_string, enum_type, stored_type)        \
    if (str::compare(semantic, semantic_string) == 0)                             \
    {                                                                             \
        return MeshAttribute::enum_type;                                          \
    }

    BEE_MESH_ATTRIBUTES

#undef BEE_DEFINE_MESH_ATTRIBUTE

    return MeshAttribute::unknown;
}

/**
 * Converts a MeshAttribute flag to it's associated HLSL semantic string. If the attributes value is `unknown` or it's
 * a set of multiple flags then this function will return "UNKNOWN".
 */
inline const char* mesh_attribute_to_semantic(const MeshAttribute attribute)
{
#define BEE_DEFINE_MESH_ATTRIBUTE(semantic_string, enum_type, stored_type)  \
    case MeshAttribute::enum_type: return semantic_string;

    switch (attribute)
    {
        BEE_MESH_ATTRIBUTES
        default: break;
    }

#undef BEE_DEFINE_MESH_ATTRIBUTE

    return "UNKNOWN";
}


inline MeshAttribute mesh_attribute_position_cast(const MeshAttributeFlags flag)
{
    return static_cast<MeshAttribute>(math::log2i(static_cast<u32>(flag)));
}

inline MeshAttributeFlags mesh_attribute_flag_cast(const MeshAttribute attribute)
{
    return static_cast<MeshAttributeFlags>(1u << static_cast<u32>(attribute));
}


} // namespace bee