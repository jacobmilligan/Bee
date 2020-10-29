/*
 *  Codegen2.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "CodeGen.hpp"
#include "Storage.hpp"

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Bit.hpp"


namespace bee {
namespace reflect {


BEE_FLAGS(CodegenTypeOptions, u32)
{
    none                    = 0u,
    use_explicit_kind_flags = 1u << 0u // uses the types explicit kind flags rather than using the types `static_kind`
};

static void get_kind_as_macro(String* dst, const TypeKind kind)
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

i32 CodeGenerator::generated_count() const
{
    return generated_count_;
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

void CodeGenerator::newline(const i32 count)
{
    for (int i = 0; i < count; ++i)
    {
        stream_->write("\n");
    }
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

const char * CodeGenerator::as_ident(const StringView& name, const char* prefix)
{
    if (prefix != nullptr)
    {
        ident_buffer_.assign(prefix);
    }
    else
    {
        ident_buffer_.clear();
    }

    for (int i = 0; i < name.size(); ++i)
    {
        switch (name[i])
        {
            case ':':
            {
                const bool is_double_colon = i > 0 && name[i - 1] == ':';
                const bool is_single_colon = i < name.size() - 1 && name[i + 1] != ':';
                if (is_double_colon || is_single_colon)
                {
                    ident_buffer_.append('_');
                }
                break;
            }
            case ' ':
            case '<':
            case '>':
            case '[':
            case ']':
            case ',':
            case '-':
            case '.':
            {
                ident_buffer_.append('_');
                break;
            }
            default:
            {
                ident_buffer_.append(name[i]);
                break;
            }
        }
    }

    return ident_buffer_.c_str();
}

const char* CodeGenerator::as_ident(const TypeInfo& type)
{
    return as_ident(type.name, type.is(TypeKind::array) ? "AT_" : nullptr);
}

BEE_TRANSLATION_TABLE(export_or_inline, CodegenMode, const char*, CodegenMode::none,
    "BEE_EXPORT_SYMBOL",    // cpp
    "inline",               // inl
    "inline"                // templates_only
)

void CodeGenerator::write_type_signature(const TypeInfo& type, const CodegenMode force_mode)
{
    const auto selected_mode = force_mode == CodegenMode::none ? mode_ : force_mode;

    switch (selected_mode)
    {
        case CodegenMode::cpp:
        {
            if (type.is(TypeKind::function))
            {
                write(
                    "template <> %s Type get_type<BEE_NONMEMBER(%s)>(const TypeTag<BEE_NONMEMBER(%s)>& tag)",
                    export_or_inline(mode_),
                    type.name,
                    type.name
                );
            }
            else
            {
                write(
                    "template <> %s Type get_type<%s>(const TypeTag<%s>& tag)",
                    export_or_inline(mode_),
                    type.name,
                    type.name
                );
            }
            break;
        }
        case CodegenMode::templates_only:
        {
            write("inline Type get_type(const TypeTag<%s>& tag)", type.name);
            break;
        }
        case CodegenMode::inl:
        {
            if (!type.is(TypeKind::array))
            {
                write("Type get_type__%s()", as_ident(type));
            }
            else
            {
                write("template <> Type get_type<%s>(const TypeTag<%s>& tag)", type.name, type.name);
            }
            break;
        }
        default: break;
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

/*
 * Avoids issues where types may be included multiple times i.e. with array types (an int[4] defined
 * in two places is effectively the same type) - this is really an issue only with inline get_types and arrays
 */
void CodeGenerator::type_guard_begin(const TypeInfo* type)
{
    String type_kind(temp_allocator());
    get_kind_as_macro(&type_kind, type->kind);

    auto old_indent = set_indent(0);
    {
        const char* ident = as_ident(*type);
        write_line("#ifndef BEE_%s_TYPE__%s", type_kind.c_str(), ident);
        write_line("#define BEE_%s_TYPE__%s", type_kind.c_str(), ident);
        newline();
    }
    set_indent(old_indent);
}

void CodeGenerator::type_guard_end(const TypeInfo* type)
{
    String type_kind(temp_allocator());
    get_kind_as_macro(&type_kind, type->kind);

    if (mode_ == CodegenMode::cpp)
    {
        write_line("Type get_type__%s() { return get_type<%s>(); }", as_ident(*type), type->name);
    }

    auto old_indent = set_indent(0);
    {
        write_line("#endif // BEE_%s_TYPE__%s", type_kind.c_str(), as_ident(*type));
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
                R"(TemplateParameter { 0x%)" PRIx32 R"(, "%s", "%s" },)",
                param.hash,
                param.name,
                param.type_name
            );
        }
    }, ";");
    codegen->newline(2);
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
    codegen->newline(2);
}

void codegen_type(const CodegenTypeOptions options, const TypeInfo& type, CodeGenerator* codegen)
{
    const auto* const size_align_type = type.is(TypeKind::function) ? "void*" : type.name;

    codegen->write(
        "0x%" PRIx32 ", sizeof(%s), alignof(%s), ",
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
            "%s, 0x%" PRIx32 ", \"%s\", Attribute::Value(",
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
    codegen->newline(2);
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

        codegen->newline(2);
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
                "0x%" PRIx32 ", %zu, %s, %s, \"%s\", get_type<%s>(), Span<Type>(%s), %s, %s, %d, %d, %d",
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
                "0x%" PRIx32 ", %zu, %s, %s, \"%s\", get_type<%s>(), Span<Type>(%s), %s, %s, %d, %d, %d",
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
    if (!storage->attributes.empty())
    {
        codegen->write("static Attribute %s__attributes[] =", codegen->as_ident(storage->type));
        codegen->scope([&]()
        {
            for (const Attribute& attr : storage->attributes)
            {
                codegen_attribute(attr, codegen);
                codegen->append_line(",\n");
            }
        }, ";");
        codegen->newline(2);
    }

    if (!storage->parameters.empty())
    {
        // Generate all the template type arguments for each of the parameters if needed
        for (const FieldStorage& field : storage->parameters)
        {
            codegen_field_extra_info(field, codegen);
        }

        codegen->write("static Field %s__parameters[] =", codegen->as_ident(storage->type));
        codegen->scope([&]()
        {
            for (const FieldStorage& field : storage->parameters)
            {
                codegen_field(field, nullptr, codegen);
                codegen->append_line(",\n");
            }
        }, ";");
        codegen->newline(2);
    }

    codegen_field_extra_info(storage->return_field, codegen); // generate return value template args if needed

    codegen->write("static FunctionTypeInfo %s", codegen->as_ident(storage->type));
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
            codegen->write("Span<Field>(%s__parameters), ", codegen->as_ident(storage->type));
        }
        else
        {
            codegen->write("{}, ");
        }

        if (!storage->attributes.empty())
        {
            codegen->append_line("Span<Attribute>(%s__attributes), ", codegen->as_ident(storage->type));
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
            codegen->newline(2);
        }

        codegen->write("static EnumConstant constants[] =");
        codegen->scope([&]()
        {
            for (const EnumConstant& constant : storage->constants)
            {
                codegen->write_line(
                    "EnumConstant { \"%s\", 0x%" PRIx32 ", %" PRIi64 ", get_type<%s>(), %s },",
                    constant.name,
                    get_type_hash(constant.name),
                    constant.value,
                    constant.underlying_type->name,
                    storage->type.is_flags ? "true" : "false"
                );
            }
        }, ";");
        codegen->newline(2);

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
        codegen->newline(2);
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
        codegen_template_parameters(storage->template_parameters.const_span(), codegen);

        if (!storage->attributes.empty())
        {
            codegen->write("static Attribute %s__attributes[] =", codegen->as_ident(storage->type));
            codegen->scope([&]()
            {
                for (const Attribute& attr : storage->attributes)
                {
                    codegen_attribute(attr, codegen);
                    codegen->append_line(",\n");
                }
            }, ";");
            codegen->newline(2);
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

                codegen->write("static Attribute %s__%s__attributes[] =", codegen->as_ident(storage->type), field.name);
                codegen->scope([&]()
                {
                    for (const Attribute& attr : field_storage.attributes)
                    {
                        codegen_attribute(attr, codegen);
                        codegen->append_line(",\n");
                    }
                }, ";");

                codegen->newline(2);
            }

            codegen->write("static Field %s__fields[] =", codegen->as_ident(storage->type));
            codegen->scope([&]()
            {
                for (const FieldStorage& field_storage : storage->fields)
                {
                    const auto& field = field_storage.field;

                    const char* attr_array_name = field_storage.attributes.empty()
                                                  ? nullptr
                                                  : str::format(temp_allocator(), "Span<Attribute>(%s__%s__attributes)", codegen->as_ident(storage->type), field.name).c_str();

                    codegen_field(field_storage, attr_array_name, codegen);
                    codegen->append_line(",\n");
                }
            }, ";");
            codegen->write_line("// %s__fields[]\n", codegen->as_ident(storage->type));
        }

        if (!storage->functions.empty())
        {
            for (const FunctionTypeStorage* function : storage->functions)
            {
                codegen_function(codegen, function);
                codegen->newline();
            }

            codegen->newline();
            codegen->write("static FunctionTypeInfo %s__functions[] =", codegen->as_ident(storage->type));
            codegen->scope([&]()
            {
                for (const FunctionTypeStorage* function : storage->functions)
                {
                    codegen->write_line("%s,", codegen->as_ident(function->type));
                }
            }, ";");
            codegen->write_line("// %s__functions[]\n", codegen->as_ident(storage->type));
        }

        if (!storage->nested_records.empty())
        {
            codegen->write("static RecordType %s__records[] =", codegen->as_ident(storage->type));
            codegen->scope([&]()
            {
                for (const RecordTypeStorage* record : storage->nested_records)
                {
                    codegen->write_line("get_type_as<%s, RecordTypeInfo>(),", record->type.name);
                }
            }, ";");
            codegen->write_line("// %s__records[]\n", codegen->as_ident(storage->type));
        }

        if (!storage->enums.empty())
        {
            codegen->write("static EnumType %s__enums[] =", codegen->as_ident(storage->type));
            codegen->scope([&]()
            {
                for (const EnumTypeStorage* enum_type : storage->enums)
                {
                    codegen->write_line("get_type_as<%s, EnumTypeInfo>(),", enum_type->type.name);
                }
            }, ";");
            codegen->write_line("// %s__enums[]\n", codegen->as_ident(storage->type));
        }

        if (!storage->base_type_names.empty())
        {
            codegen->write("static Type %s__base_types[] =", codegen->as_ident(storage->type));
            codegen->scope([&]()
            {
                for (const char* base_name : storage->base_type_names)
                {
                    codegen->write_line("get_type<%s>(),", base_name);
                }
            }, ";");
            codegen->write_line("// %s__base_types[]\n", codegen->as_ident(storage->type));
        }

        codegen_create_instance(storage->type, codegen);

        codegen->write("static RecordTypeInfo instance");
        codegen->scope([&]()
        {
            codegen_type(CodegenTypeOptions::use_explicit_kind_flags, storage->type, codegen);

            if (!storage->fields.empty())
            {
                codegen->append_line("Span<Field>(%s__fields)", codegen->as_ident(storage->type));
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->functions.empty())
            {
                codegen->append_line("Span<FunctionTypeInfo>(%s__functions)", codegen->as_ident(storage->type));
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->attributes.empty())
            {
                codegen->append_line("Span<Attribute>(%s__attributes)", codegen->as_ident(storage->type));
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->enums.empty())
            {
                codegen->append_line("Span<EnumType>(%s__enums)", codegen->as_ident(storage->type));
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->nested_records.empty())
            {
                codegen->append_line("Span<RecordType>(%s__records)", codegen->as_ident(storage->type));
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->base_type_names.empty())
            {
                codegen->append_line("Span<Type>(%s__base_types)", codegen->as_ident(storage->type));
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

i32 generate_reflection(const Path& dst_path, const ReflectedFile& file, io::StringStream* stream, CodegenMode mode)
{
    CodeGenerator codegen(mode, stream);
    const auto relative_location = file.location.relative_to(dst_path.parent_view(), temp_allocator()).make_generic();

    int functions_generated = 0;
    codegen.write_header_comment(relative_location);
    codegen.newline();

    if (mode != CodegenMode::templates_only)
    {
        codegen.write_line("#include \"%s\"", relative_location.c_str());
        codegen.write_line("#include <Bee/Core/Reflection.hpp>");
        codegen.newline();
    }

    codegen.write("namespace bee ");
    codegen.scope([&]()
    {
        if (mode == CodegenMode::templates_only)
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
        }

        for (const RecordTypeStorage* type : file.records)
        {
            if (!codegen.should_generate(type->type))
            {
                continue;
            }

            codegen.generate(type, codegen_record);
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
                codegen.generate_no_guard(function, codegen_function);
                codegen.newline();
                codegen.write_line("return &%s;", codegen.as_ident(function->type));
            });
            codegen.write_line("// get_type<%s>()\n", function->type.name);
            codegen.type_guard_end(&function->type);
            ++functions_generated;
        }

        for (const EnumTypeStorage* type : file.enums)
        {
            if (!codegen.should_generate(type->type))
            {
                continue;
            }

            codegen.generate(type, codegen_enum);
        }
    }, " // namespace bee\n");
    codegen.newline();

    return codegen.generated_count();
}

i32 generate_reflection_header(const Path& dst_path, const ReflectedFile& file, const i32 first_type_index, io::StringStream* stream, CodegenMode mode)
{
    CodeGenerator codegen(mode, stream);
    const auto target_path = dst_path.parent_path();
    const auto relative_location = file.location.relative_to(target_path, temp_allocator()).make_generic();
    const auto target_name = target_path.filename();
    const auto target_as_ident = codegen.as_ident(target_name);
    String target_as_macro(target_as_ident);
    str::uppercase_ascii(&target_as_macro);

    codegen.write_header_comment(relative_location);
    codegen.newline();

    int generated_count = 0;

    codegen.write_line("namespace bee {");
    {
        codegen.newline(2);
        codegen.write_line("#ifndef BEE_INLINE_GET_REFLECTION_MODULE__%s", target_as_macro.c_str());
        codegen.write_line("#define BEE_INLINE_GET_REFLECTION_MODULE__%s", target_as_macro.c_str());
        codegen.write("inline const ReflectionModule* get_reflection_module__%s()", target_as_ident);
        codegen.scope([&]()
        {
            codegen.write_line(
                "static const ReflectionModule* module = get_reflection_module(\"%" BEE_PRIsv "\");",
                BEE_FMT_SV(target_name)
            );
            codegen.write("return module;");
        });
        codegen.newline();
        codegen.write_line("#endif // BEE_INLINE_GET_REFLECTION_MODULE__%s", target_as_macro.c_str());
        codegen.newline();

        for (const auto* type : file.all_types)
        {
            if (type->is(TypeKind::template_decl))
            {
                continue;
            }

            codegen.write_type_signature(*type, CodegenMode::cpp);
            codegen.scope([&]()
            {
                codegen.write(
                    "return get_type(get_reflection_module__%s(), %d);",
                    target_as_ident,
                    first_type_index + generated_count
                );
            });
            codegen.newline(2);
            ++generated_count;
        }

        codegen.newline(2);
    }
    codegen.write_line("} // namespace bee");

    return generated_count;
}

void generate_typelist(const Path& target_dir, const Span<const TypeInfo*>& all_types, CodegenMode mode, const Span<const Path>& written_files)
{
    String output;
    io::StringStream stream(&output);
    const auto output_path = target_dir.join("TypeList.init.cpp");

    CodeGenerator codegen(CodegenMode::cpp, &stream);
    codegen.write_header_comment(output_path.filename());

    codegen.write_line("#include <Bee/Core/Reflection.hpp>");
    codegen.newline();

    int callback_count = 0;

    codegen.write("namespace bee {");
    {
        codegen.newline(2);
        for (const auto* type : all_types)
        {
            if (type->is(TypeKind::template_decl))
            {
                continue;
            }
            codegen.write_line("extern Type get_type__%s();", codegen.as_ident(*type));
            ++callback_count;
        }
        codegen.newline(2);
    }
    codegen.write_line("} // namespace bee");

    codegen.newline();
    codegen.write("%s const void* bee_load_reflection()", mode == CodegenMode::cpp ? "static" : "BEE_EXPORT_SYMBOL");
    codegen.scope([&]()
    {
        codegen.write("bee::u32 hashes[] =");
        codegen.scope([&]()
        {
            int generated_count = 0;

            for (const auto* type : all_types)
            {
                if (type->is(TypeKind::template_decl))
                {
                    continue;
                }

                codegen.write("0x%" PRIx32 ",", type->hash);
                if (generated_count < callback_count - 1)
                {
                    codegen.newline();
                }
                ++generated_count;
            }
        }, ";");

        codegen.newline(2);

        codegen.write("bee::get_type_callback_t callbacks[] =");
        codegen.scope([&]()
        {
            int generated_count = 0;

            for (const auto* type : all_types)
            {
                if (type->is(TypeKind::template_decl))
                {
                    continue;
                }

                codegen.write("bee::get_type__%s,", codegen.as_ident(*type));
                if (generated_count < callback_count - 1)
                {
                    codegen.newline();
                }
                ++generated_count;
            }
        }, ";");

        codegen.newline(2);

        codegen.write(
            "return bee::create_reflection_module(\"%" BEE_PRIsv "\", %d, hashes, callbacks);",
            BEE_FMT_SV(target_dir.filename()),
            callback_count
        );
    });

    codegen.newline(2);

    if (mode == CodegenMode::cpp)
    {
        const char* target_ident = codegen.as_ident(target_dir.filename());

        codegen.write("struct BEE_EXPORT_SYMBOL %s_AutoTypeRegistration", target_ident);
        codegen.scope([&]()
        {
            codegen.write("%s_AutoTypeRegistration() noexcept { bee_load_reflection(); }", target_ident);
        }, ";");

        codegen.newline(2);

        codegen.write_line("static %s_AutoTypeRegistration %s_auto_type_registration{};", target_ident, target_ident);
    }

    fs::write(output_path, output.view());
}


} // namespace reflect
} // namespace bee