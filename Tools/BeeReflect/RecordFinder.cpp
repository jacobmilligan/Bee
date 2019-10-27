/*
 *  RecordFinder.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "RecordFinder.hpp"

#include <clang/Lex/PreprocessorOptions.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/QualTypeNames.h>
#include <clang/Basic/DiagnosticSema.h>



namespace bee {


template <typename T>
Type builtin_type()
{
    Type t;
    t.hash = get_type_hash<T>();
    t.name = get_type_name<T>();
    t.size = sizeof_helper<T>();
    t.alignment = alignof_helper<T>();
    t.kind = TypeKind::fundamental;

    return t;
}


// This has to be a static local so that static_type_info<T> has its name fields intitialized first
Span<Type> get_builtin_types()
{
    static Type builtin[] =
    {
        builtin_type<bool>(),
        builtin_type<i8>(),
        builtin_type<i16>(),
        builtin_type<i32>(),
        builtin_type<i64>(),
        builtin_type<u8>(),
        builtin_type<u16>(),
        builtin_type<u32>(),
        builtin_type<u64>(),
        builtin_type<float>(),
        builtin_type<double>(),
        builtin_type<char>(),
        builtin_type<void>()
    };

    return make_span(builtin, static_array_length(builtin));
}


bool parse_attribute(const llvm::StringRef src, Attribute* attr)
{
    auto attr_ast = clang::tooling::buildASTFromCode(src);
    auto results = clang::ast_matchers::match(clang::ast_matchers::callExpr(), attr_ast->getASTContext());

    return false;
}


bool parse_attributes(const TypeKind kind, const llvm::StringRef attribute_string, DynamicArray<Attribute>* attributes)
{
    if (kind == TypeKind::function)
    {
        return false;
    }

    std::pair<llvm::StringRef, llvm::StringRef> attr_begin;

    if (kind == TypeKind::field)
    {
        attr_begin = attribute_string.split("bee-reflect-field[");
    }
    else
    {
        attr_begin = attribute_string.split("bee-reflect-class[");
    }

    if (attr_begin.first == attribute_string)
    {
        attributes->push_back(Attribute{});
        return true;
    }

    while (true)
    {
        attr_begin = attr_begin.second.split(',');
        if (attr_begin.second.empty())
        {
            break;
        }

        attributes->emplace_back();

        if (!parse_attribute(attr_begin.first, &attributes->back()))
        {
            return false;
        }
    }

    return true;
}

llvm::StringRef get_annotation(const clang::Decl& decl)
{
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

        llvm::StringRef annotation = annotation_decl->getAnnotation();

        const auto is_reflected = annotation.startswith("bee-reflect");
        const auto is_attribute = annotation.startswith("bee-attribute");

        if ((!is_reflected && !is_attribute) || !annotation.endswith("]"))
        {
            continue;
        }

        return annotation.split("[").second;
    }

    return llvm::StringRef{};
}


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


RecordFinder::RecordFinder(bee::DynamicArray<bee::Type*>* type_array, bee::ReflectionAllocator* allocator_ptr)
    : types(type_array),
      allocator(allocator_ptr)
{
    // Add all the builtin types to the lookup
    for (Type& type : get_builtin_types())
    {
        add_type(&type);
    }
}

void RecordFinder::add_type(Type* type)
{
    if (type_lookup.find(type->hash) != nullptr)
    {
        return;
    }

    types->push_back(type);
    type_lookup.insert(type->hash, type);
}


void RecordFinder::run(const clang::ast_matchers::MatchFinder::MatchResult& result)
{
    auto as_record = result.Nodes.getNodeAs<clang::CXXRecordDecl>("id");
    if (as_record != nullptr)
    {
        reflect_record(*as_record);
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

void RecordFinder::reflect_record(const clang::CXXRecordDecl& decl)
{
    auto& ast_context = decl.getASTContext();
    auto& diagnostics = ast_context.getDiagnostics();
    auto annotation = get_annotation(decl);

    if (annotation.data() == nullptr)
    {
        diagnostics.Report(decl.getLocation(), clang::diag::warn_unknown_attribute_ignored);
        return;
    }

    auto& layout = decl.getASTContext().getASTRecordLayout(&decl);

    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    decl.printQualifiedName(type_name_stream);

    auto type = allocator->allocate_type<RecordType>();
    type->size = layout.getSize().getQuantity();
    type->alignment = layout.getAlignment().getQuantity();
    type->name = allocator->allocate_name(type_name);
    type->hash = detail::runtime_fnv1a(type_name.data(), type_name.size());

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
        diagnostics.Report(decl.getLocation(), clang::diag::err_attribute_argument_invalid);
        return;
    }

    if (decl.isTemplateDecl())
    {
        type->kind |= TypeKind::template_decl;
    }

    add_type(type);
    current_record = type;
}


Field RecordFinder::create_field(const llvm::StringRef& name, const i32 index, const clang::ASTContext& ast_context, const clang::RecordDecl* parent, const clang::QualType& qual_type, const clang::SourceLocation& location, clang::DiagnosticsEngine& diagnostics)
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
        diagnostics.Report(location, clang::diag::err_invalid_member_in_interface);
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
    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    auto fully_qualified_name = clang::TypeName::getFullyQualifiedName(original_type, ast_context, ast_context.getPrintingPolicy());
    const auto type_hash = detail::runtime_fnv1a(fully_qualified_name.data(), fully_qualified_name.size());

    auto type = type_lookup.find(type_hash);
    if (type == nullptr)
    {
        log_error("Missing type: %s (0x%08x)", fully_qualified_name.c_str(), type_hash);
        diagnostics.Report(location, clang::diag::err_field_incomplete);
        return Field{};
    }

    field.type = type->value;
    return field;
}


void RecordFinder::reflect_field(const clang::FieldDecl& decl)
{
    auto& diagnostics = decl.getASTContext().getDiagnostics();
    auto annotation = get_annotation(decl);

    if (annotation.data() == nullptr)
    {
        diagnostics.Report(decl.getLocation(), clang::diag::warn_unknown_attribute_ignored);
        return;
    }

    auto field = create_field(decl.getName(), decl.getFieldIndex(), decl.getASTContext(), decl.getParent(), decl.getType(), decl.getTypeSpecStartLoc(), diagnostics);
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
        diagnostics.Report(decl.getLocation(), clang::diag::err_invalid_member_in_interface);
        return;
    }

    current_record->fields.push_back(field);
}


void RecordFinder::reflect_function(const clang::FunctionDecl& decl)
{
    auto& diagnostics = decl.getASTContext().getDiagnostics();
    auto annotation = get_annotation(decl);

    if (annotation.data() == nullptr)
    {
        diagnostics.Report(decl.getLocation(), clang::diag::warn_unknown_attribute_ignored);
        return;
    }

    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    decl.printQualifiedName(type_name_stream);

    auto type = allocator->allocate_type<FunctionType>();
    type->hash = detail::runtime_fnv1a(type_name.data(), type_name.size());
    type->name = allocator->allocate_name(type_name);
    type->size = sizeof(void*); // functions only have size when used as function pointer
    type->alignment = alignof(void*);
    type->kind = TypeKind::function;

    const auto is_member_function = decl.isCXXClassMember();
    const auto parent = decl.isCXXClassMember() ? (const clang::RecordDecl*)(decl.getParent()) : nullptr;

    type->return_value = create_field(decl.getName(), -1, decl.getASTContext(), parent, decl.getReturnType(), decl.getTypeSpecStartLoc(), diagnostics);

    for (auto& param : decl.parameters())
    {
        auto field = create_field(param->getName(), param->getFunctionScopeIndex(), param->getASTContext(), parent, param->getType(), param->getLocation(), diagnostics);
        field.offset = param->getFunctionScopeIndex();
        field.storage_class = get_storage_class(param->getStorageClass(), param->getStorageDuration());
        type->parameters.push_back(field);
    }

    type->storage_class = get_storage_class(decl.getStorageClass(), static_cast<clang::StorageDuration>(0));
    type->is_constexpr = decl.isConstexpr();

    if (is_member_function)
    {
        if (current_record == nullptr)
        {
            diagnostics.Report(decl.getLocation(), clang::diag::err_invalid_member_in_interface);
            return;
        }

        current_record->functions.push_back(type);
    }

    add_type(type);
}



} // namespace bee