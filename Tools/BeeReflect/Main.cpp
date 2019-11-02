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


static constexpr unsigned char bee_reflect_magic[] = { 0x7C, 0xDD, 0x93, 0xB4 };
static constexpr bee::i32 bee_reflect_magic_size = sizeof(unsigned char) * bee::static_array_length(bee_reflect_magic);



namespace bee {


struct RegistrationEntry
{
    i32         size { 0 };
    u32         hash { 0 };
};


void write_registration_file(const Path& path, const Span<const Type* const>& types)
{
    bee::io::FileStream reg_file(path, "wb");

    const auto type_count = types.size();
    reg_file.write(bee_reflect_magic, bee_reflect_magic_size);
    reg_file.write(&type_count, sizeof(bee::i32));

    String type_format(temp_allocator());

    for (const bee::Type* type : types)
    {
        const auto name_length = str::length(type->name);
        RegistrationEntry entry{};
        entry.size = name_length + sizeof("get_type<>()") - 1;
        entry.hash = type->hash;
        reg_file.write(&entry, sizeof(RegistrationEntry));
        reg_file.write("get_type<");
        reg_file.write(type->name, name_length);
        reg_file.write(">()");
    }
}


bool read_registration_file(const Path& path, DynamicHashMap<u32, String>* types)
{
    static unsigned char magic[] = { 0x00, 0x00, 0x00, 0x00 };

    io::FileStream file(path, "rb");
    file.read(magic, bee_reflect_magic_size);

    if (memcmp(magic, bee_reflect_magic, static_array_length(bee_reflect_magic)) != 0)
    {
        log_error("%s has an invalid file format", path.c_str());
        return false;
    }

    i32 type_count = 0;
    file.read(&type_count, sizeof(i32));

    log_debug("[%s] Found %d types:", path.c_str(), type_count);

    if (type_count <= 0)
    {
        log_error("%s has no types", path.c_str());
        return false;
    }

    for (int type = 0; type < type_count; ++type)
    {
        RegistrationEntry entry{};
        file.read(&entry, sizeof(RegistrationEntry));
        if (entry.size <= 0 || entry.hash == 0)
        {
            log_error("%s: Invalid registration entry (size = %d, hash = %u)", path.c_str(), entry.size, entry.hash);
            return false;
        }

        String type_name(entry.size, '\0', temp_allocator());

        auto registered = types->find(entry.hash);

        if (registered != nullptr)
        {
            log_error("%s: type %s was linked twice - this is probably an error in bee-reflect", path.c_str(), registered->value.c_str());
            continue;
        }

        registered = types->insert(entry.hash, type_name);
        file.read(registered->value.data(), entry.size);

        log_debug("- %s (size: %d, hash: %u)", registered->value.c_str(), entry.size, entry.hash);
    }

    return true;
}


void find_registration_files(const Path& root, DynamicHashMap<u32, String>* types)
{
    for (const auto& path : fs::read_dir(root))
    {
        if (fs::is_dir(path))
        {
            find_registration_files(path, types);
            continue;
        }

        if (fs::is_file(path) && path.extension() == ".registration")
        {
            if (!read_registration_file(path, types))
            {
                log_error("Failed to read registration file: %s", path.c_str());
                continue;
            }
        }
    }
}


}


int main(int argc, const char** argv)
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
    clang::tooling::CommonOptionsParser options_parser(argc, argv, bee_reflect_cat);

    if (generate_subcommand)
    {
        clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

        const auto output_dir = bee::Path(output_dir_opt.c_str());
        if (!output_dir.exists())
        {
            bee::fs::mkdir(output_dir);
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

            auto output_path = output_dir.join(file.key.filename(), bee::temp_allocator())
                                         .append_extension("generated")
                                         .append_extension("cpp");
            bee::fs::write(output_path, output.view());

            const auto reg_path = output_dir.join(file.key.filename(), bee::temp_allocator()).set_extension("registration");
            bee::write_registration_file(reg_path, file.value.const_span());
        }

        return EXIT_SUCCESS;
    }

    if (link_subcommand)
    {
        bee::DynamicHashMap<bee::u32, bee::String> types;

        for (const std::string& src : options_parser.getSourcePathList())
        {
            const auto path = bee::Path(src.c_str(), bee::temp_allocator());
            if (!bee::fs::is_dir(path))
            {
                bee::log_error("%s is not a .registration file search path", path.c_str());
                continue;
            }

            bee::find_registration_files(path, &types);
        }

        bee::Path output_path(output_link_opt.getValue().c_str());
        bee::String output_string;
        bee::io::StringStream stream(&output_string);

        bee::CodeGenerator codegen(&stream);
        codegen.write_header_comment("bee-reflect linker");
        codegen.write("#include <Bee/Core/ReflectionV2.hpp>");
        codegen.newline();
        codegen.newline();
        codegen.write("namespace bee");
        codegen.scope([&]()
        {
            codegen.write("void reflection_init()");
            codegen.scope([&]()
            {
                codegen.write("static const Type* types[] =");
                codegen.scope([&]()
                {
                    for (const auto& type : types)
                    {
                        codegen.write_line("%s,", type.value.c_str());
                    }
                }, "; // types");
                codegen.newline();
                codegen.newline();
                codegen.write_line("reflection_register_builtin_types();");
                codegen.newline();
                codegen.write("for (const Type* type : types)");
                codegen.scope([&]()
                {
                    codegen.write("register_type(type);");
                });
            }, " // void reflection_init");
        }, " // namespace bee");

        bee::fs::write(output_path, output_string.view());
    }
}