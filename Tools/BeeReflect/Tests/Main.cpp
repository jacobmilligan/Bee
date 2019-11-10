/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "BasicStruct.hpp"

#include <Bee/Core/IO.hpp>


void pretty_print_type(const bee::Type* type, bee::io::StringStream* stream)
{
    stream->write_fmt(
        "== %s (0x%08x) ==\n- size: %zu\n- alignment: %zu\n- kind: %s\n",
        type->name,
        type->hash,
        type->size,
        type->alignment,
        reflection_type_kind_to_string(type->kind)
    );

    if (type->kind == bee::TypeKind::class_decl || type->kind == bee::TypeKind::struct_decl || type->kind == bee::TypeKind::union_decl)
    {
        auto as_class = reinterpret_cast<const bee::RecordType*>(type);

        stream->write("- fields:\n");

        for (auto& field : as_class->fields)
        {
            stream->write_fmt("  * %s", field.name);

            if (field.type != nullptr)
            {
                stream->write_fmt(" [%s]", field.type->name);
            }

            stream->write_fmt(":\n    - qualifier: %s\n    - storage_class: %s\n    - offset: %zu\n",
                reflection_dump_flags(field.qualifier),
                reflection_dump_flags(field.storage_class),
                field.offset
            );

            stream->write("    - attributes: ");
            for (const bee::Attribute& attr : field.attributes)
            {
                stream->write_fmt("%s = ", attr.name);
                switch (attr.kind)
                {
                    case bee::AttributeKind::boolean:
                        stream->write_fmt("%s", attr.value.boolean ? "true" : "false");
                        break;
                    case bee::AttributeKind::integer:
                        stream->write_fmt("%d", attr.value.integer);
                        break;
                    case bee::AttributeKind::floating_point:
                        stream->write_fmt("%f", attr.value.floating_point);
                        break;
                    case bee::AttributeKind::string:
                        stream->write_fmt("%s", attr.value.string);
                        break;
                    default:
                        break;
                }
                stream->write(", ");
            }
            stream->write("\n");

        }

        stream->write("- functions:\n");

        for (auto& function : as_class->functions)
        {
            stream->write("  * ");

            if (function.is_constexpr)
            {
                stream->write("constexpr ");
            }

            stream->write_fmt("%s %s(", function.return_value.type->name, function.name);

            for (const auto param : enumerate(function.parameters))
            {
                if (param.value.qualifier != bee::Qualifier::none)
                {
                    stream->write_fmt("%s ", reflection_dump_flags(param.value.qualifier));
                }
                stream->write_fmt("%s %s", param.value.type->name, param.value.name);
                if (param.index < function.parameters.size() - 1)
                {
                    stream->write(", ");
                }
            }

            stream->write(") ");
            stream->write_fmt("[storage_class: %s]\n", reflection_dump_flags(function.storage_class));
        }

        stream->write(" - attributes: ");
        for (const bee::Attribute& attr : as_class->attributes)
        {
            stream->write_fmt("%s = ", attr.name);
            switch (attr.kind)
            {
                case bee::AttributeKind::boolean:
                    stream->write_fmt("%s", attr.value.boolean ? "true" : "false");
                    break;
                case bee::AttributeKind::integer:
                    stream->write_fmt("%d", attr.value.integer);
                    break;
                case bee::AttributeKind::floating_point:
                    stream->write_fmt("%f", attr.value.floating_point);
                    break;
                case bee::AttributeKind::string:
                    stream->write_fmt("%s", attr.value.string);
                    break;
                default:
                    break;
            }
            stream->write(", ");
        }
        stream->write("\n");
    }

    stream->write("\n");
}


int main(int argc, char** argv)
{
    bee::reflection_init();
    
    const auto type = bee::get_type(bee::get_type<bee::test_reflection::MyClass>()->hash);
    bee::String str;
    bee::io::StringStream stream(&str);
    pretty_print_type(type, &stream);
    bee::log_info("%s", str.c_str());
}