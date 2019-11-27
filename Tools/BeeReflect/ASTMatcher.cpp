/*
 *  RecordFinder.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "ASTMatcher.hpp"

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
    serializer_function
};

struct BuiltinAttribute
{
    u32                     hash { 0 };
    BuiltinAttributeKind    kind { BuiltinAttributeKind::unknown };

    BuiltinAttribute(const char* name, const BuiltinAttributeKind attr_kind)
        : hash(get_type_hash(name)),
          kind(attr_kind)
    {}
};

BuiltinAttribute g_builtin_attributes[] =
{
    { "serializable", BuiltinAttributeKind::serializable },
    { "nonserialized", BuiltinAttributeKind::nonserialized },
    { "serialized_version", BuiltinAttributeKind::serialized_version },
    { "version_added", BuiltinAttributeKind::version_added },
    { "version_removed", BuiltinAttributeKind::version_removed },
    { "id", BuiltinAttributeKind::id },
    { "format", BuiltinAttributeKind::format },
    { "serializer_function", BuiltinAttributeKind::serializer_function }
};


Qualifier get_qualifier(const clang::QualType& type)
{
    const auto type_ptr = type.getTypePtrOrNull();

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
    return container_index_of(attributes, [&](const Attribute& attr)
    {
        return attr.hash == type_hash && attr.kind == kind;
    });
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
    err_requires_explicit_ordering = engine->getCustomDiagID(
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

/*
 *************************************
 *
 * ASTMatcher - implementation
 *
 *************************************
 */
ASTMatcher::ASTMatcher(TypeStorage* type_storage, ReflectionAllocator* allocator_ptr)
    : storage(type_storage),
      allocator(allocator_ptr)
{}


void ASTMatcher::run(const clang::ast_matchers::MatchFinder::MatchResult& result)
{
    auto as_record = result.Nodes.getNodeAs<clang::CXXRecordDecl>("id");
    if (as_record != nullptr)
    {
        reflect_record(*as_record);
        return;
    }

    auto as_enum = result.Nodes.getNodeAs<clang::EnumDecl>("id");
    if (as_enum != nullptr)
    {
        reflect_enum(*as_enum);
        return;
    }

    auto as_function = result.Nodes.getNodeAs<clang::FunctionDecl>("id");
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


void ASTMatcher::reflect_record(const clang::CXXRecordDecl& decl, DynamicRecordType* parent)
{
    AttributeParser attr_parser{};

    if (!attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    const auto name = print_name(decl);
    auto& layout = decl.getASTContext().getASTRecordLayout(&decl);
    auto type = allocator->allocate_type<DynamicRecordType>(&decl);
    type->size = layout.getSize().getQuantity();
    type->alignment = layout.getAlignment().getQuantity();
    type->name = allocator->allocate_name(name);
    type->hash = get_type_hash({ name.data(), static_cast<i32>(name.size()) });

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

    if (decl.isTemplateDecl())
    {
        type->kind |= TypeKind::template_decl;
    }

    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&type->attribute_storage, &serialization_info, allocator))
    {
        return;
    }

    type->serialization_flags = serialization_info.flags;
    type->serialized_version = serialization_info.serialized_version;
    type->has_explicit_version = serialization_info.using_explicit_versioning;
    type->attributes = type->attribute_storage.span();
    if (serialization_info.serializer_function != nullptr)
    {
        type->serializer_function_name = serialization_info.serializer_function;
    }

    if (parent == nullptr)
    {
        storage->add_type(type, decl);
    }
    else
    {
        if (!storage->try_map_type(type))
        {
            return;
        }

        parent->add_record(type);
    }

    bool requires_field_order_validation = false;

    for (const clang::Decl* child : decl.decls())
    {
        const auto kind = child->getKind();
        const auto is_enum_or_record = kind == clang::Decl::Kind::CXXRecord || kind == clang::Decl::Kind::Enum;

        // Skip nested type decls that don't have the annotate attribute - we only automatically reflect fields
        if (is_enum_or_record && !child->hasAttr<clang::AnnotateAttr>())
        {
            continue;
        }

        switch (kind)
        {
            case clang::Decl::Kind::CXXRecord:
            {
                const auto child_record = llvm::dyn_cast<clang::CXXRecordDecl>(child);
                if (child_record != nullptr)
                {
                    reflect_record(*child_record, type);
                }
                break;
            }
            case clang::Decl::Kind::Enum:
            {
                const auto child_enum = llvm::dyn_cast<clang::EnumDecl>(child);
                if (child_enum != nullptr)
                {
                    reflect_enum(*child_enum, type);
                }
                break;
            }
            case clang::Decl::Kind::Field:
            {
                const auto old_field_count = type->field_storage.size();
                const auto child_field = llvm::dyn_cast<clang::FieldDecl>(child);

                if (child_field != nullptr)
                {
                    reflect_field(*child_field, type);
                }

                if (type->field_storage.size() > old_field_count && !requires_field_order_validation)
                {
                    requires_field_order_validation = type->field_storage.back().order >= 0;
                }
                break;
            }
            case clang::Decl::Kind::Function:
            case clang::Decl::Kind::CXXMethod:
            {
                const auto child_method = llvm::dyn_cast<clang::FunctionDecl>(child);
                if (child_method != nullptr)
                {
                    reflect_function(*child_method, type);
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
    std::sort(type->field_storage.begin(), type->field_storage.end());

    for (int f = 0; f < type->field_storage.size(); ++f)
    {
        auto& field = type->field_storage[f];
        // Ensure each field has the `id` attribute
        if (field.order < 0)
        {
            diagnostics.Report(field.location, diagnostics.err_requires_explicit_ordering);
            return;
        }

        if (f >= 1 && field.order == type->field_storage[f - 1].order)
        {
            diagnostics.Report(field.location, diagnostics.err_id_is_not_unique);
            return;
        }
    }

    type->fields = Span<Field>(type->field_storage.data(), type->field_storage.size());
}


void ASTMatcher::reflect_enum(const clang::EnumDecl& decl, DynamicRecordType* parent)
{
    auto& ast_context = decl.getASTContext();
    AttributeParser attr_parser{};

    if (!attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    const auto underlying = decl.getIntegerType().getDesugaredType(ast_context);
    // Get the associated types hash so we can look it up later
    const auto underlying_name = print_qualtype_name(underlying, ast_context);
    const auto underlying_type = get_type(get_type_hash({
        underlying_name.c_str(),
        static_cast<i32>(underlying_name.size())
    }));

    if (underlying_type == nullptr)
    {
        diagnostics.Report(decl.getLocation(), clang::diag::err_enum_invalid_underlying);
        return;
    }

    const auto name = print_name(decl);
    auto type = allocator->allocate_type<DynamicEnumType>();
    type->kind = TypeKind::enum_decl;
    type->size = ast_context.getTypeSize(underlying) / 8;
    type->alignment = ast_context.getTypeAlign(underlying) / 8;
    type->name = allocator->allocate_name(name);
    type->hash = get_type_hash({ name.data(), static_cast<i32>(name.size()) });
    type->is_scoped = decl.isScoped();

    for (const clang::EnumConstantDecl* ast_constant : decl.enumerators())
    {
        EnumConstant constant{};
        constant.name = allocator->allocate_name(ast_constant->getName());
        constant.value = ast_constant->getInitVal().getRawData()[0];
        constant.underlying_type = underlying_type;
        type->add_constant(constant);
    }

    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&type->attribute_storage, &serialization_info, allocator))
    {
        return;
    }

    type->serialization_flags = serialization_info.flags;
    type->attributes = type->attribute_storage.span();
    type->serialized_version = serialization_info.serialized_version;

    if (parent == nullptr)
    {
        storage->add_type(type, decl);
        return;
    }

    if (storage->try_map_type(type))
    {
        parent->add_enum(type);
    }
}

void ASTMatcher::reflect_field(const clang::FieldDecl& decl, DynamicRecordType* parent)
{
    AttributeParser attr_parser{};

    const auto requires_annotation = parent == nullptr;

    if (!attr_parser.init(decl, &diagnostics) && requires_annotation)
    {
        return;
    }

    auto field = create_field<OrderedField>(decl.getName(), decl.getFieldIndex(), decl.getASTContext(), parent, decl.getType(), decl.getTypeSpecStartLoc());
    if (field.type == nullptr)
    {
        return;
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

    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&field.attributes, &serialization_info, allocator))
    {
        return;
    }

    field.version_added = serialization_info.version_added;
    field.version_removed = serialization_info.version_removed;
    field.order = serialization_info.id;
    field.location = decl.getLocation();

    /*
     * Validate serialization - ensure the field has both `version_added` and `version_removed` attributes, that its
     * parent record is marked for serialization, and that the fields type declaration is also marked for serialization
     */
    if (parent->serialized_version > 0 && field.version_removed < limits::max<i32>() && field.version_added <= 0)
    {
        diagnostics.Report(decl.getLocation(), diagnostics.err_missing_version_added);
        return;
    }

    // Validate versioning if the parent record type is explicitly versioned
    if (parent->has_explicit_version)
    {
        if (field.version_added > 0 && parent->serialized_version <= 0)
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

    parent->add_field(field);
}


void ASTMatcher::reflect_function(const clang::FunctionDecl& decl, DynamicRecordType* parent)
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

    auto type = allocator->allocate_type<DynamicFunctionType>();
    type->hash = get_type_hash({ type_name.data(), static_cast<i32>(type_name.size()) });
    type->name = allocator->allocate_name(type_name);
    type->size = sizeof(void*); // functions only have size when used as function pointer
    type->alignment = alignof(void*);
    type->kind = TypeKind::function;
    type->return_value = create_field<Field>(decl.getName(), -1, decl.getASTContext(), parent, decl.getReturnType(), decl.getTypeSpecStartLoc());
    type->add_invoker_type_arg(print_qualtype_name(decl.getReturnType(), decl.getASTContext()));

    // If this is a method type then we need to skip the implicit `this` parameter
    auto params_begin = decl.parameters().begin();
    auto params_end = decl.parameters().end();
    if (is_member_function && !decl.parameters().empty())
    {
        ++params_begin;
    }

    for (auto& param : clang::ArrayRef<clang::ParmVarDecl*>(params_begin, params_end))
    {
        auto field = create_field<Field>(param->getName(), param->getFunctionScopeIndex(), param->getASTContext(), parent, param->getType(), param->getLocation());
        field.offset = param->getFunctionScopeIndex();
        field.storage_class = get_storage_class(param->getStorageClass(), param->getStorageDuration());
        type->add_parameter(field);
        type->add_invoker_type_arg(print_qualtype_name(param->getType(), decl.getASTContext()));
    }

    type->storage_class = get_storage_class(decl.getStorageClass(), static_cast<clang::StorageDuration>(0));
    type->is_constexpr = decl.isConstexpr();

    SerializationInfo serialization_info{};
    if (!attr_parser.parse(&type->attribute_storage, &serialization_info, allocator))
    {
        return;
    }

    type->serialization_flags = serialization_info.flags;
    type->attributes = type->attribute_storage.span();
    type->serialized_version = serialization_info.serialized_version;

    if (is_member_function)
    {
        if (parent == nullptr)
        {
            diagnostics.Report(decl.getLocation(), clang::diag::err_incomplete_type);
            return;
        }

        parent->add_function(type);
    }
    else
    {
        storage->add_type(type, decl);
    }
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

bool AttributeParser::is_value_end()
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

    const auto begin = current;

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
    const auto begin = current;

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

    double result = 0.0;
    if (!number_str.getAsDouble(result))
    {
        attribute->kind = AttributeKind::floating_point;
        attribute->value.floating_point = static_cast<float>(result);
        return true;
    }

    if (!number_str.getAsInteger(10, attribute->value.integer))
    {
        attribute->kind = AttributeKind::integer;
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

    const auto begin = current;
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

    const auto builtin_index = container_index_of(g_builtin_attributes, [&](const BuiltinAttribute& builtin)
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
        }
        default: break;
    }

    return true;
}

bool AttributeParser::parse(DynamicArray<Attribute>* dst_attributes, SerializationInfo* serialization_info, ReflectionAllocator* refl_allocator)
{
    if (is_field)
    {
        serialization_info->serializable = true;
    }

    if (!empty && current != nullptr)
    {


        allocator = refl_allocator;

        const auto begin = current;

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

    return true;
}


bool AttributeParser::init(const clang::Decl& decl, Diagnostics* new_diagnostics)
{
    is_field = decl.getKind() == clang::Decl::Kind::Field;
    current = nullptr;

    llvm::StringRef annotation_str;
    diagnostics = new_diagnostics;

    for (auto& attribute : decl.attrs())
    {
        if (attribute->getKind() != clang::attr::Annotate)
        {
            continue;
        }

        auto annotation_decl = llvm::dyn_cast<clang::AnnotateAttr>(attribute);
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