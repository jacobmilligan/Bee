/*
 *  AssetMeta.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/GUID.hpp"


namespace bee {


struct AssetCompileSettings;

struct AssetMeta
{
    static constexpr const char* settings_member_name = "compile_settings";

    GUID    guid;
    Type    type;
    char    name[256];

    AssetMeta()
    {
        name[0] = '\0';
    }

    AssetMeta(const GUID& new_guid, const Type& new_type, const char* new_name)
        : guid(new_guid),
          type(new_type)
    {
        str::copy(name, static_array_length(name), new_name);
    }

    inline bool is_valid() const
    {
        return type.is_valid();
    }
};


#define BEE_SERIALIZE_ASSET_SETTINGS(version, type) BEE_DEFINE_SERIALIZE_TYPE(version, type, bee::AssetMeta::settings_member_name)


void asset_meta_serialize(const SerializerMode& mode, const Path& location, AssetMeta* meta, AssetCompileSettings* settings, Allocator* allocator = system_allocator());

} // namespace bee