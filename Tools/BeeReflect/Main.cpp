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

#include "Bee/Core/Logger.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Filesystem.hpp"


#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>


int main(int argc, const char** argv)
{
    bee::reflection_initv2();

    // Set up the command line options
    llvm::cl::OptionCategory clang_reflect_category("bee-reflect options");

    llvm::cl::opt<std::string> output_option("output", llvm::cl::cat(clang_reflect_category), llvm::cl::desc("Relative path to the directory to output all generated cpp files"), llvm::cl::Required);

    // CommonOptionsParser declares HelpMessage with a description of the common
    // command-line options related to the compilation database and input files.
    // It's nice to have this help message in all tools.
    llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
    clang::tooling::CommonOptionsParser options_parser(argc, argv, clang_reflect_category);
    clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

    const auto output_dir = bee::Path(output_option.getValue().c_str());
    if (!bee::fs::is_dir(output_dir) || !output_dir.exists())
    {
        bee::log_error("bee-reflect: --output must be a valid relative path to a directory");
        return EXIT_FAILURE;
    }

    bee::ClangReflectFrontendActionFactory factory;

    const auto result = tool.run(&factory);
    if (result != 0)
    {
        return result;
    }

    for (auto& file : factory.storage.file_to_type_map)
    {
        bee::String output;
        bee::io::StringStream stream(&output);
        bee::reflection_codegen(file.key, file.value.span(), &stream);

        auto output_path = output_dir.join(file.key.filename(), bee::temp_allocator());
        const auto ext = bee::String(output_path.extension(), bee::temp_allocator());
        output_path.set_extension("gen").append_extension(ext.view());

        bee::fs::write(output_path, output.view());
    }
}