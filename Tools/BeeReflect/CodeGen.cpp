/*
 *  CodeGen.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "CodeGen.hpp"

#include "Bee/Core/IO.hpp"

#include <time.h>

namespace bee {


const char* reflection_flag_to_string(const Qualifier qualifier)
{
#define QUALIFIER(x) case Qualifier::x: return "Qualifier::" #x

    switch (qualifier)
    {
        QUALIFIER(cv_const);
        QUALIFIER(cv_volatile);
        QUALIFIER(lvalue_ref);
        QUALIFIER(rvalue_ref);
        QUALIFIER(pointer);
        default: break;
    }

    return "Qualifier::none";
#undef QUALIFIER
}

const char* reflection_flag_to_string(const StorageClass storage_class)
{
#define STORAGE_CLASS(x) case StorageClass::x: return "StorageClass::" #x

    switch (storage_class)
    {
        STORAGE_CLASS(auto_storage);
        STORAGE_CLASS(register_storage);
        STORAGE_CLASS(static_storage);
        STORAGE_CLASS(extern_storage);
        STORAGE_CLASS(thread_local_storage);
        STORAGE_CLASS(mutable_storage);
        default: break;
    }

    return "StorageClass::none";
#undef STORAGE_CLASS
}

const char* reflection_type_kind_to_string(const TypeKind type_kind)
{
#define TYPE_KIND(x) case TypeKind::x: return "TypeKind::" #x

    switch (type_kind)
    {
        TYPE_KIND(class_decl);
        TYPE_KIND(struct_decl);
        TYPE_KIND(enum_decl);
        TYPE_KIND(union_decl);
        TYPE_KIND(template_decl);
        TYPE_KIND(field);
        TYPE_KIND(function);
        TYPE_KIND(fundamental);
        default: break;
    }

    return "TypeKind::unknown";
#undef TYPE_KIND
}


void pretty_print_types(const Span<const Type* const>& types, io::StringStream* stream)
{
    for (const Type* type : types)
    {
        stream->write_fmt(
            "== %s (0x%08x) ==\n- size: %zu\n- alignment: %zu\n- kind: %s\n",
            type->name,
            type->hash,
            type->size,
            type->alignment,
            reflection_type_kind_to_string(type->kind)
        );

        if (type->kind == TypeKind::class_decl || type->kind == TypeKind::struct_decl || type->kind == TypeKind::union_decl)
        {
            auto as_class = reinterpret_cast<const RecordType*>(type);

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

                if (function->is_constexpr)
                {
                    stream->write("constexpr ");
                }

                stream->write_fmt("%s %s(", function->return_value.type->name, function->name);

                for (const auto param : enumerate(function->parameters))
                {
                    if (param.value.qualifier != Qualifier::none)
                    {
                        stream->write_fmt("%s ", reflection_dump_flags(param.value.qualifier));
                    }
                    stream->write_fmt("%s %s", param.value.type->name, param.value.name);
                    if (param.index < function->parameters.size() - 1)
                    {
                        stream->write(", ");
                    }
                }

                stream->write(") ");
                stream->write_fmt("[storage_class: %s]\n", reflection_dump_flags(function->storage_class));
            }
        }

        stream->write("\n");
    }
}


String get_name_as_ident(const Type* type, Allocator* allocator = system_allocator())
{
    String name_as_ident(type->name, allocator);
    str::replace(&name_as_ident, "::", "_");
    str::replace(&name_as_ident, " ", "_");
    str::replace(&name_as_ident, "<", "_");
    str::replace(&name_as_ident, ">", "_");
    return name_as_ident;
}


void codegen_record(const RecordType* type, CodeGenerator* codegen)
{
    const auto name_as_ident = get_name_as_ident(type, temp_allocator());

    codegen->write("template <> BEE_EXPORT_SYMBOL const Type* get_type<%s>()", type->name);
    codegen->scope([&]()
    {
        if (!type->fields.empty())
        {
            codegen->write("static Field %s__fields[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const Field& field : type->fields)
                {
                    codegen->write("Field");
                    codegen->scope([&]()
                    {
                        codegen->write(
                            "%zu, %s, %s, %s, get_type<%s>() ",
                            field.offset,
                            reflection_flag_to_string(field.qualifier),
                            reflection_flag_to_string(field.storage_class),
                            field.name,
                            field.type->name
                        );
                    }, ",\n");
                }
            }, ";");
            codegen->write_line("// %s__fields[]\n", name_as_ident.c_str());
        }

        if (!type->functions.empty())
        {
            codegen->write("static FunctionType* %s__functions[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const FunctionType* function : type->functions)
                {
                    const auto function_type_as_ident = get_name_as_ident(function, temp_allocator());
                    codegen->write("get_type<%s>(), ", function_type_as_ident.c_str());
                }
            }, ";");
            codegen->write_line("// %s__functions[]\n", name_as_ident.c_str());
        }

        codegen->write("static constexpr RecordType instance");
        codegen->scope([&]()
        {
            codegen->write(
                "%u, %zu, %zu, %s, %s, ",
                type->hash,
                type->size,
                type->alignment,
                reflection_type_kind_to_string(type->kind),
                type->name
            );

            if (!type->fields.empty())
            {
                codegen->append_line("%s__fields, ", name_as_ident.c_str());
            }

            if (!type->functions.empty())
            {
                codegen->append_line("%s__functions, ", name_as_ident.c_str());
            }
        }, ";\n\n");
        codegen->write_line("return &instance;");
    });
    codegen->write_line("// get_type<%s>()\n", type->name);
}


void reflection_codegen(const Path& source_location, const Span<const Type*>& types, io::StringStream* stream)
{
    CodeGenerator codegen(stream, 4);

    char time_string[256];
    const time_t timepoint = ::time(nullptr);
    const auto timeinfo = localtime(&timepoint);
    ::strftime(time_string, static_array_length(time_string), BEE_TIMESTAMP_FMT, timeinfo);

    codegen.write_line(R"(/*
 *  This file was generated by the bee-reflect tool. DO NOT EDIT DIRECTLY.
 *
 *  Generated on: %s
 *  Source: %s
 */
)", time_string, source_location.c_str());

    codegen.newline();
    codegen.write_line("#include <Bee/Core/ReflectionV2.hpp>");
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
                    codegen_record(reinterpret_cast<const RecordType*>(type), &codegen);
                }
                case TypeKind::enum_decl:
                    break;
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