/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


//#include "ClangParser.hpp"
//
#include "Frontend.hpp"
#include "CodeGen.hpp"

#include "Bee/Core/IO.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Application/Main.hpp"


#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>


int bee_main(int argc, char** argv)
{
    bee::reflection_register_builtin_types();

    // Set up the command line options
    llvm::cl::OptionCategory bee_reflect_cat("bee-reflect options");

    llvm::cl::SubCommand generate_subcommand(
        "generate",
        "Generates static reflection data as a collection .cpp files"
    );
    llvm::cl::opt<std::string> output_dir_opt(
        "output",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Directory to output all generated cpp files"),
        llvm::cl::Required,
        llvm::cl::sub(generate_subcommand)
    );

    llvm::cl::SubCommand link_subcommand(
        "link",
        "Links .registration files together into a single .cpp file for runtime type registration"
    );
    llvm::cl::opt<std::string> output_link_opt(
        "output",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Path to write the .cpp result to"),
        llvm::cl::Required,
        llvm::cl::sub(link_subcommand)
    );

    // CommonOptionsParser declares HelpMessage with a description of the common
    // command-line options related to the compilation database and input files.
    // It's nice to have this help message in all tools.
    llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
    clang::tooling::CommonOptionsParser options_parser(argc, const_cast<const char**>(argv), bee_reflect_cat);

    if (generate_subcommand)
    {
        clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

        const auto output_dir = bee::Path(output_dir_opt.c_str());
        if (!output_dir.exists())
        {
            bee::fs::mkdir(output_dir);
        }

        bee::reflect::BeeReflectFrontendActionFactory factory;

        const auto result = tool.run(&factory);
        if (result != 0)
        {
            return result;
        }

        // Output a .generated.cpp file for each of the reflected headers
        for (auto& file : factory.storage.file_to_type_map)
        {
            bee::String output;
            bee::io::StringStream stream(&output);
            bee::reflect::generate_reflection(file.key, file.value.span(), &stream);

            auto output_path = output_dir.join(file.key.filename(), bee::temp_allocator())
                                         .set_extension("generated")
                                         .append_extension("cpp");
            bee::fs::write(output_path, output.view());

            output_path.set_extension("hpp");
            output.clear();
            stream.seek(0, bee::io::SeekOrigin::begin);

            // Output a .registration file for looking up a type by hash
            const auto reg_path = output_dir.join(file.key.filename(), bee::temp_allocator()).set_extension("registration");
            bee::reflect::generate_registration(file.value.span(), &stream);

            bee::fs::write(reg_path, output.view());
        }

        return EXIT_SUCCESS;
    }

    if (link_subcommand)
    {
        auto& source_path_list = options_parser.getSourcePathList();
        auto search_paths = bee::FixedArray<bee::Path>::with_size(source_path_list.size());
        for (int p = 0; p < search_paths.size(); ++p)
        {
            auto path = bee::Path(source_path_list[p].c_str());
            if (!bee::fs::is_dir(path))
            {
                bee::log_error("%s is not a .registration file search path", path.c_str());
                continue;
            }
            search_paths[p] = path;
        }

        bee::String output;
        bee::io::StringStream stream(&output);
        bee::reflect::link_registrations(search_paths.const_span(), &stream);

        bee::Path output_path(output_link_opt.getValue().c_str());
        bee::fs::write(output_path, stream.view());
    }

    return EXIT_SUCCESS;
}