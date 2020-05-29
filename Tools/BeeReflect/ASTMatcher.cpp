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

/*
 *************************************
 *
 * ASTMatcher - implementation
 *
 *************************************
 */
ASTMatcher::ASTMatcher(TypeMap* type_map_to_use, ReflectionAllocator* allocator_ptr)
    : type_map(type_map_to_use),
      allocator(allocator_ptr)
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

std::string ASTMatcher::print_qualtype_name(const clang::QualType& type, const clang::ASTContext& ast_context)
{
    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    return clang::TypeName::getFullyQualifiedName(type, ast_context, ast_context.getPrintingPolicy());
}


void ASTMatcher::reflect_record(const clang::CXXRecordDecl& decl, RecordTypeStorage* parent)
{
    if (!decl.isThisDeclarationADefinition() || decl.isInvalidDecl())
    {
        return;
    }

    AttributeParser attr_parser{};

    if (!attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    if (decl.isAnonymousStructOrUnion())
    {
        reflect_record_children(decl, parent);
        return;
    }

    const auto name = str::format("%" BEE_PRIsv, BEE_FMT_SV(print_name(decl)));
    auto* storage = allocator->allocate_storage<RecordTypeStorage>(&decl);
    auto* type = &storage->type;

    // Get all the base class names
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

        const auto* base_name = allocator->allocate_name(print_qualtype_name(base.getType(), decl.getASTContext()));
        storage->base_type_names.push_back(base_name);
    }

    if (!decl.isDependentType())
    {
        const auto& layout = decl.getASTContext().getASTRecordLayout(&decl);
        type->size = layout.getSize().getQuantity();
        type->alignment = layout.getAlignment().getQuantity();
    }

    if (decl.isStruct())
    {
        type->kind = TypeKind::struct_decl;
    }
    else if (decl.isUnion())
    {
        type->kind = TypeKind::union_decl;
    }
    else if (decl.isClass())
    {
        type->kind = TypeKind::class_decl;
    }
    else if (decl.isEnum())
    {
        type->kind = TypeKind::enum_decl;
    }
    else
    {
        diagnostics.Report(decl.getLocation(), clang::diag::err_type_unsupported);
        return;
    }

    // Name will be valid here even for templated classes because it doesn't contain the template parameters.
    // `name` will get overwritten below if it's a template type so don't move this line of code
    type->hash = get_type_hash(name.view());

    // Gather template parameters
    auto* class_template = decl.getDescribedClassTemplate();
    if (class_template != nullptr)
    {
        type->kind |= TypeKind::template_decl;
        type->serialization_flags |= SerializationFlags::uses_builder;

        String template_name(temp_allocator());
        io::StringStream stream(&template_name);
        stream.write_fmt("%s<", name.c_str());

        int param_index = 0;
        const auto param_count = static_cast<int>(class_template->getTemplateParameters()->size());
        for (auto* clang_param : *class_template->getTemplateParameters())
        {
            TemplateParameter param{};
            param.name = allocator->allocate_name(clang_param->getName());
            param.type_name = param.name;
            param.hash = get_type_hash(param.name);

            // Default template args need to be removed so we can specialize `get_type` properly
            if (auto* ttp = llvm::dyn_cast<clang::TemplateTypeParmDecl>(clang_param))
            {
                ttp->removeDefaultArgument();
            }
            else if (auto* nttp = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(clang_param))
            {
                param.type_name = allocator->allocate_name(print_qualtype_name(nttp->getType(), decl.getASTContext()));
                nttp->removeDefaultArgument();
            }

            storage->add_template_parameter(param);

            stream.write_fmt("%s", param.name);
            if (param_index < param_count - 1)
            {
                stream.write(", ");
            }
            ++param_index;
        }

        stream.write(">");

        type_name.clear();
        llvm::raw_svector_ostream type_name_stream(type_name);
        class_template->getTemplateParameters()->print(type_name_stream, decl.getASTContext(), decl.getASTContext().getPrintingPolicy());
        storage->template_decl_string = allocator->allocate_name(type_name);
        type->name = allocator->allocate_name(template_name.c_str());
    }
    else
    {
        type->name = allocator->allocate_name(llvm::StringRef(name.c_str()));
    }


    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&storage->attributes, &serialization_info, allocator))
    {
        return;
    }

    type->serialization_flags |= serialization_info.flags;
    type->serialized_version = serialization_info.serialized_version;
    storage->has_explicit_version = serialization_info.using_explicit_versioning;

    if (parent == nullptr)
    {
        type_map->add_record(storage, decl);
    }
    else
    {
        parent->add_record(storage);
    }

    reflect_record_children(decl, storage);
}

void ASTMatcher::reflect_record_children(const clang::CXXRecordDecl &decl, RecordTypeStorage *storage)
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
                    reflect_record(*child_record, storage);
                }
                break;
            }
            case clang::Decl::Kind::Enum:
            {
                const auto* child_enum = llvm::dyn_cast<clang::EnumDecl>(child);
                if (child_enum != nullptr)
                {
                    reflect_enum(*child_enum, storage);
                }
                break;
            }
            case clang::Decl::Kind::Field:
            {
                const auto old_field_count = storage->fields.size();
                const auto* child_field = llvm::dyn_cast<clang::FieldDecl>(child);

                if (child_field != nullptr)
                {
                    reflect_field(*child_field, decl.getASTContext().getASTRecordLayout(&decl), storage);
                }

                if (storage->fields.size() > old_field_count && !requires_field_order_validation)
                {
                    requires_field_order_validation = storage->fields.back().order >= 0;
                }
                break;
            }
            case clang::Decl::Kind::Function:
            case clang::Decl::Kind::CXXMethod:
            {
                const auto* child_method = llvm::dyn_cast<clang::FunctionDecl>(child);
                if (child_method != nullptr)
                {
                    reflect_function(*child_method, storage);
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
    std::sort(storage->fields.begin(), storage->fields.end());

    for (int f = 0; f < storage->fields.size(); ++f)
    {
        auto& field = storage->fields[f];
        // Ensure each field has the `id` attribute
        if (field.order < 0)
        {
            diagnostics.Report(field.location, diagnostics.err_requires_explicit_ordering);
            return;
        }

        if (f >= 1 && field.order == storage->fields[f - 1].order)
        {
            diagnostics.Report(field.location, diagnostics.err_id_is_not_unique);
            return;
        }
    }
}


void ASTMatcher::reflect_enum(const clang::EnumDecl& decl, RecordTypeStorage* parent)
{
    auto& ast_context = decl.getASTContext();
    AttributeParser attr_parser{};

    if (!attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    const auto underlying = decl.getIntegerType().getCanonicalType();
    // Get the associated types hash so we can look it up later
    const auto underlying_name = print_qualtype_name(underlying, ast_context);
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
    auto* storage = allocator->allocate_storage<EnumTypeStorage>();
    auto* type = &storage->type;
    type->kind = TypeKind::enum_decl;
    type->size = ast_context.getTypeSize(underlying) / 8;
    type->alignment = ast_context.getTypeAlign(underlying) / 8;
    type->name = allocator->allocate_name(name);
    type->hash = get_type_hash({ name.data(), static_cast<i32>(name.size()) });
    type->is_scoped = decl.isScoped();

    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&storage->attributes, &serialization_info, allocator))
    {
        return;
    }

    type->serialization_flags = serialization_info.flags;
    type->serialized_version = serialization_info.serialized_version;

    const auto flags_attr_index = find_index_if(storage->attributes, [&](const Attribute& attr)
    {
        return str::compare(attr.name, "flags") == 0 && attr.kind == AttributeKind::boolean;
    });

    type->is_flags = flags_attr_index >= 0;
    if (type->is_flags)
    {
        // remove it as if it was a builtin attribute for enums only
        storage->attributes.erase(flags_attr_index);
    }

    for (const clang::EnumConstantDecl* ast_constant : decl.enumerators())
    {
        EnumConstant constant{};
        constant.name = allocator->allocate_name(ast_constant->getName());
        constant.value = ast_constant->getInitVal().getRawData()[0];
        constant.underlying_type = underlying_type;
        storage->add_constant(constant);
    }

    if (parent == nullptr)
    {
        type_map->add_enum(storage, decl);
    }
    else
    {
        parent->add_enum(storage);
    }
}

void ASTMatcher::reflect_field(const clang::FieldDecl& decl, const clang::ASTRecordLayout& enclosing_layout, RecordTypeStorage* parent)
{
    if (decl.isAnonymousStructOrUnion())
    {
        return;
    }

    AttributeParser attr_parser{};

    const auto requires_annotation = parent == nullptr;

    if (!attr_parser.init(decl, &diagnostics) && requires_annotation)
    {
        return;
    }

    auto qualtype = decl.getType().getCanonicalType();
    if (qualtype->isConstantArrayType())
    {
        const auto array_type_name = print_qualtype_name(qualtype, decl.getASTContext());
        const auto hash = get_type_hash({ array_type_name.data(), static_cast<i32>(array_type_name.size()) });
        if (type_map->find_type(hash) == nullptr)
        {
            const auto* clang_type = llvm::dyn_cast<clang::ConstantArrayType>(qualtype);
            auto* array_storage = allocator->allocate_storage<ArrayTypeStorage>();
            const auto element_type = clang_type->getElementType().getCanonicalType();

            auto& new_array_type = array_storage->type;
            new_array_type.hash = hash;
            new_array_type.name = allocator->allocate_name(array_type_name);
            new_array_type.kind = TypeKind::array;
            new_array_type.element_count = sign_cast<i32>(*clang_type->getSize().getRawData());
            new_array_type.size = 0; // functions only have size when used as function pointer
            new_array_type.alignment = 0;
            new_array_type.serialized_version = 1;

            const auto element_type_name = print_qualtype_name(element_type, decl.getASTContext());
            array_storage->element_type_name = allocator->allocate_name(element_type_name);

            const auto element_type_hash = get_type_hash({ element_type_name.data(), static_cast<i32>(element_type_name.size()) });
            new_array_type.element_type = TypeRef(type_map->find_type(element_type_hash));

            if (new_array_type.element_type.is_unknown())
            {
                new_array_type.element_type = get_type(element_type_hash);
            }

            if (!new_array_type.element_type.is_unknown())
            {
                new_array_type.size = decl.getASTContext().getTypeSize(element_type) * new_array_type.element_count;
                new_array_type.alignment = decl.getASTContext().getTypeAlign(element_type);
            }
            else
            {
                diagnostics.Report(decl.getLocation(), diagnostics.warn_unknown_field_type).AddString(element_type_name);
            }

            if (parent == nullptr)
            {
                type_map->add_array(array_storage, decl);
            }
            else
            {
                parent->add_array_type(array_storage);
            }
        }
    }

    // We need to parse the attributes before allocating storage to ensure ignoredd fields aren't reflected
    DynamicArray<Attribute> tmp_attributes(temp_allocator());
    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&tmp_attributes, &serialization_info, allocator))
    {
        return;
    }

    auto storage = create_field(decl.getName(), decl.getFieldIndex(), decl.getASTContext(), &enclosing_layout, parent, decl.getType(), decl.getTypeSpecStartLoc());
    storage.attributes = std::move(tmp_attributes);

    auto& field = storage.field;

    if (field.type.is_unknown())
    {
        return;
    }

    if (decl.isTemplateParameter())
    {
        const auto template_param_name = print_qualtype_name(decl.getType(), decl.getASTContext());
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
    if (parent->type.serialized_version > 0 && field.version_removed < limits::max<i32>() && field.version_added <= 0)
    {
        diagnostics.Report(decl.getLocation(), diagnostics.err_missing_version_added);
        return;
    }

    // Validate versioning if the parent record type is explicitly versioned
    if (parent->has_explicit_version)
    {
        if (field.version_added > 0 && parent->type.serialized_version <= 0)
        {
            diagnostics.Report(decl.getLocation(), diagnostics.err_parent_not_marked_for_serialization);
            return;
        }

        if (field.version_added > 0 && field.type->serialized_version <= 0)
        {
            diagnostics.Report(decl.getLocation(), diagnostics.err_field_not_marked_for_serialization);
            return;
        }
    }

    parent->add_field(storage);
}


void ASTMatcher::reflect_function(const clang::FunctionDecl& decl, RecordTypeStorage* parent)
{
    AttributeParser attr_parser{};

    const auto is_member_function = parent != nullptr && decl.isCXXClassMember();
    if (!attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    decl.printQualifiedName(type_name_stream);

    auto* storage = allocator->allocate_storage<FunctionTypeStorage>();
    auto* type = &storage->type;
    type->hash = get_type_hash({ type_name.data(), static_cast<i32>(type_name.size()) });
    type->name = allocator->allocate_name(type_name);
    type->size = sizeof(void*); // functions only have size when used as function pointer
    type->alignment = alignof(void*);
    type->kind = TypeKind::function;

    if (is_member_function)
    {
        type->kind |= TypeKind::method;
    }

    storage->return_field = create_field(decl.getName(), -1, decl.getASTContext(), nullptr, parent, decl.getReturnType(), decl.getTypeSpecStartLoc());
    storage->add_invoker_type_arg(print_qualtype_name(decl.getReturnType(), decl.getASTContext()));

    // If this is a method type then we need to skip the implicit `this` parameter
    const auto* params_begin = decl.parameters().begin();
    const auto* params_end = decl.parameters().end();
    if (is_member_function && !decl.parameters().empty())
    {
        ++params_begin;
    }

    for (const auto& param : clang::ArrayRef<clang::ParmVarDecl*>(params_begin, params_end))
    {
        auto param_storage = create_field(param->getName(), param->getFunctionScopeIndex(), param->getASTContext(), nullptr, parent, param->getType(), param->getLocation());
        auto& field = param_storage.field;
        field.offset = param->getFunctionScopeIndex();
        field.storage_class = get_storage_class(param->getStorageClass(), param->getStorageDuration());
        storage->add_parameter(param_storage);
        storage->add_invoker_type_arg(print_qualtype_name(param->getType(), decl.getASTContext()));
    }

    type->storage_class = get_storage_class(decl.getStorageClass(), static_cast<clang::StorageDuration>(0));
    type->is_constexpr = decl.isConstexpr();

    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&storage->attributes, &serialization_info, allocator))
    {
        return;
    }

    type->serialization_flags = serialization_info.flags;
    type->serialized_version = serialization_info.serialized_version;

    if (is_member_function)
    {
        if (parent == nullptr)
        {
            diagnostics.Report(decl.getLocation(), clang::diag::err_incomplete_type);
            return;
        }

        parent->add_function(storage);
    }
    else
    {
        type_map->add_function(storage, decl);
    }
}

FieldStorage ASTMatcher::create_field(const llvm::StringRef& name, const bee::i32 index, const clang::ASTContext& ast_context, const clang::ASTRecordLayout* enclosing_layout, const struct bee::reflect::RecordTypeStorage* parent, const clang::QualType& qual_type, const clang::SourceLocation& location)
{
    /*
    * Get the layout of the parent record this field is in and also get pointers to the desugared type so that
    * i.e. u8 becomes unsigned char
    */
    auto desugared_type = qual_type.getCanonicalType();

    FieldStorage storage{};
    auto& field = storage.field;
    field.name = allocator->allocate_name(name);
    field.offset = 0;
    field.qualifier = get_qualifier(desugared_type);

    if (enclosing_layout != nullptr)
    {
        field.offset = enclosing_layout->getFieldOffset(static_cast<u32>(index)) / 8;
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
    auto fully_qualified_name = print_qualtype_name(original_type, ast_context);
    auto type_hash = get_type_hash({ fully_qualified_name.data(), static_cast<i32>(fully_qualified_name.size()) });
    storage.specialized_type = allocator->allocate_name(fully_qualified_name);

    if (original_type->isRecordType())
    {
        auto* as_cxx_record_decl = !is_ptr_or_ref ? qual_type->getAsCXXRecordDecl() : type_ptr->getPointeeType()->getAsCXXRecordDecl();
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
                    storage.template_arguments.push_back(get_type<UnknownType>());
                    continue;
                }

                const auto arg_qualtype = is_type ? arg.getAsType() : arg.getIntegralType();
                const auto templ_type_name = print_qualtype_name(arg_qualtype, specialization->getASTContext());
                const auto arg_type_hash = get_type_hash(templ_type_name.c_str());
                auto arg_type = TypeRef(type_map->find_type(arg_type_hash));

                if (arg_type.is_unknown())
                {
                    arg_type = get_type(arg_type_hash);
                }

                if (arg_type->is(TypeKind::unknown))
                {
                    diagnostics.Report(location, diagnostics.warn_unknown_field_type).AddString(templ_type_name);
                }

                storage.template_arguments.push_back(arg_type);
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

    auto type = TypeRef(type_map->find_type(type_hash));
    if (type.is_unknown())
    { 
        /*
         * If the type is missing it might be a core builtin type which can be accessed by bee-reflect via
         * get_type. This is safe to do here as bee-reflect doesn't link to any generated cpp files
         */
        type = get_type(type_hash);
        if (type->kind == TypeKind::unknown && !original_type->isTemplateTypeParmType())
        {
            diagnostics.Report(location, diagnostics.warn_unknown_field_type).AddString(storage.specialized_type);
        }
    }

    field.hash = get_type_hash(field.name);
    field.type = type;
    return storage;
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

const char* AttributeParser::parse_name()
{
    const char* begin = current;

    while (current != end)
    {
        if (str::is_space(*current) || *current == '=' || *current == ',' || *current == ']')
        {
            return allocator->allocate_name(llvm::StringRef(begin, current - begin));
        }

        next();
    }

    diagnostics->Report(location, diagnostics->err_invalid_attribute_name_format).AddString(llvm::StringRef(begin, current - begin));
    return nullptr;
}

bool AttributeParser::parse_string(Attribute* attribute)
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

    attribute->kind = AttributeKind::string;
    attribute->value.string = allocator->allocate_name(llvm::StringRef(begin, current - 1 - begin));
    return true;
}

bool AttributeParser::parse_number(bee::Attribute* attribute)
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

    if (!number_str.getAsInteger(10, attribute->value.integer))
    {
        attribute->kind = AttributeKind::integer;
        return true;
    }

    number_str = number_str.rtrim('f');
    double result = 0.0;
    if (!number_str.getAsDouble(result))
    {
        attribute->kind = AttributeKind::floating_point;
        attribute->value.floating_point = static_cast<float>(result);
        return true;
    }

    return false;
}

bool is_symbol_char(const char c)
{
    return isalnum(c) || c == '_' || c == ':' || c == '(' || c == ')' || c == '<' || c == '>';
}

bool AttributeParser::parse_symbol(bee::Attribute* attribute)
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
        attribute->kind = AttributeKind::boolean;
        attribute->value.boolean = is_true;
    }
    else
    {
        attribute->kind = AttributeKind::type;
        attribute->value.string = allocator->allocate_name(str);
    }

    return true;
}

bool AttributeParser::parse_value(Attribute* attribute)
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

bool AttributeParser::parse_attribute(DynamicArray<Attribute>* dst_attributes, SerializationInfo* serialization_info)
{
    skip_whitespace();

    Attribute attribute{};
    attribute.name = parse_name();

    if (attribute.name == nullptr)
    {
        return false;
    }

    skip_whitespace();

    attribute.hash = get_type_hash(attribute.name);

    if (*current == ',' || *current == ']')
    {
        attribute.kind = AttributeKind::boolean;
        attribute.value.boolean = true;
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
        return builtin.hash == attribute.hash;
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
            serialization_info->serialized_version = attribute.value.integer;
            serialization_info->using_explicit_versioning = true;
            break;
        }
        case BuiltinAttributeKind::version_added:
        {
            serialization_info->version_added = attribute.value.integer;
            break;
        }
        case BuiltinAttributeKind::version_removed:
        {
            serialization_info->version_removed = attribute.value.integer;
            break;
        }
        case BuiltinAttributeKind::id:
        {
            serialization_info->id = attribute.value.integer;
            break;
        }
        case BuiltinAttributeKind::format:
        {
            if (attribute.kind != AttributeKind::type)
            {
                return false;
            }

            if (str::compare(attribute.value.string, "packed") == 0)
            {
                serialization_info->flags |= SerializationFlags::packed_format;
                break;
            }

            if (str::compare(attribute.value.string, "table") == 0)
            {
                serialization_info->flags |= SerializationFlags::table_format;
                break;
            }

            return false; // unknown format
        }
        case BuiltinAttributeKind::serializer_function:
        {
            if (attribute.kind != AttributeKind::type)
            {
                return false;
            }

            serialization_info->flags |= SerializationFlags::uses_builder;
            serialization_info->serializer_function = allocator->allocate_name(attribute.value.string);
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

bool AttributeParser::parse(DynamicArray<Attribute>* dst_attributes, SerializationInfo* serialization_info, ReflectionAllocator* refl_allocator)
{
    serialization_info->flags = SerializationFlags::none;

    if (is_field)
    {
        serialization_info->serializable = true;
    }

    if (!empty && current != nullptr)
    {
        allocator = refl_allocator;

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


bool AttributeParser::init(const clang::Decl& decl, Diagnostics* new_diagnostics)
{
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