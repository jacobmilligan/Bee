/*
 *  CodeGen.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "CodeGen.hpp"

#include "Bee/Core/IO.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Storage.hpp"

#include <map>
#include <set>
#include <inttypes.h>
#include <Bee/Core/Reflection.hpp>


namespace bee {
namespace reflect {

BEE_FLAGS(CodegenTypeOptions, u32)
{
    none                    = 0u,
    use_explicit_kind_flags = 1u << 0u // uses the types explicit kind flags rather than using the types `static_kind`
};


String get_name_as_ident(const Type& type, Allocator* allocator = system_allocator())
{
    String name_as_ident(type.name, allocator);
    str::replace(&name_as_ident, "::", "_");
    str::replace(&name_as_ident, " ", "_");
    str::replace(&name_as_ident, "<", "_");
    str::replace(&name_as_ident, ">", "_");
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
    return name_as_ident;
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

void codegen_create_instance(const Type& type, CodeGenerator* codegen)
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

void codegen_type(const CodegenTypeOptions options, const Type& type, CodeGenerator* codegen)
{
    const auto size_align_type = type.is(TypeKind::function) ? "void*" : type.name;

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
    auto& field = storage.field;
    const auto uses_builder = (field.type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none;
    const auto is_templated_and_serialized = !storage.template_arguments.empty() && field.type->serialized_version > 0;
    return uses_builder || is_templated_and_serialized;
}

void codegen_field_extra_info(const FieldStorage& storage, CodeGenerator* codegen)
{
    auto& field = storage.field;
    if (field.type->is(TypeKind::template_decl))
    {
        codegen->write("static const Type* %s__template_args[] =", field.name);
        codegen->scope([&]()
        {
            codegen->indent();
            for (const Type* template_arg : storage.template_arguments)
            {
                codegen->append_line("get_type<%s>(), ", template_arg->name);
            }
        }, ";");

        codegen->newline();
        codegen->newline();
    }


    if (has_serializer_function(storage))
    {
        // [](SerializationBuilder* builder, void* data) { serialize_type(builder, static_cast<bee::GUID*>(data)); };
        codegen->write("static auto %s__serializer_function = [](SerializationBuilder* builder, void* data) { serialize_type(builder, static_cast<%s*>(data)); };", field.name, storage.specialized_type);
        codegen->newline();
        codegen->newline();
    }

}

void codegen_field(const FieldStorage& storage, const char* attributes_array_name, CodeGenerator* codegen)
{
    String template_args_array_name(temp_allocator());
    String serializer_function_name(temp_allocator());

    auto& field = storage.field;
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
                "%u, %zu, %s, %s, \"%s\", get_type<%s>(), Span<const Type*>(%s), %s, %s, %d, %d, %d",
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
                "%u, %zu, %s, %s, \"%s\", get_type<%s>(), Span<const Type*>(%s), %s, %s, %d, %d, %d",
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

void codegen_array_type(ArrayTypeStorage* storage, CodeGenerator* codegen)
{
    if (storage->is_generated)
    {
        return;
    }

    storage->is_generated = true;
    auto& type = storage->type;

    // Array types shouldn't be exported
    codegen->write("template <> BEE_EXPORT_SYMBOL const Type* get_type<%s>(const TypeTag<%s>& tag)", type.name, type.name);
    codegen->scope([&]()
    {
        codegen_create_instance(type, codegen);
        codegen->write("static ArrayType instance");
        codegen->scope([&]()
        {
            codegen_type(CodegenTypeOptions::none, type, codegen);
            codegen->append_line("%d, get_type<%s>()", type.element_count, type.element_type->name);
        }, ";\n\n");
        codegen->write_line("return &instance;");
    });
    codegen->write_line("// get_type<%s>()\n", type.name);
}


void codegen_function(const FunctionTypeStorage* storage, CodeGenerator* codegen)
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

    codegen->write("static FunctionType %s", function_name_as_ident.c_str());
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

void codegen_enum(const EnumTypeStorage* storage, CodeGenerator* codegen)
{
    codegen->write("template <> BEE_EXPORT_SYMBOL const Type* get_type<%s>(const TypeTag<%s>& tag)", storage->type.name, storage->type.name);
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

        codegen->write("static EnumType instance");
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
        codegen->write_line("return &instance;");
    });
    codegen->write_line("// get_type<%s>()\n", storage->type.name);
}

void codegen_record(const CodegenMode mode, const RecordTypeStorage* storage, CodeGenerator* codegen)
{
    // Generate all the dependent types first - including any array types declared on this record
    for (ArrayTypeStorage* array_type : storage->field_array_types)
    {
        codegen_array_type(array_type, codegen);
    }

    for (const EnumTypeStorage* nested_enum : storage->enums)
    {
        codegen_enum(nested_enum, codegen);
    }

    for (const RecordTypeStorage* nested_record : storage->nested_records)
    {
        codegen_record(mode, nested_record, codegen);
    }

    if (storage->type.is(TypeKind::template_decl))
    {
        codegen->write_line("%s", storage->template_decl_string);
        codegen->write("BEE_EXPORT_SYMBOL const Type* get_type(const TypeTag<%s>& tag)", storage->type.name);
    }
    else
    {
        codegen->write("template <> BEE_EXPORT_SYMBOL const Type* get_type<%s>(const TypeTag<%s>& tag)", storage->type.name, storage->type.name);
    }
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
                auto& field = field_storage.field;

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
                    auto& field = field_storage.field;

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
                codegen_function(function, codegen);
                codegen->newline();
            }

            codegen->newline();
            codegen->write("static FunctionType %s__functions[] =", name_as_ident.c_str());
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
            codegen->write("static const RecordType* %s__records[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const RecordTypeStorage* record : storage->nested_records)
                {
                    codegen->write_line("get_type_as<%s, RecordType>(),", record->type.name);
                }
            }, ";");
            codegen->write_line("// %s__records[]\n", name_as_ident.c_str());
        }

        if (!storage->enums.empty())
        {
            codegen->write("static const EnumType* %s__enums[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const EnumTypeStorage* enum_type : storage->enums)
                {
                    codegen->write_line("get_type_as<%s, EnumType>(),", enum_type->type.name);
                }
            }, ";");
            codegen->write_line("// %s__enums[]\n", name_as_ident.c_str());
        }

        if (!storage->base_type_names.empty())
        {
            codegen->write("static const Type* %s__base_types[] =", name_as_ident.c_str());
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

        codegen->write("static RecordType instance");
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
                codegen->append_line("Span<FunctionType>(%s__functions)", name_as_ident.c_str());
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
                codegen->append_line("Span<const EnumType*>(%s__enums)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->nested_records.empty())
            {
                codegen->append_line("Span<const RecordType*>(%s__records)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!storage->base_type_names.empty())
            {
                codegen->append_line("Span<const Type*>(%s__base_types)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

        }, ";\n\n");
        codegen->write_line("return &instance;");
    });
    codegen->write_line("// get_type<%s>()\n", storage->type.name);
}


bool should_generate(const Type& type, const CodegenMode mode)
{
    if (mode == CodegenMode::skip_templates)
    {
        return !type.is(TypeKind::template_decl);
    }

    if (mode == CodegenMode::templates_only)
    {
        return type.is(TypeKind::template_decl);
    }

    return true;
}


void generate_empty_reflection(const char* location, io::StringStream* stream)
{
    CodeGenerator codegen(stream);
    codegen.write_header_comment(location);
    codegen.write_line("// THIS FILE IS INTENTIONALLY EMPTY - NO REFLECTION DATA WAS GENERATED");
}


i32 generate_reflection(const ReflectedFile& file, io::StringStream* src_stream, const CodegenMode mode)
{
    CodeGenerator codegen(src_stream);

    int types_generated = 0;
    codegen.write_header_comment(file.location);
    codegen.newline();

    if (mode != CodegenMode::templates_only)
    {
        codegen.write_line("#include \"%s\"", file.location.to_generic_string(temp_allocator()).c_str());
        codegen.write_line("#include <Bee/Core/Reflection.hpp>");
        codegen.newline();
    }

    codegen.write("namespace bee ");
    codegen.scope([&]()
    {
        if (mode == CodegenMode::templates_only)
        {
            codegen.write_line("struct Type;");
            codegen.write_line("template <typename T> struct TypeTag;");
            codegen.newline();
        }

        for (ArrayTypeStorage* type : file.arrays)
        {
            if (!should_generate(type->type, mode))
            {
                continue;
            }

            codegen_array_type(type, &codegen);
            ++types_generated;
        }

        for (const RecordTypeStorage* type : file.records)
        {
            if (!should_generate(type->type, mode))
            {
                continue;
            }

            codegen_record(mode, type, &codegen);
            ++types_generated;
        }

        for (const FunctionTypeStorage* function : file.functions)
        {
            if (!should_generate(function->type, mode))
            {
                continue;
            }

            codegen.write("template <> BEE_EXPORT_SYMBOL const Type* get_type<BEE_NONMEMBER(%s)>(const TypeTag<BEE_NONMEMBER(%s)>& tag)", function->type.name, function->type.name);
            codegen.scope([&]()
            {
                codegen_create_instance(function->type, &codegen);
                codegen_function(function, &codegen);
                codegen.newline();
                codegen.write_line("return &%s;", get_name_as_ident(function->type, temp_allocator()).c_str());
            });
            codegen.write_line("// get_type<%s>()\n", function->type.name);
            codegen_function(function, &codegen);
            ++types_generated;
        }

        for (const EnumTypeStorage* type : file.enums)
        {
            if (!should_generate(type->type, mode))
            {
                continue;
            }

            codegen_enum(type, &codegen);
            ++types_generated;
        }
    }, " // namespace bee\n");
    codegen.newline();

    return types_generated;
}


void generate_typelist(const Path& target_dir, const llvm::ArrayRef<std::string>& target_dependencies, const Span<const Type*>& all_types)
{
    String output;
    io::StringStream stream(&output);

    const auto target_name = get_target_name_as_ident(target_dir.filename());
    const auto output_path = target_dir.join("TypeList.generated.hpp");

    CodeGenerator codegen(&stream);
    codegen.write_header_comment(target_dir);

    DynamicArray<std::string> included_target_deps;

    if (!target_dependencies.empty())
    {
        for (const std::string& dep : target_dependencies)
        {
            const auto typelist = target_dir.parent(temp_allocator()).join(dep.c_str()).join("TypeList.generated.hpp");
            if (!fs::is_file(typelist))
            {
                continue;
            }

            included_target_deps.push_back(dep);
            codegen.write_line("#include \"%s\"", typelist.relative_to(target_dir).make_generic().c_str());
        }
        codegen.newline();
    }

    for (const Type* type : all_types)
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

        if (type.index < all_types.size() - 1 || !included_target_deps.empty())
        {
            codegen.append_line(" \\");
        }

        codegen.newline();
    }

    if (!included_target_deps.empty())
    {
        for (const auto& dep : enumerate(included_target_deps))
        {
            const auto dep_as_ident = get_target_name_as_ident({ dep.value.c_str(), static_cast<i32>(dep.value.size()) });
            codegen.write_line("    %s_TYPE_REGISTRATION", dep_as_ident.c_str());
            if (dep.index < included_target_deps.size() - 1)
            {
                codegen.append_line(" \\");
            }
            codegen.newline();
        }
    }
    codegen.newline();

    fs::write(output_path, output.view());
}

void read_typelist(const Path& root, std::map<Path, String>* results)
{
    for (const auto& path : fs::read_dir(root))
    {
        if (fs::is_dir(path))
        {
            read_typelist(path, results);
            continue;
        }

        if (fs::is_file(path) && path.filename() == "TypeList.generated.hpp")
        {
            const auto file_contents = fs::read(path);
            auto macro_begin = str::first_index_of(file_contents, "_TYPE_REGISTRATION");
            if (macro_begin < 0)
            {
                log_error("bee-reflect: unable to read TypeList - couldn't find valid TYPE_REGISTRATION macro");
                return;
            }

            int count = sizeof("_TYPE_REGISTRATION") - 1;

            while (macro_begin > 0 && !str::is_space(file_contents[macro_begin - 1]))
            {
                --macro_begin;
                ++count;
            }

            results->insert({
                path.get_generic(),
                String(str::substring(file_contents.view(), macro_begin, count))
            });
        }
    }
}

void link_typelists(const Path& output_path, const Span<const Path>& search_paths)
{
    std::map<Path, String> link_results;

    for (const auto& path : search_paths)
    {
        read_typelist(path, &link_results);
    }

    String output;
    io::StringStream stream(&output);
    CodeGenerator codegen(&stream);
    codegen.write_header_comment("bee-reflect linker");

    for (const auto& include_path : link_results)
    {
        codegen.write_line("#include \"%s\"", include_path.first.c_str());
    }

    codegen.newline();
    codegen.write_line("#include <Bee/Core/Reflection.hpp>");
    codegen.newline();
    codegen.newline();
    codegen.write_line("#define BEE_REGISTER_TYPE(X) bee::register_type(bee::get_type<X>());");
    codegen.newline();
    codegen.newline();
    codegen.write_line("namespace bee {");
    codegen.newline();
    codegen.newline();
    codegen.write_line(R"(/*
 * MUST be called from an executables `main()` to register all types so that
 * the version of `get_type()` that uses type hashes instead of template types
 * works correctly.
 */)");
    codegen.write("void reflection_init()");
    codegen.scope([&]()
    {
        codegen.write("reflection_register_builtin_types();");

        if (!link_results.empty())
        {
            codegen.newline();
            for (const auto& result : link_results)
            {
                codegen.write_line("%s", result.second.c_str());
            }
        }
    }, " // void reflection_init()\n");
    codegen.newline();
    codegen.newline();
    codegen.write("} // namespace bee");

    fs::write(output_path, output.view());
}


} // namespace reflect
} // namespace bee