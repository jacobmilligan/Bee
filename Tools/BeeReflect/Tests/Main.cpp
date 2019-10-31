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
    }

    stream->write("\n");
}


int main(int argc, char** argv)
{
    const auto type = bee::get_type<bee::test_reflection::MyClass>();
    bee::String str;
    bee::io::StringStream stream(&str);
    pretty_print_type(type, &stream);
    bee::log_info("%s", str.c_str());
}