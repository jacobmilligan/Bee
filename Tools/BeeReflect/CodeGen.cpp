/*
 *  CodeGen.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "CodeGen.hpp"

#include "Bee/Core/IO.hpp"


namespace bee {


String get_name_as_ident(const Type* type, Allocator* allocator = system_allocator())
{
    String name_as_ident(type->name, allocator);
    str::replace(&name_as_ident, "::", "_");
    str::replace(&name_as_ident, " ", "_");
    str::replace(&name_as_ident, "<", "_");
    str::replace(&name_as_ident, ">", "_");
    return name_as_ident;
}


void codegen_type(const Type* type, CodeGenerator* codegen)
{
    codegen->write(
        "%u, %zu, %zu, %s, \"%s\", ",
        type->hash,
        type->size,
        type->alignment,
        reflection_type_kind_to_string(type->kind),
        type->name
    );
}


void codegen_field(const Field& field, CodeGenerator* codegen)
{
    codegen->write("Field");
    codegen->scope([&]()
    {
        codegen->write(
            "%zu, %s, %s, \"%s\", get_type<%s>() ",
            field.offset,
            reflection_flag_to_string(field.qualifier),
            reflection_flag_to_string(field.storage_class),
            field.name,
            field.type->name
        );
    });
}


void codegen_function(const FunctionType* type, CodeGenerator* codegen)
{
    const auto function_name_as_ident = get_name_as_ident(type, temp_allocator());

    if (!type->parameters.empty())
    {
        codegen->write("static Field %s__parameters[] =", function_name_as_ident.c_str());
        codegen->scope([&]()
        {
            for (const Field& field : type->parameters)
            {
                codegen_field(field, codegen);
                codegen->append_line(",\n");
            }
        }, ";");
        codegen->newline();
        codegen->newline();
    }

    codegen->write("static FunctionType %s", function_name_as_ident.c_str());
    codegen->scope([&]()
    {
        codegen_type(type, codegen);

        codegen->append_line("%s, %s,", reflection_flag_to_string(type->storage_class), type->is_constexpr ? "true" : "false");
        codegen->newline();

        codegen_field(type->return_value, codegen);

        if (!type->parameters.empty())
        {
            codegen->append_line(", Span<Field>(%s__parameters)", function_name_as_ident.c_str());
        }
        else
        {
            codegen->append_line(", {}");
        }
    }, ";");
}

void codegen_record(const RecordType* type, CodeGenerator* codegen)
{
    const auto name_as_ident = get_name_as_ident(type, temp_allocator());

    if (!type->fields.empty())
    {
        codegen->write("static Field %s__fields[] =", name_as_ident.c_str());
        codegen->scope([&]()
        {
            for (const Field& field : type->fields)
            {
                codegen_field(field, codegen);
                codegen->append_line(",\n");
            }
        }, ";");
        codegen->write_line("// %s__fields[]\n", name_as_ident.c_str());
    }

    if (!type->functions.empty())
    {
        for (const FunctionType& function : type->functions)
        {
            codegen_function(&function, codegen);
            codegen->newline();
        }

        codegen->newline();
        codegen->write("static FunctionType %s__functions[] =", name_as_ident.c_str());
        codegen->scope([&]()
        {
            for (const FunctionType& function : type->functions)
            {
                const auto function_name_as_ident = get_name_as_ident(&function, temp_allocator());
                codegen->write_line("%s,", function_name_as_ident.c_str());
            }
        }, ";");
        codegen->write_line("// %s__functions[]\n", name_as_ident.c_str());
    }

    codegen->write("static RecordType instance");
    codegen->scope([&]()
    {
        codegen_type(type, codegen);

        if (!type->fields.empty())
        {
            codegen->append_line("Span<Field>(%s__fields)", name_as_ident.c_str());
        }
        else
        {
            codegen->append_line("{}");
        }

        codegen->append_line(", ");

        if (!type->functions.empty())
        {
            codegen->append_line("Span<FunctionType>(%s__functions)", name_as_ident.c_str());
        }
        else
        {
            codegen->append_line("{}");
        }
    }, ";\n\n");
    codegen->write_line("return &instance;");
}


void reflection_codegen(const Path& source_location, const Span<const Type*>& types, io::StringStream* stream)
{
    CodeGenerator codegen(stream, 4);

    codegen.write_header_comment(source_location);
    codegen.newline();
    codegen.write_line("#include \"%" BEE_PRIsv "\"", BEE_FMT_SV(source_location.filename()));
    codegen.newline();

    codegen.write("namespace bee ");
    codegen.scope([&]() {
        for (const Type* type : types)
        {
            switch (type->kind)
            {
                case TypeKind::class_decl:
                case TypeKind::struct_decl:
                case TypeKind::union_decl:
                {
                    codegen.write("template <> BEE_EXPORT_SYMBOL const Type* get_type<%s>()", type->name);
                    codegen.scope([&]()
                    {
                        codegen_record(reinterpret_cast<const RecordType*>(type), &codegen);
                    });
                    codegen.write_line("// get_type<%s>()\n", type->name);

                    break;
                }
                case TypeKind::enum_decl:
                {
//                    codegen_enum(type, &codegen);
                    break;
                }
                case TypeKind::template_decl:
                    break;
                case TypeKind::field:
                    break;
                case TypeKind::function:
                    break;
                case TypeKind::fundamental:
                    break;
                default: break;
            }
        }
    }, " // namespace bee\n");
    codegen.newline();
}


} // namespace bee