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

    auto as_field = result.Nodes.getNodeAs<clang::FieldDecl>("id");
    if (as_field != nullptr)
    {
        reflect_field(*as_field);
        return;
    }

    auto as_function = result.Nodes.getNodeAs<clang::FunctionDecl>("id");
    if (as_function != nullptr && as_function->isFirstDecl())
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


void ASTMatcher::reflect_record(const clang::CXXRecordDecl& decl)
{
    AttributeParser attr_parser{};

    if (!attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    const auto name = print_name(decl);
    auto& layout = decl.getASTContext().getASTRecordLayout(&decl);;
    auto type = allocator->allocate_type<DynamicRecordType>();
    type->size = layout.getSize().getQuantity();
    type->alignment = layout.getAlignment().getQuantity();
    type->name = allocator->allocate_name(name);
    type->hash = detail::runtime_fnv1a(name.data(), name.size());

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

    if (!attr_parser.parse(&type->attribute_storage, allocator))
    {
        return;
    }

    type->attributes = type->attribute_storage.span();
    storage->add_type(type, decl);
    current_record = type;
}


void ASTMatcher::reflect_enum(const clang::EnumDecl& decl)
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
    const auto underlying_type = get_type(detail::runtime_fnv1a(underlying_name.data(), underlying_name.size()));

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
    type->hash = detail::runtime_fnv1a(name.data(), name.size());
    type->is_scoped = decl.isScoped();

    for (const clang::EnumConstantDecl* ast_constant : decl.enumerators())
    {
        EnumConstant constant{};
        constant.name = allocator->allocate_name(ast_constant->getName());
        constant.value = ast_constant->getInitVal().getRawData()[0];
        constant.underlying_type = underlying_type;
        type->add_constant(constant);
    }

    if (!attr_parser.parse(&type->attribute_storage, allocator))
    {
        return;
    }

    type->attributes = type->attribute_storage.span();
    storage->add_type(type, decl);
}


Field ASTMatcher::create_field(const llvm::StringRef& name, const i32 index, const clang::ASTContext& ast_context, const clang::RecordDecl* parent, const clang::QualType& qual_type, const clang::SourceLocation& location)
{
    /*
     * Get the layout of the parent record this field is in and also get pointers to the desugared type so that
     * i.e. u8 becomes unsigned char
     */
    auto desugared_type = qual_type.getDesugaredType(ast_context);

    Field field;
    field.name = allocator->allocate_name(name);
    field.offset = 0;
    field.qualifier = get_qualifier(desugared_type);

    if (parent != nullptr && index >= 0)
    {
        field.offset = ast_context.getASTRecordLayout(parent).getFieldOffset(static_cast<u32>(index)) / 8;
    }

    if (current_record == nullptr)
    {
        diagnostics.Report(location, clang::diag::err_incomplete_type);
        return Field{};
    }

    clang::QualType original_type;
    auto type_ptr = desugared_type.getTypePtrOrNull();

    // Check if reference or pointer and get the pointee and const-qualified info before removing qualifications
    if (type_ptr != nullptr && (type_ptr->isPointerType() || type_ptr->isLValueReferenceType()))
    {
        const auto pointee = type_ptr->getPointeeType();

        if (pointee.isConstQualified())
        {
            field.qualifier |= Qualifier::cv_const;
        }

        original_type = pointee.getUnqualifiedType();
    }
    else
    {
        original_type = desugared_type.getUnqualifiedType();
    }

    // Get the associated types hash so we can look it up later
    const auto fully_qualified_name = print_qualtype_name(original_type, ast_context);
    const auto type_hash = detail::runtime_fnv1a(fully_qualified_name.data(), fully_qualified_name.size());

    auto type = storage->find_type(type_hash);
    if (type == nullptr)
    {
        /*
         * If the type is missing it might be a core builtin type which can be accessed by bee-reflect via
         * get_type. This is safe to do here as bee-reflect doesn't link to any generated cpp files
         */
        type = get_type(type_hash);
        if (type->kind == TypeKind::unknown)
        {
            log_error("Missing type: %s (0x%08x)", fully_qualified_name.c_str(), type_hash);
            diagnostics.Report(location, clang::diag::err_field_incomplete);
            return Field{};
        }
    }

    field.type = type;
    return field;
}


void ASTMatcher::reflect_field(const clang::FieldDecl& decl)
{
    AttributeParser attr_parser{};

    if (!attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    auto field = create_field(decl.getName(), decl.getFieldIndex(), decl.getASTContext(), decl.getParent(), decl.getType(), decl.getTypeSpecStartLoc());
    if (field.type == nullptr)
    {
        return;
    }

    if (decl.isMutable())
    {
        field.storage_class |= StorageClass::mutable_storage;
    }

    if (current_record == nullptr)
    {
        diagnostics.Report(decl.getLocation(), clang::diag::err_incomplete_type);
        return;
    }

    DynamicArray<Attribute> attributes;
    if (!attr_parser.parse(&attributes, allocator))
    {
        return;
    }

    current_record->add_field(field, std::move(attributes));
}


void ASTMatcher::reflect_function(const clang::FunctionDecl& decl)
{
    AttributeParser attr_parser{};

    if (attr_parser.init(decl, &diagnostics))
    {
        return;
    }

    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    decl.printQualifiedName(type_name_stream);

    auto type = allocator->allocate_type<DynamicFunctionType>();
    type->hash = detail::runtime_fnv1a(type_name.data(), type_name.size());
    type->name = allocator->allocate_name(type_name);
    type->size = sizeof(void*); // functions only have size when used as function pointer
    type->alignment = alignof(void*);
    type->kind = TypeKind::function;

    const auto is_member_function = decl.isCXXClassMember();
    const auto parent = decl.isCXXClassMember() ? (const clang::RecordDecl*)(decl.getParent()) : nullptr;

    type->return_value = create_field(decl.getName(), -1, decl.getASTContext(), parent, decl.getReturnType(), decl.getTypeSpecStartLoc());

    for (auto& param : decl.parameters())
    {
        auto field = create_field(param->getName(), param->getFunctionScopeIndex(), param->getASTContext(), parent, param->getType(), param->getLocation());
        field.offset = param->getFunctionScopeIndex();
        field.storage_class = get_storage_class(param->getStorageClass(), param->getStorageDuration());
        type->add_parameter(field);
    }

    type->storage_class = get_storage_class(decl.getStorageClass(), static_cast<clang::StorageDuration>(0));
    type->is_constexpr = decl.isConstexpr();

    if (!attr_parser.parse(&type->attribute_storage, allocator))
    {
        return;
    }

    type->attributes = type->attribute_storage.span();

    if (is_member_function)
    {
        if (current_record == nullptr)
        {
            diagnostics.Report(decl.getLocation(), clang::diag::err_incomplete_type);
            return;
        }

        current_record->add_function(type);
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

const char* AttributeParser::parse_name()
{
    const char* begin = current;

    while (current != end)
    {
        if (str::is_space(*current) || *current == '=' || *current == ',')
        {
            return allocator->allocate_name(llvm::StringRef(begin, current - begin));
        }

        next();
    }

    diagnostics->Report(location, clang::diag::err_expected_string_literal);
    return nullptr;
}

bool AttributeParser::parse_value(Attribute* attribute)
{
    // Parse as string
    if (*current == '\"')
    {
        next();

        const auto begin = current;

        while (current != end && *current != '\"')
        {
            ++current;
        }

        if (*current != '\"')
        {
            diagnostics->Report(location, clang::diag::err_expected_string_literal);
            return false;
        }

        attribute->kind = AttributeKind::string;
        attribute->value.string = allocator->allocate_name(llvm::StringRef(begin, current - begin));
        next();
        return true;
    }


    const char* begin = current;

    // Otherwise parse as unquoted value type
    while (current != end && *current != ',' && !str::is_space(*current) && *current != ']')
    {
        ++current;
    }

    llvm::StringRef ref(begin, current - begin);

    if (ref.empty())
    {
        diagnostics->Report(location, clang::diag::err_attribute_unsupported);
        return false;
    }

    /*
     * Test:
     * - bool
     * - double
     * - int
     */
    const auto is_true = ref == "true";
    const auto is_false = ref == "false";
    if (is_true || is_false)
    {
        attribute->kind = AttributeKind::boolean;
        attribute->value.boolean = is_true;
        return true;
    }

    double result = 0.0;
    if (!ref.getAsDouble(result))
    {
        attribute->kind = AttributeKind::floating_point;
        attribute->value.floating_point = static_cast<float>(result);
        return true;
    }

    if (!ref.getAsInteger(10, attribute->value.integer))
    {
        attribute->kind = AttributeKind::integer;
        return true;
    }

    diagnostics->Report(location, clang::diag::err_type_unsupported);
    return false;
}

bool AttributeParser::parse_attribute()
{
    skip_whitespace();

    Attribute attribute{};
    attribute.name = parse_name();

    if (attribute.name == nullptr)
    {
        return false;
    }

    skip_whitespace();

    attribute.hash = detail::runtime_fnv1a(attribute.name, str::length(attribute.name));

    if (*current == ',')
    {
        attribute.kind = AttributeKind::boolean;
        attribute.value.boolean = true;
        next();
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
            return false;
        }

        if (*current == ',')
        {
            next();
        }
    }

    dst->push_back(attribute);
    return true;
}

bool AttributeParser::parse(DynamicArray<Attribute>* dst_attributes, ReflectionAllocator* refl_allocator)
{
    dst = dst_attributes;
    allocator = refl_allocator;

    while (current != end && *current != ']')
    {
        if (!parse_attribute())
        {
            return false;
        }
    }

    if (*current != ']')
    {
        diagnostics->Report(location, diagnostics->err_invalid_annotation_format);
        return false;
    }

    return true;
}


bool AttributeParser::init(const clang::Decl& decl, Diagnostics* new_diagnostics)
{
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


    if (annotation_str.empty() || !annotation_str.startswith("bee-reflect"))
    {
        diagnostics->Report(decl.getLocation(), diagnostics->err_invalid_annotation_format);
        return false;
    }

    auto attributes_str = annotation_str.split("[");

    if (attributes_str.first == annotation_str)
    {
        diagnostics->Report(decl.getLocation(), diagnostics->err_invalid_annotation_format);
        return false;
    }

    current = attributes_str.second.data();
    end = attributes_str.second.end();
    location = decl.getLocation();

    return true;
}


} // namespace reflect
} // namespace bee