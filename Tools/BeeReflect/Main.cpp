/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


//#include "ClangParser.hpp"
//
#include "Bee/Core/Main.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/IO.hpp"
#include "CodeGen.hpp"
#include "Frontend.hpp"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <unordered_set>

int bee_main(int argc, char** argv)
{
    bee::reflection_register_builtin_types();

    // Set up the command line options
    llvm::cl::OptionCategory bee_reflect_cat("bee-reflect options");

    llvm::cl::opt<std::string> output_dir_opt(
        "output",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Directory to output all generated cpp files"),
        llvm::cl::Required
    );
    llvm::cl::opt<bool> inline_opt(
        "inline",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Generate reflection as a .inl file to be #included rather than a .cpp file with exported symbols")
    );
    llvm::cl::alias inline_alias("i", llvm::cl::desc("Alias for -inline"), llvm::cl::aliasopt(inline_opt));

    // CommonOptionsParser declares HelpMessage with a description of the common
    // command-line options related to the compilation database and input files.
    // It's nice to have this help message in all tools.
    llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
    clang::tooling::CommonOptionsParser options_parser(argc, const_cast<const char**>(argv), bee_reflect_cat);

    clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

    const auto output_dir = bee::Path(output_dir_opt.c_str());
    const auto generated_inl_dir = output_dir.join("ReflectedTemplates");

    if (!output_dir.exists())
    {
        bee::fs::mkdir(output_dir, true);
    }

    if (!generated_inl_dir.exists())
    {
        bee::fs::mkdir(generated_inl_dir, true);
    }

    bee::reflect::BeeReflectFrontendActionFactory factory;

    const auto result = tool.run(&factory);

    if (result != 0)
    {
        bee::log_error("bee-reflect: failed to generate reflection data");
        return result;
    }

    // Keep track of all the reflected files absolute paths for later so we can delete old, nonreflected files
    bee::DynamicArray<bee::Path> reflected_abs_paths;
    bee::DynamicArray<const bee::TypeInfo*> reflected_types;

    const auto src_path_list = options_parser.getSourcePathList();
    int type_count_for_inl = 0;

    // Output a .generated.cpp file for each of the reflected headers
    for (auto& file : factory.storage.reflected_files)
    {
        const auto iter = std::find_if(src_path_list.begin(), src_path_list.end(), [&](const std::string& str)
        {
            return llvm::StringRef(str).endswith(llvm::StringRef(file.value.location.c_str(), file.value.location.size()));
        });

        if (iter == src_path_list.end())
        {
            continue;
        }

        auto src_codegen_mode = bee::reflect::CodegenMode::cpp;
        if (inline_opt)
        {
            src_codegen_mode = bee::reflect::CodegenMode::inl;
        }

        auto output_path = output_dir.join(file.value.location.filename(), bee::temp_allocator())
                                     .set_extension("generated")
                                     .append_extension(".cpp");

        // Generate all non-templated types into a generated.cpp file
        bee::String output;
        bee::io::StringStream stream(&output);
        if (bee::reflect::generate_reflection(output_path, file.value, &stream, src_codegen_mode) <= 0)
        {
            // If there's only template types in a generated file, this should be re-written as a empty file
            output.clear();
            stream.seek(0, bee::io::SeekOrigin::begin);
            bee::reflect::generate_empty_reflection(output_path, file.value.location.c_str(), &stream);
        }
        bee::fs::write(output_path, output.view());

        // Generate a matching .inl file with just the get_type(module, index) portion if inline mode and the file has
        // non-template types
        if (src_codegen_mode == bee::reflect::CodegenMode::inl)
        {
            output_path.set_extension(".inl");
            output.clear();
            stream.seek(0, bee::io::SeekOrigin::begin);

            type_count_for_inl += bee::reflect::generate_reflection_header(
                output_path,
                file.value,
                type_count_for_inl,
                &stream,
                src_codegen_mode
            );

            bee::fs::write(output_path, output.view());
        }

        // Output a generated.inl file if required - a type in the file is templated and requires a `get_type` specialization
        const auto inl_path = generated_inl_dir.join(file.value.location.filename(), bee::temp_allocator())
                                               .set_extension("generated")
                                               .append_extension("inl");
        output.clear();
        stream.seek(0, bee::io::SeekOrigin::begin);
        if (bee::reflect::generate_reflection(inl_path, file.value, &stream, bee::reflect::CodegenMode::templates_only) > 0)
        {
            bee::fs::write(inl_path, output.view());
        }

        reflected_abs_paths.push_back(file.value.location);
        reflected_types.append(file.value.all_types.const_span()); // keep track of these for generating typelists
    }

    bee::String header_comment(bee::temp_allocator());
    bee::io::StringStream stream(&header_comment);

    for (const std::string& compilation : options_parser.getSourcePathList())
    {
        const auto was_reflected = bee::find_index_if(reflected_abs_paths, [&](const bee::Path& reflected)
        {
            return compilation.c_str() == reflected.view();
        }) >= 0;

        if (!was_reflected)
        {
            const auto filename = bee::Path(compilation.c_str(), bee::temp_allocator()).filename();
            auto output_path = output_dir.join(filename).set_extension("generated").append_extension("cpp");
            bee::reflect::generate_empty_reflection(output_path, compilation.c_str(), &stream);
            bee::fs::write(output_path, header_comment.view());
            stream.seek(0, bee::io::SeekOrigin::begin);
            header_comment.clear();
        }
    }

    const auto typelist_mode = inline_opt ? bee::reflect::CodegenMode::inl : bee::reflect::CodegenMode::cpp;
    bee::reflect::generate_typelist(output_dir, reflected_types.span(), typelist_mode, reflected_abs_paths.const_span());

    return EXIT_SUCCESS;
}