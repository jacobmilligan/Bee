/*
 *  AssetMeta.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#define BEE_RAPIDJSON_RAPIDJSON_H
#define BEE_RAPIDJSON_DOCUMENT_H
#define BEE_RAPIDJSON_ERROR_H
#include "Bee/AssetPipeline/AssetMeta.hpp"
#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/Core/Filesystem.hpp"

namespace bee {


void asset_meta_serialize(const SerializerMode& mode, const Path& location, AssetMeta* meta, AssetCompileSettings* settings, Allocator* allocator)
{
    static constexpr const char* type_member_name = "bee::type";

    if (mode == SerializerMode::reading)
    {
        auto src = fs::read(location, allocator);
        JSONReader reader(&src, allocator);
        reader.reset(SerializerMode::reading);
        reader.begin();
        serialize_type(&reader, &meta->guid, "guid");
        serialize_type(&reader, &meta->type, "type");
        serialize_type(&reader, &meta->name, "name");
        reader.end();

        auto& doc = reader.document();
        auto settings_member = doc.FindMember(AssetMeta::settings_member_name);

        if (BEE_FAIL_F(settings_member != doc.MemberEnd() && settings_member->value.IsObject(), "AssetMeta was corrupt or missing a `settings` JSON member"))
        {
            return;
        }

        auto type_member = settings_member->value.FindMember(type_member_name);
        if (BEE_FAIL_F(type_member != doc.MemberEnd() && type_member->value.IsString(), "AssetMeta.compile_settings is missing a `%s` JSON member", type_member_name))
        {
            return;
        }

        rapidjson::Document settings_doc(&doc.GetAllocator());
        settings_doc.SetObject();
        rapidjson::Value keyval(type_member->value.GetString(), doc.GetAllocator());
        settings_doc.RemoveMember(keyval.GetString());
        settings_doc.AddMember(keyval, settings_member->value, doc.GetAllocator());

        rapidjson::StringBuffer string_buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> pretty_writer(string_buffer);
        settings_doc.Accept(pretty_writer);

        settings->json = String(string_buffer.GetString(), allocator);
    }
    else
    {
        JSONWriter writer(allocator);
        writer.reset(SerializerMode::writing);
        writer.begin();
        serialize_type(&writer, &meta->guid, "guid");
        serialize_type(&writer, &meta->type, "type");
        serialize_type(&writer, &meta->name, "name");

        rapidjson::Document doc;

        if (settings->is_valid())
        {
            doc.Parse(settings->json.c_str());
        }

        auto type_member = doc.MemberBegin()->value.FindMember(type_member_name);
        const auto has_type_member = type_member != doc.MemberEnd();
        if (BEE_FAIL_F(type_member != doc.MemberEnd(), "Invalid settings JSON - object member `%s` is missing", type_member_name))
        {
            return;
        }

        if (BEE_FAIL_F(type_member->value.IsNull() || type_member->value.IsString(), "Invalid settings JSON: `%s` has an invalid type", type_member_name))
        {
            return;
        }

        rapidjson::StringBuffer string_buffer;
        auto& pretty_writer = writer.pretty_writer();
        pretty_writer.Key(AssetMeta::settings_member_name);

        if (has_type_member && type_member->value != doc.MemberBegin()->name)
        {
            type_member->value.SetString(doc.MemberBegin()->name.GetString(), doc.GetAllocator());
        }

        // Append a bee::type object member if it's missing and hasn't been added earlier
        if (!has_type_member)
        {
            rapidjson::Value key_val(type_member_name, doc.GetAllocator());
            doc.MemberBegin()->value.AddMember(key_val, doc.MemberBegin()->name, doc.GetAllocator());
        }

        doc.MemberBegin()->value.Accept(pretty_writer);

        writer.end();

        fs::write(location, writer.c_str());
    }
}


} // namespace bee