/*
 *  RecordFinder.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "ASTMatcher.hpp"
#include "Bee/Core/IO.hpp"

#include <clang/Lex/PreprocessorOptions.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/QualTypeNames.h>
#include <clang/Basic/DiagnosticSema.h>


namespace bee {
namespace reflect {


enum class BuiltinAttributeKind
{
    unknown,
    serializable,
    nonserialized,
    serialized_version,
    version_added,
    version_removed,
    id,
    format,
    serializer_function,
    use_builder,
    ignored
};

struct BuiltinAttribute
{
    u32                     hash { 0 };
    BuiltinAttributeKind    kind { BuiltinAttributeKind::unknown };

    BuiltinAttribute(const char* name, const BuiltinAttributeKind attr_kind) noexcept
        : hash(get_type_hash(name)),
          kind(attr_kind)
    {}
};

BuiltinAttribute g_builtin_attributes[] =
{
    { "serializable", BuiltinAttributeKind::serializable },
    { "nonserialized", BuiltinAttributeKind::nonserialized },
    { "version", BuiltinAttributeKind::serialized_version },
    { "added", BuiltinAttributeKind::version_added },
    { "removed", BuiltinAttributeKind::version_removed },
    { "id", BuiltinAttributeKind::id },
    { "format", BuiltinAttributeKind::format },
    { "serializer", BuiltinAttributeKind::serializer_function },
    { "use_builder", BuiltinAttributeKind::use_builder },
    { "ignored", BuiltinAttributeKind::ignored }
};

StringView to_sv(const llvm::StringRef& ref)
{
    return StringView { ref.data(), sign_cast<i32>(ref.size()) };
}

llvm::StringRef from_sv(const StringView& sv)
{
    return llvm::StringRef { sv.data(), static_cast<size_t>(sv.size()) };
}

Qualifier get_qualifier(const clang::QualType& type)
{
    const auto* type_ptr = type.getTypePtrOrNull();

    auto qualifier = Qualifier::none
        | get_flag_if_true(type.isConstQualified(), Qualifier::cv_const)
        | get_flag_if_true(type.isVolatileQualified(), Qualifier::cv_volatile);

    if (type_ptr != nullptr)
    {
        qualifier |= Qualifier::none
            | get_flag_if_true(type_ptr->isLValueReferenceType(), Qualifier::lvalue_ref)
            | get_flag_if_true(type_ptr->isRValueReferenceType(), Qualifier::rvalue_ref)
            | get_flag_if_true(type_ptr->isPointerType(), Qualifier::pointer);
    }

    return qualifier;
}


StorageClass get_storage_class(const clang::StorageClass cls, const clang::StorageDuration duration)
{
    StorageClass result;

    switch (cls)
    {
        case clang::SC_Extern: result = StorageClass::extern_storage; break;
        case clang::SC_Static: result = StorageClass::static_storage; break;
        case clang::SC_PrivateExtern: result = StorageClass::extern_storage; break;
        case clang::SC_Auto: result = StorageClass::auto_storage; break;
        case clang::SC_Register: result = StorageClass::register_storage; break;
        default: result = StorageClass::none; break;
    }

    switch(duration)
    {
        case clang::SD_Automatic: result |= StorageClass::auto_storage; break;
        case clang::SD_Thread: result |= StorageClass::thread_local_storage; break;
        case clang::SD_Static: result |= StorageClass::static_storage; break;
        default: break;
    }

    return result;
}

i32 get_attribute_index(const DynamicArray<Attribute>& attributes, const char* name, const AttributeKind kind)
{
    const auto type_hash = get_type_hash(name);
    return find_index_if(attributes, [&](const Attribute& attr)
    {
        return attr.hash == type_hash && attr.kind == kind;
    });
}

bool has_reflect_attribute(const clang::Decl& decl)
{
    for (const auto& attribute : decl.attrs())
    {
        if (attribute->getKind() != clang::attr::Annotate)
        {
            continue;
        }

        auto* annotation_decl = llvm::dyn_cast<clang::AnnotateAttr>(attribute);
        if (annotation_decl != nullptr && annotation_decl->getAnnotation().startswith("bee-reflect"))
        {
            return true;
        }
    }

    return false;
}

void Diagnostics::init(clang::DiagnosticsEngine* diag_engine)
{
    engine = diag_engine;
    engine->setSuppressSystemWarnings(true);

    // Errors
    err_attribute_missing_equals = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "invalid attribute format - missing '='"
    );
    err_invalid_annotation_format = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error, "invalid reflection annotation `%0` - expected `bee-reflect`");
    err_missing_version_added = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "invalid serialized version range: you must provide both `version_added` and `version_removed` attributes"
    );
    err_parent_not_marked_for_serialization = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "cannot serialize field: parent record is not marked for explicit versioned serialization using the "
        "`version = <version>` attribute"
    );
    err_field_not_marked_for_serialization = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "cannot serialize field: missing the `added = <serialized_version>` attribute. If the parent record of a field "
        "is marked for explicit versioned serialization all fields must contain the `added` attribute"
    );
    err_invalid_attribute_name_format = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "attribute name `%0` is not a valid identifier"
    );
    err_requires_explicit_ordering = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "field is missing the `id` attribute. If one field in a class, struct or union has the `id` attribute "
        "then all other fields are required to also have an `id` attribute where each `id` is a unique integer id."
    );
    err_id_is_not_unique = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "`id` attribute on field is not unique - all fields that have the `id` attribute must be unique and greater "
        "than zero"
    );

    // Warnings
    warn_unknown_field_type = engine->getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "non-reflected or incomplete field type: %0"
    );
}

clang::DiagnosticBuilder Diagnostics::Report(clang::SourceLocation location, unsigned diag_id) const
{
    return engine->Report(location, diag_id);
}

template <typename T>
static void copy_fields(TypeBufferWriter<T>* writer, Field* dst_fields, const FieldStorage* src_fields, const i32 count)
{
    // Copy all the fields across into the type storage buffer
    for (int i = 0; i < count; ++i)
    {
        ::bee::copy(&dst_fields[i], &src_fields[i].value, 1);
        writer->write_external_string(&dst_fields[i], &Field::name, src_fields[i].name.view());
        writer->write_external_attributes(&dst_fields[i], &Field::attributes, src_fields[i].attributes.const_span());
    }
}

/*
 *************************************
 *
 * ASTMatcher - implementation
 *
 *************************************
 */
ASTMatcher::ASTMatcher(TypeMap* type_map_to_use)
    : type_map(type_map_to_use)
{}


void ASTMatcher::run(const clang::ast_matchers::MatchFinder::MatchResult& result)
{
    const auto* as_record = result.Nodes.getNodeAs<clang::CXXRecordDecl>("id");
    if (as_record != nullptr)
    {
        reflect_record(*as_record);
        return;
    }

    const auto* as_enum = result.Nodes.getNodeAs<clang::EnumDecl>("id");
    if (as_enum != nullptr)
    {
        reflect_enum(*as_enum);
        return;
    }

    const auto* as_function = result.Nodes.getNodeAs<clang::FunctionDecl>("id");
    if (as_function != nullptr)
    {
        reflect_function(*as_function);
    }
}

llvm::StringRef ASTMatcher::print_name(const clang::NamedDecl& decl)
{
    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    decl.printQualifiedName(type_name_stream);
    return type_name;
}

void ASTMatcher::print_qualtype_name(String* dst, const clang::QualType& type, const clang::ASTContext& ast_context)
{
    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    const auto std_string = clang::TypeName::getFullyQualifiedName(type, ast_context, ast_context.getPrintingPolicy());
    dst->assign(std_string.c_str());
}

void ASTMatcher::reflect_record(const clang::CXXRecordDecl& decl, ParentTypeContainer* parent)
{

    if (!decl.isThisDeclarationADefinition() || decl.isInvalidDecl())
    {
        return;
    }

    TempAllocScope temp_alloc;
    AttributeParser attr_parser{};

    if (!attr_parser.init(decl, &diagnostics, temp_alloc))
    {
        return;
    }

    if (decl.isAnonymousStructOrUnion())
    {
        reflect_record_children(decl, parent);
        return;
    }

    const auto name = str::format(temp_alloc, "%" BEE_PRIsv, BEE_FMT_SV(print_name(decl)));
    auto* type_buffer = make_type_buffer<RecordTypeInfo>(type_map);
    const u32 type_hash = get_type_hash(name.view());
    TypeBufferWriter<RecordTypeInfo> writer(type_buffer);

    // Get all the base class names
    DynamicArray<ReflTypeRef> base_hashes(temp_alloc);
    for (const auto& base : decl.bases())
    {
        if (base.isVirtual())
        {
            continue;
        }

        const auto* base_type_ptr = base.getType().getTypePtrOrNull();
        if (base_type_ptr == nullptr || !has_reflect_attribute(*base_type_ptr->getAsCXXRecordDecl()))
        {
            continue;
        }

        String base_name(temp_alloc);
        print_qualtype_name(&base_name, base.getType(), decl.getASTContext());
        base_hashes.emplace_back(get_type_hash(base_name.view()));
    }

    ::bee::copy(writer.write_array(&RecordTypeInfo::base_records, base_hashes.size()), base_hashes.data(), base_hashes.size());

    if (!decl.isDependentType())
    {
        const auto& layout = decl.getASTContext().getASTRecordLayout(&decl);
        writer.write(&RecordTypeInfo::size, sign_cast<size_t>(layout.getSize().getQuantity()));
        writer.write(&RecordTypeInfo::alignment, sign_cast<size_t>(layout.getAlignment().getQuantity()));
    }

    TypeKind kind = TypeKind::unknown;
    if (decl.isStruct())
    {
        kind |= TypeKind::struct_decl;
    }
    else if (decl.isUnion())
    {
        kind |= TypeKind::union_decl;
    }
    else if (decl.isClass())
    {
        kind |= TypeKind::class_decl;
    }
    else if (decl.isEnum())
    {
        kind |= TypeKind::enum_decl;
    }
    else
    {
        diagnostics.Report(decl.getLocation(), clang::diag::err_type_unsupported);
        return;
    }

    // Name will be valid here even for templated classes because it doesn't contain the template parameters.
    // `name` will get overwritten below if it's a template type so don't move this line of code
    writer.write(&RecordTypeInfo::hash, type_hash);

    Span<const TemplateParameter> template_parameters; // this is used later for nested types if valid

    // Gather template parameters
    SerializationFlags serialization_flags = SerializationFlags::none;
    auto* class_template = decl.getDescribedClassTemplate();
    if (class_template == nullptr)
    {
        writer.write_string(&RecordTypeInfo::name, name.view());
    }
    else
    {
        kind |= TypeKind::template_decl;
        serialization_flags |= SerializationFlags::uses_builder;

        String template_name(temp_allocator());
        io::StringStream stream(&template_name);
        stream.write_fmt("%s<", name.c_str());

        int param_index = 0;
        const auto param_count = static_cast<int>(class_template->getTemplateParameters()->size());
        auto* template_params = writer.write_array(&RecordTypeInfo::template_parameters, param_count);
        template_parameters = make_const_span(template_params, param_count);

        for (auto* clang_param : *class_template->getTemplateParameters())
        {
            auto* param = &template_params[param_index];
            param->hash = get_type_hash(param->name);

            const auto param_name = clang_param->getName();
            writer.write_external_string(param, &TemplateParameter::name, to_sv(param_name));
            copy_refl_ptr(&param->name, &param->type_name);

            // Default template args need to be removed so we can specialize `get_type` properly
            if (auto* ttp = llvm::dyn_cast<clang::TemplateTypeParmDecl>(clang_param))
            {
                ttp->removeDefaultArgument();
            }
            else if (auto* nttp = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(clang_param))
            {
                String param_type_name(temp_alloc);
                print_qualtype_name(&param_type_name, nttp->getType(), decl.getASTContext());
                writer.write_external_string(param, &TemplateParameter::type_name, param_type_name.view());
                nttp->removeDefaultArgument();
            }

            stream.write_fmt("%s", param->name.get());
            if (param_index < param_count - 1)
            {
                stream.write(", ");
            }
            ++param_index;
        }

        stream.write(">");
        writer.write_string(&RecordTypeInfo::name, template_name.view());
#if 0
        // The below is unused currently - it gets the full template decl string - may be used later
        type_name.clear();
        llvm::raw_svector_ostream type_name_stream(type_name);
        class_template->getTemplateParameters()->print(type_name_stream, decl.getASTContext(), decl.getASTContext().getPrintingPolicy());
#endif // 0
    }

    DynamicArray<AttributeStorage> attributes(temp_alloc);
    SerializationInfo serialization_info{};
    BEE_ASSERT(attr_parser.parse(&attributes, &serialization_info));

    writer.write(&RecordTypeInfo::serialization_flags, serialization_flags | serialization_info.flags);
    writer.write(&RecordTypeInfo::serialized_version, serialization_info.serialized_version);
    writer.write_attributes(&RecordTypeInfo::attributes, attributes.const_span());

    ParentTypeContainer container_for_children(&writer, temp_alloc);
    container_for_children.has_explicit_version = serialization_info.using_explicit_versioning;
    container_for_children.template_parameters = template_parameters;

    // Handle nested structs/nested enums/fields/functions etc.
    reflect_record_children(decl, &container_for_children);

    auto* fields = writer.write_array(&RecordTypeInfo::fields, container_for_children.fields.size());

    // Copy all the fields across into the type storage buffer
    copy_fields(&writer, fields, container_for_children.fields.data(), container_for_children.fields.size());

    auto fixup_nested_types = [&](const uintptr_t base_offset, const Span<const TypeFixup>& fixups)
    {
        for (auto fixup : enumerate(fixups))
        {
            type_buffer->type_fixups.push_back(fixup.value);
            type_buffer->type_fixups.back().offset_in_parent = base_offset + sizeof(ReflPtr<TypeInfo>) * fixup.index;
        }
    };

    fixup_nested_types(sizeof(RecordTypeInfo) + writer.get_offset(), container_for_children.functions.const_span());
    writer.write_array(&RecordTypeInfo::functions, container_for_children.functions.size());

    fixup_nested_types(sizeof(RecordTypeInfo) + writer.get_offset(), container_for_children.enums.const_span());
    writer.write_array(&RecordTypeInfo::enums, container_for_children.enums.size());

    fixup_nested_types(sizeof(RecordTypeInfo) + writer.get_offset(), container_for_children.records.const_span());
    writer.write_array(&RecordTypeInfo::records, container_for_children.records.size());

    // Add record to map
    type_map_add(type_map, type_buffer->type, decl);

    if (parent != nullptr)
    {
        parent->add_record(type_buffer);
    }
}

void ASTMatcher::reflect_record_children(const clang::CXXRecordDecl &decl, ParentTypeContainer* parent)
{
    bool requires_field_order_validation = false;

    for (const clang::Decl* child : decl.decls())
    {
        const auto kind = child->getKind();
        const auto is_enum_or_record = kind == clang::Decl::Kind::CXXRecord || kind == clang::Decl::Kind::Enum;

        // skip nested type decls that don't have the annotate attribute - we only automatically reflect fields
        if (is_enum_or_record && !child->hasAttr<clang::AnnotateAttr>())
        {
            continue;
        }

        // ensure that private/protected children only get reflected if they're explicitly annotated
        if (child->getAccess() != clang::AccessSpecifier::AS_public && !child->hasAttr<clang::AnnotateAttr>())
        {
            continue;
        }

        switch (kind)
        {
            case clang::Decl::Kind::CXXRecord:
            {
                const auto *const child_record = llvm::dyn_cast<clang::CXXRecordDecl>(child);
                if (child_record != nullptr)
                {
                    reflect_record(*child_record, parent);
                }
                break;
            }
            case clang::Decl::Kind::Enum:
            {
                const auto* child_enum = llvm::dyn_cast<clang::EnumDecl>(child);
                if (child_enum != nullptr)
                {
                    reflect_enum(*child_enum, parent);
                }
                break;
            }
            case clang::Decl::Kind::Field:
            {
                const auto old_field_count = parent->fields.size();
                const auto* child_field = llvm::dyn_cast<clang::FieldDecl>(child);

                if (child_field != nullptr)
                {
                    reflect_field(*child_field, decl.getASTContext().getASTRecordLayout(&decl), parent);
                }

                if (parent->fields.size() > old_field_count && !requires_field_order_validation)
                {
                    requires_field_order_validation = parent->fields.back().order >= 0;
                }
                break;
            }
            case clang::Decl::Kind::Function:
            case clang::Decl::Kind::CXXMethod:
            {
                const auto* child_method = llvm::dyn_cast<clang::FunctionDecl>(child);
                if (child_method != nullptr)
                {
                    reflect_function(*child_method, parent);
                }
                break;
            }
            default: break;
        }
    }

    // Check if a field was added and check if it declares an explicit ordering via the `id` attribute
    if (!requires_field_order_validation)
    {
        return;
    }

    // Sort and ensure id's are unique and increasing - skip the first field
    std::sort(parent->fields.begin(), parent->fields.end());

    for (int f = 0; f < parent->fields.size(); ++f)
    {
        auto& field = parent->fields[f];
        // Ensure each field has the `id` attribute
        if (field.order < 0)
        {
            diagnostics.Report(field.location, diagnostics.err_requires_explicit_ordering);
            return;
        }

        if (f >= 1 && field.order == parent->fields[f - 1].order)
        {
            diagnostics.Report(field.location, diagnostics.err_id_is_not_unique);
            return;
        }
    }
}


void ASTMatcher::reflect_enum(const clang::EnumDecl& decl, ParentTypeContainer* parent)
{
    auto& ast_context = decl.getASTContext();
    AttributeParser attr_parser{};
    TempAllocScope temp_alloc;

    if (!attr_parser.init(decl, &diagnostics, temp_alloc))
    {
        return;
    }

    const auto underlying = decl.getIntegerType().getCanonicalType();
    // Get the associated types hash so we can look it up later
    String underlying_name(temp_alloc);
    print_qualtype_name(&underlying_name, underlying, ast_context);

    const auto underlying_type = get_type(get_type_hash({
        underlying_name.c_str(),
        static_cast<i32>(underlying_name.size())
    }));

    if (underlying_type.is_unknown())
    {
        diagnostics.Report(decl.getLocation(), clang::diag::err_enum_invalid_underlying);
        return;
    }

    const auto name = print_name(decl);
    const u32 type_hash = get_type_hash({ name.data(), static_cast<i32>(name.size()) });
    auto* buffer = make_type_buffer<EnumTypeInfo>(type_map);

    TypeBufferWriter<EnumTypeInfo> writer(buffer);
    writer.write(&EnumTypeInfo::kind, TypeKind::enum_decl);
    writer.write(&EnumTypeInfo::size, sign_cast<size_t>(ast_context.getTypeSize(underlying) / 8));
    writer.write(&EnumTypeInfo::alignment, sign_cast<size_t>(ast_context.getTypeAlign(underlying) / 8));
    writer.write(&EnumTypeInfo::hash, type_hash);
    writer.write(&EnumTypeInfo::is_scoped, decl.isScoped());
    writer.write_string(&EnumTypeInfo::name, to_sv(name));

    SerializationInfo serialization_info{};
    DynamicArray<AttributeStorage> attributes(temp_alloc);
    if (!attr_parser.parse(&attributes, &serialization_info))
    {
        return;
    }

    writer.write(&EnumTypeInfo::serialization_flags, serialization_info.flags);
    writer.write(&EnumTypeInfo::serialized_version, serialization_info.serialized_version);

    const auto flags_attr_index = find_index_if(attributes, [&](const AttributeStorage& attr)
    {
        return str::compare(attr.name, "flags") == 0 && attr.data.kind == AttributeKind::boolean;
    });
    const bool is_flags = flags_attr_index >= 0;

    if (is_flags)
    {
        // remove it as if it was a builtin attribute for enums only
        attributes.erase(flags_attr_index);
    }

    writer.write_attributes(&EnumTypeInfo::attributes, attributes.const_span());
    writer.write(&EnumTypeInfo::is_flags, is_flags);

    int constants_count = 0;
    for (const clang::EnumConstantDecl* ast_constant : decl.enumerators()) { ++constants_count; }

    auto* constants = writer.write_array(&EnumTypeInfo::constants, constants_count);
    int constant_index = 0;
    for (const clang::EnumConstantDecl* ast_constant : decl.enumerators())
    {
        const auto const_name = to_sv(ast_constant->getName());
        constants[constant_index].hash = get_type_hash(const_name);
        constants[constant_index].value = ast_constant->getInitVal().getRawData()[0];
        constants[constant_index].underlying_type.hash = underlying_type->hash;
        writer.write_external_string(&constants[constant_index], &EnumConstant::name, const_name);
    }

    type_map_add(type_map, buffer->type, decl);
    if (parent != nullptr)
    {
        parent->add_enum(buffer);
    }
}

void ASTMatcher::reflect_array(const clang::FieldDecl& decl, ParentTypeContainer* parent, const clang::QualType& qualtype, AttributeParser* attr_parser)
{
    TempAllocScope temp_alloc;
    String array_type_name(temp_alloc);
    print_qualtype_name(&array_type_name, qualtype, decl.getASTContext());
    const auto hash = get_type_hash({ array_type_name.data(), static_cast<i32>(array_type_name.size()) });
    if (type_map_find(type_map, hash) != nullptr)
    {
        return;
    }

    const auto* clang_type = llvm::dyn_cast<clang::ConstantArrayType>(qualtype);
    auto* type_buffer = make_type_buffer<ArrayTypeInfo>(type_map);
    const auto element_type = clang_type->getElementType().getCanonicalType();

    TypeBufferWriter<ArrayTypeInfo> writer(type_buffer);
    writer.write(&ArrayTypeInfo::hash, hash);
    writer.write_string(&ArrayTypeInfo::name, array_type_name.view());
    writer.write(&ArrayTypeInfo::kind, TypeKind::array);
    writer.write(&ArrayTypeInfo::element_count, sign_cast<i32>(*clang_type->getSize().getRawData()));
    writer.write(&ArrayTypeInfo::size, 0llu); // functions only have size when used as function pointer
    writer.write(&ArrayTypeInfo::alignment, 0llu);
    writer.write(&ArrayTypeInfo::serialized_version, 1);

    String element_type_name(temp_alloc);
    print_qualtype_name(&element_type_name ,element_type, decl.getASTContext());

    const auto element_type_hash = get_type_hash({ element_type_name.data(), static_cast<i32>(element_type_name.size()) });
    auto mapped_element_type = Type(type_map_find(type_map, element_type_hash));
    writer.type->element_type.hash = mapped_element_type->hash;

    if (mapped_element_type->is(TypeKind::unknown))
    {
        mapped_element_type = get_type(element_type_hash);
    }

    if (!mapped_element_type->is(TypeKind::unknown))
    {
        writer.write(&ArrayTypeInfo::size, decl.getASTContext().getTypeSize(element_type) * writer.type->element_count);
        writer.write(&ArrayTypeInfo::alignment, sign_cast<size_t>(decl.getASTContext().getTypeAlign(element_type)));
    }
    else
    {
        // fallback - we need to know if the type in the array uses a builder for serialization
        // not sure if this is a great way to do this but considering we absolutely need this, it might be the only way
        if (element_type->isConstantArrayType())
        {
            reflect_array(decl, parent, element_type, attr_parser);
        }
        else if (!element_type->isRecordType() || element_type->getAsCXXRecordDecl() == nullptr)
        {
            diagnostics.Report(decl.getLocation(), diagnostics.warn_unknown_field_type).AddString(element_type_name.c_str());
        }
        else
        {
#if BEE_JACOB_TODO
            // if the type is a template we already know it needs a builder
            auto* element_type_decl = element_type->getAsCXXRecordDecl();
            array_storage->uses_builder = element_type_decl->getTemplateSpecializationKind() != clang::TSK_Undeclared;

            if (!array_storage->uses_builder)
            {
                // ugh the worst part - now we need to reparse the attributes in-place to see if use_builder is in them
                AttributeParser element_type_attr_parser{};
                if (!element_type_attr_parser.init(*element_type_decl, &diagnostics))
                {
                    return;
                }

                DynamicArray<Attribute> tmp_attributes(temp_allocator());
                SerializationInfo serialization_info{};
                if (!attr_parser->parse(&tmp_attributes, &serialization_info))
                {
                    return;
                }

                array_storage->uses_builder = (serialization_info.flags & SerializationFlags::uses_builder) != SerializationFlags::none;
            }
#endif // BEE_JACOB_TODO
        }
    }

    type_map_add(type_map, type_buffer->type, decl);
}

void ASTMatcher::reflect_field(const clang::FieldDecl& decl, const clang::ASTRecordLayout& enclosing_layout, ParentTypeContainer* parent)
{
    if (decl.isAnonymousStructOrUnion())
    {
        return;
    }

    TempAllocScope temp_alloc;
    AttributeParser attr_parser{};

    const auto requires_annotation = parent == nullptr;

    if (!attr_parser.init(decl, &diagnostics, temp_alloc) && requires_annotation)
    {
        return;
    }

    auto qualtype = decl.getType().getCanonicalType();
    if (qualtype->isConstantArrayType())
    {
        reflect_array(decl, parent, qualtype, &attr_parser);
    }

    // We need to parse the attributes before allocating storage to ensure ignored fields aren't reflected
    DynamicArray<AttributeStorage> tmp_attributes;
    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&tmp_attributes, &serialization_info))
    {
        return;
    }

    FieldCreateInfo field_info{};

    field_info.name = decl.getName();
    field_info.index = decl.getFieldIndex();
    field_info.ast_context = &decl.getASTContext();
    field_info.enclosing_layout = &enclosing_layout;
    field_info.parent = parent;
    field_info.qual_type = decl.getType();
    field_info.location = decl.getTypeSpecStartLoc();

    auto storage = create_field(field_info, temp_alloc.allocator);
    storage.attributes = std::move(tmp_attributes);

    auto& field = storage.value;

    if (storage.type == nullptr || storage.type->is(TypeKind::unknown))
    {
        return;
    }

    if (decl.isTemplateParameter())
    {
        String template_param_name(temp_alloc);
        print_qualtype_name(&template_param_name, decl.getType(), decl.getASTContext());
        const auto template_param_hash = get_type_hash(template_param_name.c_str());
        const auto param_idx = find_index_if(parent->template_parameters, [&](const TemplateParameter& param)
        {
            return param.hash == template_param_hash;
        });

        if (param_idx < 0)
        {
            diagnostics.Report(decl.getLocation(), clang::diag::err_template_param_different_kind);
            return;
        }

        field.template_argument_in_parent = param_idx;
    }

    if (decl.isMutable())
    {
        field.storage_class |= StorageClass::mutable_storage;
    }

    if (parent == nullptr)
    {
        diagnostics.Report(decl.getLocation(), clang::diag::err_incomplete_type);
        return;
    }

    field.version_added = serialization_info.version_added;
    field.version_removed = serialization_info.version_removed;
    storage.order = serialization_info.id;
    storage.location = decl.getLocation();

    /*
     * Validate serialization - ensure the field has both `version_added` and `version_removed` attributes, that its
     * parent record is marked for serialization, and that the fields type declaration is also marked for serialization
     */
    const auto* parent_type = parent->writer->type;
    if (parent_type->serialized_version > 0 && field.version_removed < limits::max<i32>() && field.version_added <= 0)
    {
        diagnostics.Report(decl.getLocation(), diagnostics.err_missing_version_added);
        return;
    }

    // Validate versioning if the parent record type is explicitly versioned
    if (parent->has_explicit_version)
    {
        if (field.version_added > 0 && parent_type->serialized_version <= 0)
        {
            diagnostics.Report(decl.getLocation(), diagnostics.err_parent_not_marked_for_serialization);
            return;
        }

        if (field.version_added > 0 && storage.type->serialized_version <= 0)
        {
            diagnostics.Report(decl.getLocation(), diagnostics.err_field_not_marked_for_serialization);
            return;
        }
    }

    parent->add_field(std::move(storage));
}


void ASTMatcher::reflect_function(const clang::FunctionDecl& decl, ParentTypeContainer* parent)
{
    TempAllocScope temp_alloc;
    AttributeParser attr_parser{};

    const auto is_member_function = parent != nullptr && decl.isCXXClassMember();
    if (!attr_parser.init(decl, &diagnostics, temp_alloc))
    {
        return;
    }

    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    decl.printQualifiedName(type_name_stream);

    auto* type_buffer = make_type_buffer<FunctionTypeInfo>(type_map);
    TypeBufferWriter<FunctionTypeInfo> writer(type_buffer);
    writer.write(&FunctionTypeInfo::hash, get_type_hash({ type_name.data(), static_cast<i32>(type_name.size()) }));
    writer.write_string(&FunctionTypeInfo::name, to_sv(type_name));
    writer.write(&FunctionTypeInfo::size, sizeof(void*)); // functions only have size when used as function pointer
    writer.write(&FunctionTypeInfo::alignment, alignof(void*));

    TypeKind type_kind = TypeKind::function;

    if (is_member_function)
    {
        type_kind |= TypeKind::method;
    }

    writer.write(&FunctionTypeInfo::kind, type_kind);

#if 0
    // TODO(Jacob): function invokers
    storage->return_field = create_field(decl.getName(), -1, decl.getASTContext(), nullptr, parent, decl.getReturnType(), decl.getTypeSpecStartLoc());
    storage->add_invoker_type_arg(print_qualtype_name(decl.getReturnType(), decl.getASTContext()));
#endif // 0

    // If this is a method type then we need to skip the implicit `this` parameter
    const auto* params_begin = decl.parameters().begin();
    const auto* params_end = decl.parameters().end();
    if (is_member_function && !decl.parameters().empty())
    {
        ++params_begin;
    }

    auto* params = writer.write_array(&FunctionTypeInfo::parameters, params_end - params_begin);

    FieldCreateInfo field_info{};
    int param_index = 0;

    for (const auto& param : clang::ArrayRef<clang::ParmVarDecl*>(params_begin, params_end))
    {
        field_info.name = param->getName();
        field_info.index = param->getFunctionScopeIndex();
        field_info.ast_context = &param->getASTContext();
        field_info.enclosing_layout = nullptr;
        field_info.parent = parent;
        field_info.qual_type = param->getType();
        field_info.location = param->getLocation();
        auto param_storage = create_field(field_info, temp_alloc.allocator);
        auto& field = param_storage.value;
        field.offset = param->getFunctionScopeIndex();
        field.storage_class = get_storage_class(param->getStorageClass(), param->getStorageDuration());
#if 0
        // TODO(Jacob): function invokers
        storage->add_invoker_type_arg(print_qualtype_name(param->getType(), decl.getASTContext()));
#endif // 0
        copy_fields(&writer, &params[param_index], &param_storage, 1);
        ++param_index;
    }

    writer.write(&FunctionTypeInfo::storage_class, get_storage_class(decl.getStorageClass(), static_cast<clang::StorageDuration>(0)));
    writer.write(&FunctionTypeInfo::is_constexpr, decl.isConstexpr());

    DynamicArray<AttributeStorage> attributes(temp_alloc);
    SerializationInfo serialization_info{};

    if (!attr_parser.parse(&attributes, &serialization_info))
    {
        return;
    }

    writer.write_attributes(&FunctionTypeInfo::attributes, attributes.const_span());
    writer.write(&FunctionTypeInfo::serialization_flags, serialization_info.flags);
    writer.write(&FunctionTypeInfo::serialized_version, serialization_info.serialized_version);

    type_map_add(type_map, type_buffer->type, decl);

    if (is_member_function)
    {
        if (parent == nullptr)
        {
            diagnostics.Report(decl.getLocation(), clang::diag::err_incomplete_type);
            return;
        }

        parent->add_function(type_buffer);
    }
}

FieldStorage ASTMatcher::create_field(const FieldCreateInfo& info, Allocator* allocator)
{
    /*
    * Get the layout of the parent record this field is in and also get pointers to the desugared type so that
    * i.e. u8 becomes unsigned char
    */
    auto desugared_type = info.qual_type.getCanonicalType();

    FieldStorage storage(allocator);
    auto& field = storage.value;
    storage.name.append(to_sv(info.name));
    field.offset = 0;
    field.qualifier = get_qualifier(desugared_type);

    if (info.enclosing_layout != nullptr)
    {
        field.offset = info.enclosing_layout->getFieldOffset(static_cast<u32>(info.index)) / 8;
    }

    clang::QualType original_type;
    const auto* type_ptr = desugared_type.getTypePtrOrNull();
    const auto is_ptr_or_ref = type_ptr != nullptr && (type_ptr->isPointerType() || type_ptr->isLValueReferenceType());

    // Check if reference or pointer and get the pointee and const-qualified info before removing qualifications
    if (is_ptr_or_ref)
    {
        const auto pointee = type_ptr->getPointeeType();

        if (pointee.isConstQualified())
        {
            field.qualifier |= Qualifier::cv_const;
        }

        original_type = pointee.getUnqualifiedType().getCanonicalType();
    }
    else
    {
        original_type = desugared_type.getUnqualifiedType().getCanonicalType();
    }

    // Get the associated types hash so we can look it up later
    print_qualtype_name(&storage.specialized_type, original_type, *info.ast_context);
    auto type_hash = get_type_hash({ storage.specialized_type.data(), static_cast<i32>(storage.specialized_type.size()) });

    if (original_type->isRecordType())
    {
        String templ_type_name(allocator);
        auto* as_cxx_record_decl = !is_ptr_or_ref ? info.qual_type->getAsCXXRecordDecl() : type_ptr->getPointeeType()->getAsCXXRecordDecl();
        if (as_cxx_record_decl != nullptr && as_cxx_record_decl->getTemplateSpecializationKind() != clang::TSK_Undeclared)
        {
            const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(as_cxx_record_decl);
            BEE_ASSERT(specialization != nullptr);

            for (const clang::TemplateArgument& arg : specialization->getTemplateArgs().asArray())
            {
                const auto is_type = arg.getKind() == clang::TemplateArgument::Type;
                const auto is_integral = arg.getKind() == clang::TemplateArgument::Integral;

                if (!is_type && !is_integral)
                {
                    storage.template_args.emplace_back(0u);
                    continue;
                }

                const auto arg_qualtype = is_type ? arg.getAsType() : arg.getIntegralType();
                print_qualtype_name(&templ_type_name, arg_qualtype, specialization->getASTContext());
                const auto arg_type_hash = get_type_hash(templ_type_name.c_str());
                auto arg_type = Type(type_map_find(type_map, arg_type_hash));

                if (arg_type.is_unknown())
                {
                    arg_type = get_type(arg_type_hash);
                }

                if (arg_type->is(TypeKind::unknown))
                {
                    diagnostics.Report(info.location, diagnostics.warn_unknown_field_type).AddString(from_sv(templ_type_name.view()));
                }

                storage.template_args.emplace_back(arg_type->hash);
            }

            original_type = original_type.getCanonicalType();

            // We want to lookup the type using the unspecialized type name from the template decl
            const auto* template_decl = specialization->getInstantiatedFrom().dyn_cast<clang::ClassTemplateDecl*>();
            if (template_decl == nullptr)
            {
                type_hash = 0;
            }
            else
            {
                const auto unspecialized_type_name = template_decl->getQualifiedNameAsString();
                type_hash = get_type_hash({ unspecialized_type_name.data(), static_cast<i32>(unspecialized_type_name.size()) });
            }
        }
    }

    auto type = Type(type_map_find(type_map, type_hash));
    if (type.is_unknown())
    { 
        /*
         * If the type is missing it might be a core builtin type which can be accessed by bee-reflect via
         * get_type. This is safe to do here as bee-reflect doesn't link to any generated cpp files
         */
        type = get_type(type_hash);
        if (type->kind == TypeKind::unknown && !original_type->isTemplateTypeParmType())
        {
            diagnostics.Report(info.location, diagnostics.warn_unknown_field_type).AddString(from_sv(storage.specialized_type.view()));
        }
    }

    field.hash = get_type_hash(field.name);
    field.type.hash = type->hash;
    storage.type = type.get();

    return std::move(storage);
}

/*
 *************************************
 *
 * Attribute parsing - implementation
 *
 *************************************
 */
void AttributeParser::next()
{
    if (current == end)
    {
        return;
    }

    ++current;
}

void AttributeParser::skip_whitespace()
{
    while (current != end && str::is_space(*current))
    {
        ++current;
    }
}

bool AttributeParser::advance_on_char(char c)
{
    if (*current == c)
    {
        next();
        return true;
    }

    return false;
}

bool AttributeParser::is_value_end() const
{
    return current == end || *current == ',' || str::is_space(*current) || *current == ']';
}

bool AttributeParser::parse_name(String* dst)
{
    const char* begin = current;

    while (current != end)
    {
        if (str::is_space(*current) || *current == '=' || *current == ',' || *current == ']')
        {
            dst->assign(StringView(begin, current - begin));
            return true;
        }

        next();
    }

    diagnostics->Report(location, diagnostics->err_invalid_attribute_name_format).AddString(llvm::StringRef(begin, current - begin));
    return false;
}

bool AttributeParser::parse_string(AttributeStorage* attribute)
{
    if (!advance_on_char('\"'))
    {
        return false;
    }

    const auto* begin = current;

    while (current != end && *current != '\"')
    {
        ++current;
    }

    if (!advance_on_char('\"'))
    {
        diagnostics->Report(location, diagnostics->err_invalid_attribute_name_format).AddString(llvm::StringRef(begin, current - begin));
        return false;
    }

    attribute->data.kind = AttributeKind::string;
    attribute->string_value.assign(StringView(begin, current - 1 - begin));
    return true;
}

bool AttributeParser::parse_number(AttributeStorage* attribute)
{
    const auto* begin = current;

    while (!is_value_end())
    {
        ++current;
    }

    llvm::StringRef number_str(begin, current - begin);

    if (number_str.empty())
    {
        diagnostics->Report(location, clang::diag::err_attribute_unsupported);
        return false;
    }

    if (!number_str.getAsInteger(10, attribute->data.value.integer))
    {
        attribute->data.kind = AttributeKind::integer;
        return true;
    }

    number_str = number_str.rtrim('f');
    double result = 0.0;
    if (!number_str.getAsDouble(result))
    {
        attribute->data.kind = AttributeKind::floating_point;
        attribute->data.value.floating_point = static_cast<float>(result);
        return true;
    }

    return false;
}

bool is_symbol_char(const char c)
{
    return isalnum(c) || c == '_' || c == ':' || c == '(' || c == ')' || c == '<' || c == '>';
}

bool AttributeParser::parse_symbol(AttributeStorage* attribute)
{
    if (!isalpha(*current) && *current != '_')
    {
        return false;
    }

    const auto* begin = current;
    auto colon_count = 0;

    while (!is_value_end())
    {
        if (!is_symbol_char(*current))
        {
            return false;
        }

        if (*current != ':')
        {
            if (colon_count > 2)
            {
                return false;
            }

            colon_count = 0;
        }
        else
        {
            ++colon_count;
        }

        ++current;
    }

    llvm::StringRef str(begin, current - begin);

    const auto is_true = str == "true";
    const auto is_false = str == "false";

    if (is_true || is_false)
    {
        attribute->data.kind = AttributeKind::boolean;
        attribute->data.value.boolean = is_true;
    }
    else
    {
        attribute->data.kind = AttributeKind::type;
        attribute->string_value.assign(to_sv(str));
    }

    return true;
}

bool AttributeParser::parse_value(AttributeStorage* attribute)
{
    // Parse as string
    if (*current == '\"')
    {
        return parse_string(attribute);
    }

    const auto is_number = *current >= '0' && *current <= '9';
    if (is_number || *current == '-' || *current == '+' || *current == '.')
    {
        return parse_number(attribute);
    }

    return parse_symbol(attribute);
}

bool AttributeParser::parse_attribute(DynamicArray<AttributeStorage>* dst_attributes, SerializationInfo* serialization_info)
{
    skip_whitespace();

    AttributeStorage attribute(allocator);
    parse_name(&attribute.name);

    if (attribute.name == nullptr)
    {
        return false;
    }

    skip_whitespace();

    attribute.data.hash = get_type_hash(attribute.name.view());

    if (*current == ',' || *current == ']')
    {
        attribute.data.kind = AttributeKind::boolean;
        attribute.data.value.boolean = true;
        if (*current != ']')
        {
            next();
        }
    }
    else
    {
        if (*current != '=')
        {
            diagnostics->Report(location, diagnostics->err_attribute_missing_equals);
            return false;
        }

        next();
        skip_whitespace();

        if (!parse_value(&attribute))
        {
            diagnostics->Report(location, clang::diag::err_type_unsupported);
            return false;
        }

        if (*current == ',')
        {
            next();
        }
    }

    const auto builtin_index = find_index_if(g_builtin_attributes, [&](const BuiltinAttribute& builtin)
    {
        return builtin.hash == attribute.data.hash;
    });

    if (builtin_index < 0)
    {
        dst_attributes->push_back(attribute);
        return true;
    }

    switch (g_builtin_attributes[builtin_index].kind)
    {
        case BuiltinAttributeKind::serializable:
        {
            serialization_info->serializable = true;
            break;
        }
        case BuiltinAttributeKind::nonserialized:
        {
            serialization_info->serializable = false;
            break;
        }
        case BuiltinAttributeKind::serialized_version:
        {
            serialization_info->serialized_version = attribute.data.value.integer;
            serialization_info->using_explicit_versioning = true;
            break;
        }
        case BuiltinAttributeKind::version_added:
        {
            serialization_info->version_added = attribute.data.value.integer;
            break;
        }
        case BuiltinAttributeKind::version_removed:
        {
            serialization_info->version_removed = attribute.data.value.integer;
            break;
        }
        case BuiltinAttributeKind::id:
        {
            serialization_info->id = attribute.data.value.integer;
            break;
        }
        case BuiltinAttributeKind::format:
        {
            if (attribute.data.kind != AttributeKind::type)
            {
                return false;
            }

            if (str::compare(attribute.string_value, "packed") == 0)
            {
                serialization_info->flags |= SerializationFlags::packed_format;
                break;
            }

            if (str::compare(attribute.string_value, "table") == 0)
            {
                serialization_info->flags |= SerializationFlags::table_format;
                break;
            }

            return false; // unknown format
        }
        case BuiltinAttributeKind::serializer_function:
        {
            if (attribute.data.kind != AttributeKind::type)
            {
                return false;
            }

            serialization_info->flags |= SerializationFlags::uses_builder;
#if 0
            TODO(Jacob): serializer functions i.e. builders
            serialization_info->serializer_function = alloc_name(attribute.string_value);
#endif // 0
            break;
        }
        case BuiltinAttributeKind::use_builder:
        {
            serialization_info->flags |= SerializationFlags::uses_builder;
            break;
        }
        case BuiltinAttributeKind::ignored:
        {
            // returning false will cause parsing to fail which will cause the type to not be reflected
            return false;
        }
        default: break;
    }

    return true;
}

bool AttributeParser::parse(DynamicArray<AttributeStorage>* dst_attributes, SerializationInfo* serialization_info)
{
    serialization_info->flags = SerializationFlags::none;

    if (is_field)
    {
        serialization_info->serializable = true;
    }

    if (!empty && current != nullptr)
    {
        const auto* begin = current;

        while (current != end && *current != ']')
        {
            if (!parse_attribute(dst_attributes, serialization_info))
            {
                return false;
            }
        }

        if (*current != ']')
        {
            diagnostics->Report(location, diagnostics->err_invalid_annotation_format).AddString(llvm::StringRef(begin, current - begin));
            return false;
        }

        // We want to keep the attributes sorted by hash so that they can be searched much faster with a binary search
        if (!dst_attributes->empty())
        {
            std::sort(dst_attributes->begin(), dst_attributes->end(), [](const Attribute& lhs, const Attribute& rhs)
            {
                return lhs.hash < rhs.hash;
            });
        }
    }

    if (!serialization_info->serializable)
    {
        serialization_info->version_added = 0;
        serialization_info->version_removed = limits::max<i32>();
        return true;
    }

    if (serialization_info->version_added <= 0)
    {
        serialization_info->version_added = 1;
    }

    if (serialization_info->serialized_version <= 0)
    {
        serialization_info->serialized_version = 1;
    }

    if (serialization_info->flags == SerializationFlags::none)
    {
        serialization_info->flags |= SerializationFlags::packed_format;
    }

    return true;
}


bool AttributeParser::init(const clang::Decl& decl, Diagnostics* new_diagnostics, Allocator* new_allocator)
{
    allocator = new_allocator;
    is_field = decl.getKind() == clang::Decl::Kind::Field;
    current = nullptr;

    llvm::StringRef annotation_str;
    diagnostics = new_diagnostics;

    for (const auto& attribute : decl.attrs())
    {
        if (attribute->getKind() != clang::attr::Annotate)
        {
            continue;
        }

        auto* annotation_decl = llvm::dyn_cast<clang::AnnotateAttr>(attribute);
        if (annotation_decl == nullptr)
        {
            continue;
        }

        annotation_str = annotation_decl->getAnnotation();
        break;
    }

    if (annotation_str.empty())
    {
        empty = true;
        return false;
    }

    if (!annotation_str.startswith("bee-reflect"))
    {
        diagnostics->Report(decl.getLocation(), diagnostics->err_invalid_annotation_format).AddString(annotation_str);
        return false;
    }

    auto attributes_str = annotation_str.split("[");

    if (attributes_str.first == annotation_str)
    {
        diagnostics->Report(decl.getLocation(), diagnostics->err_invalid_annotation_format).AddString(annotation_str);
        return false;
    }

    current = attributes_str.second.data();
    end = attributes_str.second.end();
    location = decl.getLocation();

    return true;
}


} // namespace reflect
} // namespace bee
