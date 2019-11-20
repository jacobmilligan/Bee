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

#include <map>
#include <set>
#include <inttypes.h>


namespace bee {
namespace reflect {


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
        "%u, %zu, %zu, %s, \"%s\", %d, ",
        type->hash,
        type->size,
        type->alignment,
        reflection_type_kind_to_string(type->kind),
        type->name,
        type->serialized_version
    );
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
            default:
            {
                BEE_UNREACHABLE("Invalid attribute kind: AttributeKind::invalid");
            }
        }

        codegen->append_line(")");
    });
}


void codegen_field(const Field& field, const char* attributes_array_name, CodeGenerator* codegen)
{
    codegen->write("Field");
    codegen->scope([&]()
    {
        codegen->write(
            "%u, %zu, %s, %s, \"%s\", get_type<%s>(), %s, %d, %d",
            field.hash,
            field.offset,
            reflection_flag_to_string(field.qualifier),
            reflection_flag_to_string(field.storage_class),
            field.name,
            field.type->name,
            attributes_array_name == nullptr ? "{}" : attributes_array_name,
            field.version_added,
            field.version_removed
        );
    });
}


void codegen_function(const FunctionType* type, CodeGenerator* codegen)
{
    const auto function_name_as_ident = get_name_as_ident(type, temp_allocator());

    if (!type->attributes.empty())
    {
        codegen->write("static Attribute %s__attributes[] =", function_name_as_ident.c_str());
        codegen->scope([&]()
        {
            for (const Attribute& attr : type->attributes)
            {
                codegen_attribute(attr, codegen);
                codegen->append_line(",\n");
            }
        }, ";");
        codegen->newline();
        codegen->newline();
    }

    if (!type->parameters.empty())
    {
        codegen->write("static Field %s__parameters[] =", function_name_as_ident.c_str());
        codegen->scope([&]()
        {
            for (const Field& field : type->parameters)
            {
                codegen_field(field, nullptr, codegen);
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

        codegen_field(type->return_value, nullptr, codegen);

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
    codegen->write("template <> BEE_EXPORT_SYMBOL const Type* get_type<%s>()", type->name);
    codegen->scope([&]()
    {
        const auto name_as_ident = get_name_as_ident(type, temp_allocator());

        if (!type->attributes.empty())
        {
            codegen->write("static Attribute %s__attributes[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const Attribute& attr : type->attributes)
                {
                    codegen_attribute(attr, codegen);
                    codegen->append_line(",\n");
                }
            }, ";");
            codegen->newline();
            codegen->newline();
        }

        if (!type->fields.empty())
        {
            const auto ordered_fields = Span<OrderedField>(reinterpret_cast<OrderedField*>(type->fields.data()), type->fields.size());
            for (const Field& field : ordered_fields)
            {
                if (field.attributes.empty())
                {
                    continue;
                }

                codegen->write("static Attribute %s__%s__attributes[] =", name_as_ident.c_str(), field.name);
                codegen->scope([&]()
                {
                    for (const Attribute& attr : field.attributes)
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
                for (const Field& field : ordered_fields)
                {
                    const char* attr_array_name = field.attributes.empty()
                                                  ? nullptr
                                                  : str::format(temp_allocator(), "Span<Attribute>(%s__%s__attributes)", name_as_ident.c_str(), field.name).c_str();

                    codegen_field(field, attr_array_name, codegen);
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

        if (!type->records.empty())
        {
            codegen->write("static const RecordType* %s__records[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const RecordType* record : type->records)
                {
                    codegen->write_line("reinterpret_cast<const RecordType*>(get_type<%s>()),", record->name);
                }
            }, ";");
            codegen->write_line("// %s__records[]\n", name_as_ident.c_str());
        }

        if (!type->enums.empty())
        {
            codegen->write("static const EnumType* %s__enums[] =", name_as_ident.c_str());
            codegen->scope([&]()
            {
                for (const EnumType* enum_type : type->enums)
                {
                    codegen->write_line("reinterpret_cast<const EnumType*>(get_type<%s>()),", enum_type->name);
                }
            }, ";");
            codegen->write_line("// %s__enums[]\n", name_as_ident.c_str());
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

            codegen->append_line(", ");

            if (!type->attributes.empty())
            {
                codegen->append_line("Span<Attribute>(%s__attributes)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!type->enums.empty())
            {
                codegen->append_line("Span<const EnumType*>(%s__enums)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }

            codegen->append_line(", ");

            if (!type->records.empty())
            {
                codegen->append_line("Span<const RecordType*>(%s__records)", name_as_ident.c_str());
            }
            else
            {
                codegen->append_line("{}");
            }
        }, ";\n\n");
        codegen->write_line("return &instance;");
    });
    codegen->write_line("// get_type<%s>()\n", type->name);
}


void codegen_enum(const EnumType* type, CodeGenerator* codegen)
{
    codegen->write("template <> BEE_EXPORT_SYMBOL const Type* get_type<%s>()", type->name);
    codegen->scope([&]()
    {
        if (!type->attributes.empty())
        {
            codegen->write("static Attribute attributes[] =");
            codegen->scope([&]()
            {
                for (const Attribute& attr : type->attributes)
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
            for (const EnumConstant& constant : type->constants)
            {
                codegen->write_line("EnumConstant { \"%s\", %" PRIi64 ", get_type<%s>() },", constant.name, constant.value, constant.underlying_type->name);
            }
        }, ";");
        codegen->newline();
        codegen->newline();
        codegen->write("static EnumType instance");
        codegen->scope([&]()
        {
            codegen_type(type, codegen);
            codegen->append_line(
                "%s, Span<EnumConstant>(constants), %s",
                type->is_scoped ? "true" : "false",
                type->attributes.empty() ? "{}" : "Span<Attribute>(attributes)"
            );
        }, ";");
        codegen->newline();
        codegen->newline();
        codegen->write_line("return &instance;");
    });
    codegen->write_line("// get_type<%s>()\n", type->name);
}


void generate_reflection(const Path& source_location, const Span<const Type*>& types, io::StringStream* stream)
{
    CodeGenerator codegen(stream, 4);

    codegen.write_header_comment(source_location);
    codegen.newline();
    codegen.write_line("#include \"%s\"", source_location.to_generic_string(temp_allocator()).c_str());
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
                    const auto as_record = reinterpret_cast<const RecordType*>(type);

                    for (const EnumType* nested_enum : as_record->enums)
                    {
                        codegen_enum(nested_enum, &codegen);
                    }

                    for (const RecordType* nested_record : as_record->records)
                    {
                        codegen_record(nested_record, &codegen);
                    }

                    codegen_record(as_record, &codegen);
                    break;
                }
                case TypeKind::enum_decl:
                {
                    codegen_enum(reinterpret_cast<const EnumType*>(type), &codegen);
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


void generate_registration(const Path& source_location, const Span<const Type*>& types, io::StringStream* stream)
{
    // Generate the output .cpp file that the user will need to link in their project
    RegistrationHeader header{};
    memcpy(header.magic, bee_reflect_magic, bee_reflect_magic_size);
    header.version = RegistrationVersion::current;
    header.type_count = 0;
    header.source_location_offset = sizeof(RegistrationHeader);
    header.source_location_size = static_cast<u32>(source_location.size());
    header.types_byte_count = 0;

    FixedArray<RegistrationTypeOffset> hashes(types.size(), temp_allocator());

    u32 offset = 0;
    for (const Type* type : types)
    {
        if (type->kind == TypeKind::enum_decl && !reinterpret_cast<const EnumType*>(type)->is_scoped)
        {
            log_warning("bee-reflect: skipping dynamic reflection for unscoped `enum %s`. Consider converting to scoped `enum class` to enable dynamic reflection.", type->name);
            continue;
        }

        const auto size = str::length(type->name)
            + str::length(reflection_type_kind_to_code_string(type->kind))
            + sizeof("BEE_REGISTER_TYPE(,)");

        hashes.push_back({ type->hash, offset });
        ++header.type_count;
        header.types_byte_count += size;
        offset += size;
    }

    const auto hashes_byte_size = sizeof(RegistrationTypeOffset) * header.type_count;
    header.hashes_offset = header.source_location_offset + header.source_location_size + sizeof(RegistrationHeader);
    header.types_offset = header.hashes_offset + header.type_count;

    // Write out the registration file
    stream->write(&header, sizeof(RegistrationHeader)); // header
    stream->write(source_location.view());              // source location
    stream->write(hashes.data(), hashes_byte_size);     // hashes

    // Write out all the type macros
    for (const auto type : enumerate(types))
    {
        if (type.value->kind == TypeKind::enum_decl && !reinterpret_cast<const EnumType*>(type.value)->is_scoped)
        {
            continue;
        }

        stream->write_fmt("BEE_REGISTER_TYPE(%s,%s)", reflection_type_kind_to_code_string(type.value->kind), type.value->name);
        stream->write('\0');
    }

    const auto eof = EOF;
    stream->write(&eof, sizeof(int));
}


struct LinkResult
{
    String                          macro;                  // the formatted type macro, i.e. "BEE_REGISTER_TYPE(TypeKind::record, bee::TestStruct)"
    StringView                      kind;                   // the TypeKind part of the macro string
    StringView                      fully_qualified_name;   // the fully qualified name part of the macro string
    StringView                      unqualified_name;       // `fully_qualified_name` minus any namespaces
    RegistrationVersion             version;                // the version of bee-reflect used to generate the registration

    LinkResult() = default;

    LinkResult(const char* type_macro, const RegistrationVersion version, Allocator* allocator)
        : macro(type_macro, allocator),
          version(version)
    {
        constexpr auto kind_begin = sizeof("BEE_REGISTER_TYPE(") - 1;

        const auto first_comma = str::first_index_of(macro, ',');
        const auto last_paren = str::last_index_of(macro, ')');

        kind = str::substring(macro, kind_begin, first_comma - kind_begin);
        fully_qualified_name = str::substring(macro, first_comma + 1, last_paren - (first_comma + 1));
        unqualified_name = get_unqualified_name(fully_qualified_name);
    }

    NamespaceRangeFromNameAdapter namespaces() const
    {
        return get_namespaces_from_name(fully_qualified_name);
    }
};


void read_registration_file(const Path& path, std::map<u32, LinkResult>* results, std::set<Path>* include_paths)
{
    io::FileStream file(path, "rb");
    RegistrationHeader header{};
    file.read(&header, sizeof(RegistrationHeader));

    if (memcmp(header.magic, bee_reflect_magic, bee_reflect_magic_size) != 0)
    {
        log_error("bee-reflect: invalid file signature in .registration file (%s)", path.c_str());
        return;
    }

    // Read the source file
    String src_location(header.source_location_size, '\0', temp_allocator());
    file.read(src_location.data(), sizeof(char) * header.source_location_size);
    str::replace(&src_location, Path::preferred_slash, Path::generic_slash);

    if (include_paths->find(src_location.view()) == include_paths->end())
    {
        include_paths->insert(Path(src_location.view(), temp_allocator()));
    }

    // Read all the type hashes
    auto hashes = FixedArray<RegistrationTypeOffset>::with_size(header.type_count, temp_allocator());
    file.read(hashes.data(), sizeof(RegistrationTypeOffset) * header.type_count);

    // Read the type macro
    String types_string(header.types_byte_count, '\0', temp_allocator());
    file.read(types_string.data(), header.types_byte_count);

    for (const auto& hash : hashes)
    {
        const char* type_macro = types_string.data() + hash.offset;
        const auto existing = results->find(hash.hash);

        if (existing != results->end())
        {
            log_error(
                "bee-reflect: internal error: %s (0x%08X) was linked multiple times.\npreviously linked as: %" BEE_PRIsv,
                type_macro,
                hash.hash,
                BEE_FMT_SV(existing->second.fully_qualified_name)
            );
            continue;
        }

        results->insert(std::make_pair(
            hash.hash,
            LinkResult(type_macro, header.version, temp_allocator())
        ));
    }
}


void read_registrations(const Path& root, std::map<u32, LinkResult>* results, std::set<Path>* include_paths)
{
    for (const auto& path : fs::read_dir(root))
    {
        if (fs::is_dir(path))
        {
            read_registrations(path, results, include_paths);
            continue;
        }

        if (fs::is_file(path) && path.extension() == ".registration")
        {
            read_registration_file(path, results, include_paths);
        }
    }
}

void link_registrations(const Span<const Path>& search_paths, io::StringStream* stream)
{
    std::map<u32, LinkResult>   link_results;
    std::set<Path>              include_paths;

    for (const auto& path : search_paths)
    {
        read_registrations(path, &link_results, &include_paths);
    }

    CodeGenerator codegen(stream);
    codegen.write_header_comment("bee-reflect linker");

    for (const Path& include_path : include_paths)
    {
        codegen.write_line("#include \"%s\"", include_path.c_str());
    }

    codegen.write_line("#include <Bee/Core/ReflectionV2.hpp>");
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
        codegen.write("static const Type* types[] = ");
        codegen.scope([&]()
        {
            for (const auto& result : link_results)
            {
                const LinkResult& data = result.second;
                codegen.write_line("get_type<%" BEE_PRIsv ">(),", BEE_FMT_SV(data.fully_qualified_name));
            }
        }, "; // types");
        codegen.newline();
        codegen.newline();
        codegen.write_line("reflection_register_builtin_types();");
        codegen.newline();
        codegen.write("for (const Type* type : types)");
        codegen.scope([&]()
        {
            codegen.write("register_type(type);");
        });
    }, " // void reflection_init()\n");
    codegen.newline();
    codegen.newline();
    codegen.write("} // namespace bee");
}


} // namespace reflect
} // namespace bee