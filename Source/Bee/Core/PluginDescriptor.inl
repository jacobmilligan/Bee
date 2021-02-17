/*
 *  Plugin.inl
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"


namespace bee {


struct BEE_REFLECT(serializable, version = 1) PluginDependencyDescriptor
{
    String          name;
    PluginVersion   version;
};

struct BEE_REFLECT(serializable, version = 1) PluginDescriptor
{
    String                                      name;
    PluginVersion                               version;
    DynamicArray<PluginDependencyDescriptor>    dependencies;

    BEE_REFLECT(nonserialized)
    Path                                        path;
};

BEE_SERIALIZE_TYPE(SerializationBuilder* builder, PluginVersion* version)
{
    static thread_local StaticString<16> format_buffer;

    if (builder->format() == SerializerFormat::binary)
    {
        serialize_type(builder->serializer(), builder->params());
        return;
    }

    format_buffer.clear();

    if (builder->mode() == SerializerMode::writing)
    {
        str::format_buffer(&format_buffer, "%d.%d.%d", version->major, version->minor, version->patch);
        int size = format_buffer.size();
        builder->container(SerializedContainerKind::text, &size);
        builder->text(format_buffer.data(), size, format_buffer.capacity());
    }
    else
    {
        int size = 0;
        builder->container(SerializedContainerKind::text, &size);
        format_buffer.resize(size);
        builder->text(format_buffer.data(), size, format_buffer.capacity());

        StringView parts[3];
        const int count = str::split(format_buffer.view(), parts, 3, ".");
        if (count <= 0 || count > 3)
        {
            log_error("Invalid plugin version format: %s", format_buffer.c_str());
            return;
        }

        if (count > 0)
        {
            BEE_CHECK(str::to_i32(parts[0], &version->major));
        }
        if (count > 1)
        {
            BEE_CHECK(str::to_i32(parts[1], &version->minor));
        }
        if (count > 2)
        {
            BEE_CHECK(str::to_i32(parts[2], &version->patch));
        }
    }
}


} // namespace bee