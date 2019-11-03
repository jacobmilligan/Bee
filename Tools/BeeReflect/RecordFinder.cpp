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


Type* TypeStorage::add_type(Type* type, const clang::Decl& decl)
{
    auto& src_manager = decl.getASTContext().getSourceManager();
    const auto file = src_manager.getFileEntryForID(src_manager.getFileID(decl.getLocation()));
    const auto filename_ref = file->getName();
    const auto filename = StringView(filename_ref.data(), filename_ref.size());

    if (hash_to_type_map.find(type->hash) != nullptr)
    {
        return nullptr;
    }

    types.emplace_back(type);

    hash_to_type_map.insert(type->hash, type);

    auto mapped_file = file_to_type_map.find(filename);

    if (mapped_file == nullptr)
    {
        mapped_file = file_to_type_map.insert(Path(filename), DynamicArray<const Type*>());
    }

    mapped_file->value.push_back(type);

    return types.back();

}

const Type* TypeStorage::find_type(const u32 hash)
{
    auto type = hash_to_type_map.find(hash);
    return type == nullptr ? nullptr : type->value;
}


RecordFinder::RecordFinder(TypeStorage* type_array, bee::ReflectionAllocator* allocator_ptr)
    : storage(type_array),
      allocator(allocator_ptr)
{}


void RecordFinder::run(const clang::ast_matchers::MatchFinder::MatchResult& result)
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

llvm::StringRef RecordFinder::print_name(const clang::NamedDecl& decl)
{
    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    decl.printQualifiedName(type_name_stream);
    return type_name;
}

std::string RecordFinder::print_qualtype_name(const clang::QualType& type, const clang::ASTContext& ast_context)
{
    type_name.clear();
    llvm::raw_svector_ostream type_name_stream(type_name);
    return clang::TypeName::getFullyQualifiedName(type, ast_context, ast_context.getPrintingPolicy());
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
        diagnostics.Report(decl.getLocation(), clang::diag::err_attribute_argument_invalid);
        return;
    }

    if (decl.isTemplateDecl())
    {
        type->kind |= TypeKind::template_decl;
    }

    storage->add_type(type, decl);
    current_record = type;
}


void RecordFinder::reflect_enum(const clang::EnumDecl& decl)
{
    auto& ast_context = decl.getASTContext();
    auto& diagnostics = ast_context.getDiagnostics();
    auto annotation = get_annotation(decl);

    if (annotation.data() == nullptr)
    {
        diagnostics.Report(decl.getLocation(), clang::diag::warn_unknown_attribute_ignored);
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

    log_info("%s", type->name);
    storage->add_type(type, decl);
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

    current_record->add_field(field);
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

    auto type = allocator->allocate_type<DynamicFunctionType>();
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
        type->add_parameter(field);
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

        current_record->add_function(type);
    }
    else
    {
        storage->add_type(type, decl);
    }
}



} // namespace bee