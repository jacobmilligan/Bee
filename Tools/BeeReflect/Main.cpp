/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


//#include "ClangParser.hpp"
//
#include "Frontend.hpp"

#include "Bee/Core/Logger.hpp"
#include "Bee/Core/IO.hpp"

#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>


const char* reflection_flag_to_string(const bee::Qualifier qualifier)
{
#define QUALIFIER(x) case bee::Qualifier::x: return "bee::Qualifier::" #x

    switch (qualifier)
    {
        QUALIFIER(cv_const);
        QUALIFIER(cv_volatile);
        QUALIFIER(lvalue_ref);
        QUALIFIER(rvalue_ref);
        QUALIFIER(pointer);
        default: break;
    }

    return "bee::Qualifier::none";
#undef QUALIFIER
}

const char* reflection_flag_to_string(const bee::StorageClass storage_class)
{
#define STORAGE_CLASS(x) case bee::StorageClass::x: return "bee::StorageClass::" #x

    switch (storage_class)
    {
        STORAGE_CLASS(auto_storage);
        STORAGE_CLASS(register_storage);
        STORAGE_CLASS(static_storage);
        STORAGE_CLASS(extern_storage);
        STORAGE_CLASS(thread_local_storage);
        STORAGE_CLASS(mutable_storage);
        default: break;
    }

    return "bee::StorageClass::none";
#undef STORAGE_CLASS
}

const char* type_kind_to_string(const bee::TypeKind type_kind)
{
#define TYPE_KIND(x) case bee::TypeKind::x: return "bee::TypeKind::" #x

    switch (type_kind)
    {
        TYPE_KIND(class_decl);
        TYPE_KIND(struct_decl);
        TYPE_KIND(enum_decl);
        TYPE_KIND(union_decl);
        TYPE_KIND(template_decl);
        TYPE_KIND(field);
        TYPE_KIND(function);
        TYPE_KIND(fundamental);
        default: break;
    }

    return "bee::TypeKind::unknown";
#undef TYPE_KIND
}

template <typename FlagType>
const char* dump_flags(const FlagType flag)
{
    static thread_local char buffer[4096];
    bee::io::StringStream stream(buffer, bee::static_array_length(buffer), 0);

    int count = 0;
    bee::for_each_flag(flag, [&](const FlagType& f)
    {
        stream.write_fmt(" %s |", reflection_flag_to_string(f));
        ++count;
    });

    if (count == 0)
    {
        stream.write(reflection_flag_to_string(static_cast<FlagType>(0u)));
    }

    if (buffer[stream.size() - 1] == '|')
    {
        buffer[stream.size() - 1] = '\0';
    }

    // Skip the first space that occurs when getting multiple flags
    return count > 0 ? buffer + 1 : buffer;
}


void dump_types(const bee::Span<bee::Type* const>& types, bee::io::StringStream* stream)
{

    for (const bee::Type* type : types)
    {
        stream->write_fmt(
            "== %s (0x%08x) ==\n\n- size: %zu\n- alignment: %zu\n- kind: %s\n",
            type->name,
            type->hash,
            type->size,
            type->alignment,
            type_kind_to_string(type->kind)
        );

        if (type->kind == bee::TypeKind::class_decl || type->kind == bee::TypeKind::struct_decl || type->kind == bee::TypeKind::union_decl)
        {
            auto as_class = reinterpret_cast<const bee::RecordType*>(type);

            stream->write("- fields:\n");

            for (auto& field : as_class->fields)
            {
                stream->write_fmt("  * %s", field.name);

                if (field.type != nullptr)
                {
                    stream->write_fmt(" [%s]", field.type->name);
                }

                stream->write_fmt(":\n    - qualifier: %s\n    - storage_class: %s\n    - offset: %zu\n",
                    dump_flags(field.qualifier),
                    dump_flags(field.storage_class),
                    field.offset
                );
            }

            stream->write("- functions:\n");

            for (auto& function : as_class->functions)
            {
                stream->write("  * ");

                if (function->is_constexpr)
                {
                    stream->write("constexpr ");
                }

                stream->write_fmt("%s %s(", function->return_value.type->name, function->name);

                for (const auto param : enumerate(function->parameters))
                {
                    if (param.value.qualifier != bee::Qualifier::none)
                    {
                        stream->write_fmt("%s ", dump_flags(param.value.qualifier));
                    }
                    stream->write_fmt("%s %s", param.value.type->name, param.value.name);
                    if (param.index < function->parameters.size() - 1)
                    {
                        stream->write(", ");
                    }
                }

                stream->write(") ");
                stream->write_fmt("[storage_class: %s]\n", dump_flags(function->storage_class));
            }
        }

        stream->write("\n");
    }
}


int main(int argc, const char** argv)
{

    // Set up the command line options
    llvm::cl::OptionCategory clang_reflect_category("clang-reflect options");

    // CommonOptionsParser declares HelpMessage with a description of the common
    // command-line options related to the compilation database and input files.
    // It's nice to have this help message in all tools.
    llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
    clang::tooling::CommonOptionsParser options_parser(argc, argv, clang_reflect_category);
    clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

    bee::ClangReflectFrontendActionFactory factory;

    const auto result = tool.run(&factory);
    if (result != 0)
    {
        return result;
    }

    bee::String output;
    bee::io::StringStream stream(&output);
    auto x = factory.types.const_span();
    dump_types(factory.types.const_span(), &stream);

    bee::log_info("%s", output.c_str());
}