/*
 *  CodeGen.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "CodeGen.hpp"
#include "Storage.hpp"

#include "Bee/Core/IO.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Reflection.hpp"

#include <map>
#include <set>
#include <inttypes.h>


namespace bee {
namespace reflect {


BEE_FLAGS(CodegenTypeOptions, u32)
{
    none                    = 0u,
    use_explicit_kind_flags = 1u << 0u // uses the types explicit kind flags rather than using the types `static_kind`
};

CodeGenerator::CodeGenerator(const CodegenMode mode, io::StringStream *stream, const i32 indent_size)
    : mode_(mode),
      stream_(stream),
      indent_size_(indent_size)
{}

void CodeGenerator::reset(io::StringStream *new_stream)
{
    stream_ = new_stream;
    indent_ = 0;
}

i32 CodeGenerator::set_indent(const i32 indent)
{
    const auto old_indent = indent_;
    indent_ = indent;
    return old_indent;
}

void CodeGenerator::indent()
{
    for (int i = 0; i < indent_; ++i)
    {
        stream_->write(' ');
    }
}

void CodeGenerator::newline()
{
    stream_->write("\n");
}

bool CodeGenerator::should_generate(const TypeInfo& type)
{
    if (mode_ == CodegenMode::cpp)
    {
        return !type.is(TypeKind::template_decl);
    }

    if (mode_ == CodegenMode::templates_only)
    {
        return type.is(TypeKind::template_decl);
    }

    return true;
}

void CodeGenerator::write_header_comment(const char *source_location)
{
    char time_string[256];
    const time_t timepoint = ::time(nullptr);
    auto* const timeinfo = localtime(&timepoint);
    ::strftime(time_string, static_array_length(time_string), BEE_TIMESTAMP_FMT, timeinfo);

    write_line(R"(/*
 *  This file was generated by the bee-reflect tool. DO NOT EDIT DIRECTLY.
 *
 *  Generated on: %s
 *  Source: %s
 */
)", time_string, source_location);
}

void CodeGenerator::write_header_comment(const Path &source_location)
{
    write_header_comment(source_location.c_str());
}

void CodeGenerator::write_type_signature(const TypeInfo& type)
{
    // Array types shouldn't be exported
    const auto* export_symbol = type.is(TypeKind::array) ? "" : "BEE_EXPORT_SYMBOL";

    switch (mode_)
    {
        case CodegenMode::cpp:
        {
            if (type.is(TypeKind::function))
            {
                write("template <> %s Type get_type<BEE_NONMEMBER(%s)>(const TypeTag<BEE_NONMEMBER(%s)>& tag)", export_symbol, type.name, type.name);
            }
            else
            {
                write("template <> %s Type get_type<%s>(const TypeTag<%s>& tag)", export_symbol, type.name, type.name);
            }
            break;
        }
        case CodegenMode::templates_only:
        {
            if (type.is(TypeKind::template_decl))
            {
                write("%s inline Type get_type(const TypeTag<%s>& tag)", export_symbol, type.name);
            }
            break;
        }
        case CodegenMode::inl:
        {
            if (type.is(TypeKind::function))
            {
                write("template <> %s inline Type get_type<BEE_NONMEMBER(%s)>(const TypeTag<BEE_NONMEMBER(%s)>& tag)", export_symbol, type.name, type.name);
            }
            else
            {
                write("template <> %s inline Type get_type<%s>(const TypeTag<%s>& tag)", export_symbol, type.name, type.name);
            }
        }
    }
}

void CodeGenerator::append_line(const char *format, ...)
{
    va_list args = nullptr;
    va_start(args, format);

    stream_->write_v(format, args);

    va_end(args);
}

void CodeGenerator::write(const char *format, ...)
{
    va_list args = nullptr;
    va_start(args, format);

    indent();
    stream_->write_v(format, args);

    va_end(args);
}

void CodeGenerator::write_line(const char *format, ...)
{
    va_list args = nullptr;
    va_start(args, format);

    indent();
    stream_->write_v(format, args);
    stream_->write("\n");

    va_end(args);
}


String get_name_as_ident(const TypeInfo& type, Allocator* allocator = system_allocator())
{
    String name_as_ident(type.name, allocator);
    str::replace(&name_as_ident, "::", "_");
    str::replace(&name_as_ident, " ", "_");
    str::replace(&name_as_ident, "<", "_");
    str::replace(&name_as_ident, ">", "_");
    str::replace(&name_as_ident, "[", "_");
    str::replace(&name_as_ident, "]", "_");
    str::replace(&name_as_ident, ",", "_");
    return name_as_ident;
}


String get_target_name_as_ident(const StringView& target_name, Allocator* allocator = system_allocator())
{
    String name_as_ident(target_name, allocator);
    str::replace(&name_as_ident, "::", "__");
    str::replace(&name_as_ident, " ", "__");
    str::replace(&name_as_ident, "<", "__");
    str::replace(&name_as_ident, ">", "__");
    str::replace(&name_as_ident, "-", "__");
    str::replace(&name_as_ident, ".", "__");
    str::replace(&name_as_ident, "[", "__");
    str::replace(&name_as_ident, "]", "__");
    return name_as_ident;
}

void get_kind_as_macro(String* dst, const TypeKind kind)
{
    dst->clear();

#define TYPE_KIND(x) case TypeKind::x: if (!dst->empty()) { dst->append('_'); } dst->append(#x); break;

    for_each_flag(kind, [&](const TypeKind k)
    {
        switch (k)
        {
            TYPE_KIND(unknown);
            TYPE_KIND(class_decl);
            TYPE_KIND(struct_decl);
            TYPE_KIND(enum_decl);
            TYPE_KIND(union_decl);
            TYPE_KIND(template_decl);
            TYPE_KIND(field);
            TYPE_KIND(function);
            TYPE_KIND(method);
            TYPE_KIND(fundamental);
            TYPE_KIND(array);
            default:
            {
                BEE_UNREACHABLE("Missing TypeKind string representation: %u", static_cast<u32>(k));
            }
        }
    });
#undef TYPE_KIND

    str::uppercase_ascii(dst);
}

/*
 * Avoids issues where types may be included multiple times i.e. with array types (an int[4] defined
 * in two places is effectively the same type) - this is really an issue only with inline get_types and arrays
 */
void CodeGenerator::type_guard_begin(const TypeInfo* type)
{
    const auto ident = get_name_as_ident(*type, temp_allocator());
    String type_kind(temp_allocator());
    get_kind_as_macro(&type_kind, type->kind);

    auto old_indent = set_indent(0);
    {
        write_line("#ifndef BEE_%s_TYPE__%s", type_kind.c_str(), ident.c_str());
        write_line("#define BEE_%s_TYPE__%s", type_kind.c_str(), ident.c_str());
        newline();
    }
    set_indent(old_indent);
}

void CodeGenerator::type_guard_end(const TypeInfo* type)
{
    const auto ident = get_name_as_ident(*type, temp_allocator());
    String type_kind(temp_allocator());
    get_kind_as_macro(&type_kind, type->kind);

    auto old_indent = set_indent(0);
    {
        write_line("#endif // BEE_%s_TYPE__%s", type_kind.c_str(), ident.c_str());
        newline();
    }
    set_indent(old_indent);
}

void codegen_template_parameters(const Span<const TemplateParameter>& parameters, CodeGenerator* codegen)
{
    if (parameters.empty())
    {
        return;
    }

    codegen->write("static TemplateParameter template_parameters[] =");
    codegen->scope([&]()
    {
        for (const TemplateParameter& param : parameters)
        {
            codegen->write_line(
                R"(TemplateParameter { %u, "%s", "%s" },)",
                param.hash,
                param.name,
                param.type_name
            );
        }
    }, ";");
    codegen->newline();
    codegen->newline();
}

void codegen_create_instance(const TypeInfo& type, CodeGenerator* codegen)
{
    codegen->write("static auto create_instance_function = [](bee::Allocator* allocator)");
    codegen->scope([&]()
    {
        if (type.is(TypeKind::function) || type.is(TypeKind::array))
        {
            codegen->write("return bee::make_type_instance<void>(allocator);");
        }
        else
        {
            codegen->write("return bee::make_type_instance<%s>(allocator);", type.name);
        }
    }, ";");
    codegen->newline();
    codegen->newline();
}

void codegen_type(const CodegenTypeOptions options, const TypeInfo& type, CodeGenerator* codegen)
{
    const auto* const size_align_type = type.is(TypeKind::function) ? "void*" : type.name;

    codegen->write(
        "%u, sizeof(%s), alignof(%s), ",
        type.hash,
        size_align_type,
        size_align_type
    );

    if ((options & CodegenTypeOptions::use_explicit_kind_flags) != CodegenTypeOptions::none)
    {
        codegen->append_line("%s, ", reflection_dump_flags(type.kind));
    }

    codegen->append_line(
        "\"%s\", %d, %s, create_instance_function, ",
        type.name,
        type.serialized_version,
        reflection_dump_flags(type.serialization_flags)
    );

    if (type.is(TypeKind::template_decl))
    {
        codegen->append_line("Span<TemplateParameter>(template_parameters), ");
    }
}


void codegen_attribute(const Attribute& attr, CodeGenerator* codegen)
{
    codegen->write("Attribute");
    codegen->scope([&]()
    {
        codegen->write(
            "%s, %u, \"%s\", Attribute::Value(",
            reflection_attribute_kind_to_string(attr.kind),
            attr.hash,
            attr.name
        );

        switch (attr.kind)
        {
            case AttributeKind::boolean:
            {
                codegen->append_line("%s", attr.value.boolean ? "true" : "false");
                break;
            }
            case AttributeKind::integer:
            {
                codegen->append_line("%d", attr.value.integer);
                break;
            }
            case AttributeKind::floating_point:
            {
                codegen->append_line("%ff", attr.value.floating_point);
                break;
            }
            case AttributeKind::string:
            {
                codegen->append_line("\"%s\"", attr.value.string);
                break;
            }
            case AttributeKind::type:
            {
                codegen->append_line("get_type<%s>()", attr.value.string); // type names are stored in strings by the ASTMatcher
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Invalid attribute kind: AttributeKind::invalid");
            }
        }

        codegen->append_line(")");
    });
}

bool has_serializer_function(const FieldStorage& storage)
{
    const auto& field = storage.field;
    const auto uses_builder = (field.type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none;
    const auto is_templated_and_serialized = !storage.template_arguments.empty() && field.type->serialized_version > 0;
    return uses_builder || is_templated_and_serialized;
}

// TODO(Jacob): add specialized type to array
void codegen_serializer_function(CodeGenerator* codegen, const char* field_name, const char* specialized_type_name)
{
    // [](SerializationBuilder* builder, void* data) { serialize_type(builder, static_cast<bee::GUID*>(data)); };
    codegen->write("static auto %s__serializer_function = [](SerializationBuilder* builder, void* data) { serialize_type(builder, static_cast<%s*>(data)); };", field_name, specialized_type_name);
    codegen->newline();
    codegen->newline();
}

void codegen_field_extra_info(const FieldStorage& storage, CodeGenerator* codegen)
{
    const auto& field = storage.field;
    if (field.type->is(TypeKind::template_decl))
    {
        codegen->write("static Type %s__template_args[] =", field.name);
        codegen->scope([&]()
        {
            codegen->indent();
            for (const auto& template_arg : storage.template_arguments)
            {
                codegen->append_line("get_type<%s>(), ", template_arg->name);
            }
        }, ";");

        codegen->newline();
        codegen->newline();
    }


    if (has_serializer_function(storage))
    {
        codegen_serializer_function(codegen, storage.field.name, storage.specialized_type);
    }
}

void codegen_field(const FieldStorage& storage, const char* attributes_array_name, CodeGenerator* codegen)
{
    String template_args_array_name(temp_allocator());
    String serializer_function_name(temp_allocator());

    const auto& field = storage.field;
    if (field.type->is(TypeKind::template_decl))
    {
        template_args_array_name = str::format(temp_allocator(), "%s__template_args", field.name);
    }

    if (has_serializer_function(storage))
    {
        serializer_function_name = str::format(temp_allocator(), "%s__serializer_function", field.name);
    }
    else
    {
        serializer_function_name.append("nullptr");
    }

    codegen->write("Field");
    codegen->scope([&]()
    {
        if (field.type->is(TypeKind::template_decl))
        {
            codegen->write(
                "%u, %zu, %s, %s, \"%s\", get_type<%s>(), Span<Type>(%s), %s, %s, %d, %d, %d",
                field.hash,
                field.offset,
                reflection_dump_flags(field.qualifier),
                reflection_dump_flags(field.storage_class),
                field.name,
                storage.specialized_type,
                template_args_array_name.empty() ? "" : template_args_array_name.c_str(),
                attributes_array_name == nullptr ? "{}" : attributes_array_name,
                serializer_function_name.c_str(),
                field.version_added,
                field.version_removed,
                field.template_argument_in_parent
            );
        }
        else
        {
            codegen->write(
                "%u, %zu, %s, %s, \"%s\", get_type<%s>(), Span<Type>(%s), %s, %s, %d, %d, %d",
                field.hash,
                field.offset,
                reflection_dump_flags(field.qualifier),
                reflection_dump_flags(field.storage_class),
                field.name,
                field.type->name,
                template_args_array_name.empty() ? "" : template_args_array_name.c_str(),
                attributes_array_name == nullptr ? "{}" : attributes_array_name,
                serializer_function_name.c_str(),
                field.version_added,
                field.version_removed,
                field.template_argument_in_parent
            );
        }
    });
}

void codegen_array_type(CodeGenerator* codegen, ArrayTypeStorage* storage)
{
    if (storage->is_generated)
    {
        return;
    }

    storage->is_generated = true;
    auto& type = storage->type;

    codegen->write_type_signature(type);
    codegen->scope([&]()
    {
        codegen_create_instance(type, codegen);

        if (storage->uses_builder)
        {
            codegen_serializer_function(codegen, "elements", storage->element_type_name);
        }
        codegen->write("static ArrayTypeInfo instance");
        codegen->scope([&]()
        {
            codegen_type(CodegenTypeOptions::none, type, codegen);
            codegen->append_line(
                "%d, get_type<%s>(), %s",
                type.element_count,
                storage->element_type_name,
                storage->uses_builder ? "elements__serializer_function" : "nullptr"
            );
        }, ";\n\n");
        codegen->write_line("return Type(&instance);");
    });
    codegen->write("// get_type<%s>()\n", type.name);

    codegen->newline();
}


void codegen_function(CodeGenerator* codegen, const FunctionTypeStorage* storage)
{
    const auto function_name_as_ident = get_name_as_ident(storage->type, temp_allocator());

    if (!storage->attributes.empty())
    {
        codegen->write("static Attribute %s__attributes[] =", function_name_as_ident.c_str());
        codegen->scope([&]()
        {
            for (const Attribute& attr : storage->attributes)
            {
                codegen_attribute(attr, codegen);
                codegen->append_line(",\n");
            }
        }, ";");
        codegen->newline();
        codegen->newline();
    }

    if (!storage->parameters.empty())
    {
        // Generate all the template type arguments for each of the parameters if needed
        for (const FieldStorage& field : storage->parameters)
        {
            codegen_field_extra_info(field, codegen);
        }

        codegen->write("static Field %s__parameters[] =", function_name_as_ident.c_str());
        codegen->scope([&]()
        {
            for (const FieldStorage& field : storage->parameters)
            {
                codegen_field(field, nullptr, codegen);
                codegen->append_line(",\n");
            }
        }, ";");
        codegen->newline();
        codegen->newline();
    }

    codegen_field_extra_info(storage->return_field, codegen); // generate return value template args if needed

    codegen->write("static FunctionTypeInfo %s", function_name_as_ident.c_str());
    codegen->scope([&]()
    {
        codegen_type(CodegenTypeOptions::none, storage->type, codegen);
        codegen->append_line("%s, %s,", reflection_dump_flags(storage->type.storage_class), storage->type.is_constexpr ? "true" : "false");
        codegen->newline();

        codegen_field(storage->return_field, nullptr, codegen);
        codegen->append_line(", // return value");
        codegen->newline();

        if (!storage->parameters.empty())
        {
            codegen->write("Span<Field>(%s__parameters), ", function_name_as_ident.c_str());
        }
        else
        {
            codegen->write("{}, ");
        }

        if (!storage->attributes.empty())
        {
            codegen->append_line("Span<Attribute>(%s__attributes), ", function_name_as_ident.c_str());
        }
        else
        {
            codegen->append_line("{}, ");
        }

        if (!storage->type.is(TypeKind::method))
        {
            codegen->append_line("FunctionTypeInvoker::from<");
            for (const auto invoker_arg : enumerate(storage->invoker_type_args))
            {
                codegen->append_line(" %s", invoker_arg.value.c_str());
                if (invoker_arg.index < storage->invoker_type_args.size() - 1)
                {
                    codegen->append_line(",");
                }
            }
            codegen->append_line(">(%s)", storage->type.name);
        }
        else
        {
            log_warning("bee-reflect: cannot generate function invoker for type %s: method invokers are not supported yet", storage->type.name);
            codegen->append_line("{}");
        }

    }, ";");
}

void codegen_enum(CodeGenerator* codegen, const EnumTypeStorage* storage)
{
    codegen->write_type_signature(storage->type);
    codegen->scope([&]()
    {
        if (!storage->attributes.empty())
        {
            codegen->write("static Attribute attributes[] =");
            codegen->scope([&]()
            {
                for (const Attribute& attr : storage->attributes)
                {
                    codegen_attribute(attr, codegen);
                    codegen->append_line(",\n");
                }
            }, ";");
            codegen->newline();
            codegen->newline();
        }

        codegen->write("static EnumConstant constants[] =");
        codegen->scope([&]()
        {
            for (const EnumConstant& constant : storage->constants)
            {
                codegen->write_line(
                    "EnumConstant { \"%s\", %u, %" PRIi64 ", get_type<%s>(), %s },",
                    constant.name,
                    get_type_hash(constant.name),
                    constant.value,
                    constant.underlying_type->name,
                    storage->type.is_flags ? "true" : "false"
                );
            }
        }, ";");
        codegen->newline();
        codegen->newline();

        codegen_create_instance(storage->type, codegen);

        codegen->write("static EnumTypeInfo instance");
        codegen->scope([&]()
        {
            codegen_type(CodegenTypeOptions::none, storage->type, codegen);
            codegen->append_line(
                "%s, Span<EnumConstant>(constants), %s",
                storage->type.is_scoped ? "true" : "false",
                storage->attributes.empty() ? "{}" : "Span<Attribute>(attributes)"
            );
        }, ";");
        codegen->newline();
        codegen->newline();
        codegen->write_line("return Type(&instance);");
    });
    codegen->write_line("// get_type<%s>()\n", storage->type.name);
}

void codegen_record(CodeGenerator* codegen, const RecordTypeStorage* storage)
{
    // Generate all the dependent types first - including any array types declared on this record
    for (ArrayTypeStorage* array_type : storage->field_array_types)
    {
        codegen->generate(array_type, codegen_array_type);
    }

    for (const EnumTypeStorage* nested_enum : storage->enums)
    {
        codegen->generate(nested_enum, codegen_enum);
    }

    for (const RecordTypeStorage* nested_record : storage->nested_records)
    {
        codegen->generate(nested_record, codegen_record);
    }

    if (storage->type.is(TypeKind::template_decl))
    {
        codegen->write_line("%s", storage->template_decl_string);
    }

    codegen->write_type_signature(storage->type);

    codegen->scope([&]()
    {
        const auto name_as_ident = get_name_as_ident(storage->type, temp_allocator());

        codegen_template_parameters(storage->template_parameters.const_span(), codegen);

        if (!storage->attributes.empty())
        {
            codegen->write("static Attribute %s__attributes[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const Attribute& attr : storage->attributes)
                {
                    codegen_attribute(attr, codegen);
                    codegen->append_line(",\n");
                }
            }, ";");
            codegen->newline();
            codegen->newline();
        }

        if (!storage->fields.empty())
        {
            for (const FieldStorage& field_storage : storage->fields)
            {
                const auto& field = field_storage.field;

                codegen_field_extra_info(field_storage, codegen);

                if (field_storage.attributes.empty())
                {
                    continue;
                }

                codegen->write("static Attribute %s__%s__attributes[] =", name_as_ident.c_str(), field.name);
                codegen->scope([&]()
                {
                    for (const Attribute& attr : field_storage.attributes)
                    {
                        codegen_attribute(attr, codegen);
                        codegen->append_line(",\n");
                    }
                }, ";");

                codegen->newline();
                codegen->newline();
            }

            codegen->write("static Field %s__fields[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const FieldStorage& field_storage : storage->fields)
                {
                    const auto& field = field_storage.field;

                    const char* attr_array_name = field_storage.attributes.empty()
                                                  ? nullptr
                                                  : str::format(temp_allocator(), "Span<Attribute>(%s__%s__attributes)", name_as_ident.c_str(), field.name).c_str();

                    codegen_field(field_storage, attr_array_name, codegen);
                    codegen->append_line(",\n");
                }
            }, ";");
            codegen->write_line("// %s__fields[]\n", name_as_ident.c_str());
        }

        if (!storage->functions.empty())
        {
            for (const FunctionTypeStorage* function : storage->functions)
            {
                codegen_function(codegen, function);
                codegen->newline();
            }

            codegen->newline();
            codegen->write("static FunctionTypeInfo %s__functions[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const FunctionTypeStorage* function : storage->functions)
                {
                    const auto function_name_as_ident = get_name_as_ident(function->type, temp_allocator());
                    codegen->write_line("%s,", function_name_as_ident.c_str());
                }
            }, ";");
            codegen->write_line("// %s__functions[]\n", name_as_ident.c_str());
        }

        if (!storage->nested_records.empty())
        {
            codegen->write("static RecordType %s__records[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const RecordTypeStorage* record : storage->nested_records)
                {
                    codegen->write_line("get_type_as<%s, RecordTypeInfo>(),", record->type.name);
                }
            }, ";");
            codegen->write_line("// %s__records[]\n", name_as_ident.c_str());
        }

        if (!storage->enums.empty())
        {
            codegen->write("static EnumType %s__enums[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const EnumTypeStorage* enum_type : storage->enums)
                {
                    codegen->write_line("get_type_as<%s, EnumTypeInfo>(),", enum_type->type.name);
                }
            }, ";");
            codegen->write_line("// %s__enums[]\n", name_as_ident.c_str());
        }

        if (!storage->base_type_names.empty())
        {
            codegen->write("static Type %s__base_types[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const char* base_name : storage->base_type_names)
                {
                    codegen->write_line("get_type<%s>(),", base_name);
                }
            }, ";");
            codegen->write_line("// %s__base_types[]\n", name_as_ident.c_str());
        }

        codegen_create_instance(storage->type, codegen);

        codegen->write("static RecordTypeInfo instance");
        codegen->scope([&]()
        {
            codegen_type(CodegenTypeOptions::use_explicit_kind_flags, storage->type, codegen);

            if (!storage->fields.empty())
            {
                codegen->append_line("Span<Field>(%s__fields)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->functions.empty())
            {
                codegen->append_line("Span<FunctionTypeInfo>(%s__functions)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->attributes.empty())
            {
                codegen->append_line("Span<Attribute>(%s__attributes)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->enums.empty())
            {
                codegen->append_line("Span<EnumType>(%s__enums)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->nested_records.empty())
            {
                codegen->append_line("Span<RecordType>(%s__records)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->base_type_names.empty())
            {
                codegen->append_line("Span<Type>(%s__base_types)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

        }, ";\n\n");
        codegen->write_line("return Type(&instance);");
    });
    codegen->write_line("// get_type<%s>()\n", storage->type.name);
}


void generate_empty_reflection(const Path& dst_path, const char* location, io::StringStream* stream)
{
    CodeGenerator codegen(CodegenMode::cpp, stream);
    const auto relative_location = Path(location).relative_to(dst_path).make_generic();
    codegen.write_header_comment(relative_location);
    codegen.write_line("// THIS FILE IS INTENTIONALLY EMPTY - NO REFLECTION DATA WAS GENERATED");
}

i32 generate_reflection(const Path& dst_path, const ReflectedFile& file, io::StringStream* src_stream, const CodegenMode mode)
{
    CodeGenerator codegen(mode, src_stream);
    const auto relative_location = file.location.relative_to(dst_path, temp_allocator()).make_generic();

    int types_generated = 0;
    codegen.write_header_comment(relative_location);
    codegen.newline();

    if (mode == CodegenMode::cpp)
    {
        codegen.write_line("#include \"%s\"", relative_location.c_str());
        codegen.write_line("#include <Bee/Core/Reflection.hpp>");
        codegen.newline();
    }

    codegen.write("namespace bee ");
    codegen.scope([&]()
    {
        if (mode != CodegenMode::cpp)
        {
            codegen.write_line("struct TypeInfo;");
            codegen.write_line("class Type;");
            codegen.write_line("template <typename T> struct TypeTag;");
            codegen.newline();
        }

        for (ArrayTypeStorage* type : file.arrays)
        {
            if (!codegen.should_generate(type->type))
            {
                continue;
            }

            codegen.generate(type, codegen_array_type);
            ++types_generated;
        }

        for (const RecordTypeStorage* type : file.records)
        {
            if (!codegen.should_generate(type->type))
            {
                continue;
            }

            codegen.generate(type, codegen_record);
            ++types_generated;
        }

        for (const FunctionTypeStorage* function : file.functions)
        {
            if (!codegen.should_generate(function->type))
            {
                continue;
            }

            codegen.type_guard_begin(&function->type);
            codegen.write_type_signature(function->type);
            codegen.scope([&]()
            {
                codegen_create_instance(function->type, &codegen);
                codegen_function(&codegen, function);
                codegen.newline();
                codegen.write_line("return &%s;", get_name_as_ident(function->type, temp_allocator()).c_str());
            });
            codegen.write_line("// get_type<%s>()\n", function->type.name);
            codegen.type_guard_end(&function->type);
            ++types_generated;
        }

        for (const EnumTypeStorage* type : file.enums)
        {
            if (!codegen.should_generate(type->type))
            {
                continue;
            }

            codegen.generate(type, codegen_enum);
            ++types_generated;
        }
    }, " // namespace bee\n");
    codegen.newline();

    return types_generated;
}


void generate_typelist(const Path& target_dir, const Span<const TypeInfo*>& all_types, const CodegenMode mode, const Span<const Path>& written_files)
{
    String output;
    io::StringStream stream(&output);

    const auto target_name = get_target_name_as_ident(target_dir.filename());
    const auto output_path = target_dir.join("TypeList.init.cpp");

    CodeGenerator codegen(CodegenMode::cpp, &stream);
    codegen.write_header_comment(target_dir);

    codegen.write_line("#include <Bee/Core/Reflection.hpp>");
    codegen.newline();

    if (mode == CodegenMode::inl)
    {
        for (const auto& path : written_files)
        {
            codegen.write_line("#include \"%" BEE_PRIsv "\"", BEE_FMT_SV(path.view()));
        }
    }
    else
    {
        for (const TypeInfo* type : all_types)
        {
            // TODO(Jacob): template types need supporting
            if (type->is(TypeKind::template_decl))
            {
                continue;
            }

            int ns_count = 0;
            for (const auto ns : type->namespaces())
            {
                codegen.write("namespace %" BEE_PRIsv " { ", BEE_FMT_SV(ns));
                ++ns_count;
            }
            codegen.write("%s %s;", reflection_type_kind_to_code_string(type->kind), type->unqualified_name());
            for (int ns = 0; ns < ns_count; ++ns)
            {
                codegen.write(" }");
            }
            codegen.newline();
        }
    }

    codegen.newline();
    codegen.write_line("#define %s_TYPE_REGISTRATION \\", target_name.c_str());
    for (const auto type : enumerate(all_types))
    {
        // TODO(Jacob): template types need supporting
        if (type.value->is(TypeKind::template_decl))
        {
            continue;
        }

        codegen.write("    BEE_REGISTER_TYPE(%s)", type.value->name);

        if (type.index < all_types.size() - 1)
        {
            codegen.append_line(" \\");
        }

        codegen.newline();
    }

    codegen.newline();

    codegen.write("struct BEE_EXPORT_SYMBOL %s_AutoTypeRegistration // NOLINT", target_name.c_str());
    codegen.scope([&]()
    {
        codegen.write("%s_AutoTypeRegistration() noexcept", target_name.c_str());
        codegen.scope([&]()
        {
            codegen.write_line("#define BEE_REGISTER_TYPE(X) ::bee::register_type(bee::get_type<X>());");
            codegen.write_line("%s_TYPE_REGISTRATION", target_name.c_str());
            codegen.write_line("#undef BEE_REGISTER_TYPE");
        });

        codegen.newline();

        codegen.write("~%s_AutoTypeRegistration()", target_name.c_str());
        codegen.scope([&]()
        {
            codegen.write_line("#define BEE_REGISTER_TYPE(X) ::bee::unregister_type(bee::get_type<X>());");
            codegen.write_line("%s_TYPE_REGISTRATION", target_name.c_str());
            codegen.write_line("#undef BEE_REGISTER_TYPE");
        });
    }, "; // struct BEE_EXPORT_SYMBOL AutoTypeRegistration");
    codegen.newline();
    codegen.newline();
    codegen.write_line("static %s_AutoTypeRegistration %s_auto_type_registration; // NOLINT", target_name.c_str(), target_name.c_str());

    fs::write(output_path, output.view());
}


} // namespace reflect
} // namespace bee