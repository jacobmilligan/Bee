//
//  CommandLine.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 2018-12-13
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/CLI.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Debug.hpp"


namespace bee {
namespace cli {


Results::Results(Results&& other) noexcept
{
    move_construct(other);
}

Results& Results::operator=(Results&& other) noexcept
{
    move_construct(other);
    return *this;
}

void Results::move_construct(Results& other) noexcept
{
    argc = other.argc;
    argv = other.argv;
    success = other.success;
    help_requested = other.help_requested;
    positionals = std::move(other.positionals);
    options = std::move(other.options);
    program_name = std::move(other.program_name);
    help_string = std::move(other.help_string);
    error_message = std::move(other.error_message);
    argv_parsed_count = other.argv_parsed_count;

    other.argc = 0;
    other.argv = nullptr;
    other.success = false;
    other.help_requested = false;
    other.program_name = "";
    other.help_string = "";
    other.error_message = "";
    other.argv_parsed_count = 0;
}

i32 skip_dashes(const char* str)
{
    const auto str_len = str::length(str);
    if (str_len == 0)
    {
        return 0;
    }

    if (str[0] != '-')
    {
        return 0;
    }

    for (auto char_idx = 0; char_idx < str_len; ++char_idx)
    {
        if (str[char_idx] != '-')
        {
            return char_idx;
        }
    }

    return str_len - 1;
}

const Option* get_option(const char* long_name, const Option* options, const i32 options_count)
{
    for (int opt_idx = 0; opt_idx < options_count; ++opt_idx)
    {
        if (strcmp(long_name, options[opt_idx].long_name.c_str()) == 0)
        {
            return &options[opt_idx];
        }
    }

    return nullptr;
}

// Tries to parse the argument at `arg_idx` as an option (anything starting with -/--)
// and returns the new `arg_idx` after parsing all of it's option arguments.
// If an error was encountered, any the error results will be saved into `results`
i32 parse_option(
    const i32 arg_idx,
    const i32 argc,
    char** const argv,
    const Option* options,
    const i32 option_count,
    Results* results
)
{
    if (argv[arg_idx][0] != '-')
    {
        return arg_idx;
    }

    const auto name_pos = skip_dashes(argv[arg_idx]);
    if (name_pos == 0)
    {
        return arg_idx;
    }

    i32 found_option = -1;
    auto option_name_ptr = &argv[arg_idx][name_pos];
    // find an option that matches with either it's short name, i.e. -i, or it's long name, i.e
    // --option-name
    for (int opt_idx = 0; opt_idx < option_count; ++opt_idx)
    {
        if (options[opt_idx].short_name == '\0')
        {
            continue;
        }

        if (option_name_ptr[0] == options[opt_idx].short_name)
        {
            found_option = opt_idx;
            break;
        }

        if (strcmp(option_name_ptr, options[opt_idx].long_name.c_str()) == 0)
        {
            found_option = opt_idx;
            break;
        }
    }

    if (found_option < 0)
    {
        results->success = false;
        results->error_message = str::format("Invalid option: %s", argv[arg_idx]);
        return arg_idx;
    }

    // TODO(Jacob): this is O(n^2) which I'm not a big fan of, improve at some point?
    // It checks mutually-exclusive groups: options that this one excludes from being present on the command line
    for (auto& excluded : options[found_option].excludes)
    {
        for (int i = 0; i < argc; ++i)
        {
            if (str::length(argv[i]) < 2)
            {
                continue;
            }

            auto option = get_option(excluded.c_str(), options, option_count);
            if (option == nullptr)
            {
                continue;
            }

            if (strcmp(argv[i], option->long_name.c_str()) == 0 || argv[i][1] == option->short_name)
            {
                // found a mutually-exclusive option so return error
                results->success = false;
                results->error_message = str::format(
                    "Invalid combination of options: %s and %s are mutually-exclusive",
                    options[found_option].long_name.c_str(),
                    argv[i]
                );
                return arg_idx;
            }
        }
    }

    // get all the arguments passed in to the option, i.e. --file-to-read File1.txt File2.txt ...
    int opt_arg_idx = arg_idx + 1;
    for (; opt_arg_idx < argc; ++opt_arg_idx)
    {
        if (argv[opt_arg_idx][0] == '-')
        {
            break;
        }
    }

    const auto arg_count = (opt_arg_idx - 1) - arg_idx;
    // Only fail if the option requires at least one argument
    if (arg_count <= 0 && options[found_option].nargs != 0)
    {
        results->success = false;
        results->error_message = "Missing at least one argument for option: ";
        if (options[found_option].short_name != '\0')
        {
            io::write_fmt(&results->error_message, "-%c/", options[found_option].short_name);
        }
        io::write_fmt(&results->error_message, "--%s", options[found_option].long_name.c_str());
        return arg_idx;
    }

    if (options[found_option].nargs >= 0 && arg_count > options[found_option].nargs)
    {
        results->success = false;
        results->error_message = "Too many arguments supplied to ";
        if (options[found_option].short_name != '\0')
        {
            io::write_fmt(&results->error_message, "-%c/", options[found_option].short_name);
        }
        io::write_fmt(&results->error_message, "--%s", options[found_option].long_name.c_str());
        return arg_idx;
    }

    results->options.insert({ options[found_option].long_name, { arg_idx + 1, arg_count } });
    return opt_arg_idx;
}

i32 option_strlen(const Option& opt)
{
    auto opt_size = 2 + opt.long_name.size(); // 2 extra for '--'
    if (opt.short_name != '\0')
    {
        opt_size += 4; // 3 extra for the leading '-' and trailing ', '
    }
    return opt_size;
}

String make_help_string(
    const char* program_name,
    const Positional* positionals,
    const i32 positional_count,
    const Option* options,
    const i32 option_count
)
{
    constexpr auto min_line_width = 25;

    auto result = str::format("usage: %s ", program_name);

    if (positional_count > 0)
    {
        // print out positionals with specific amount of spacing, i.e
        // `program <positional1> <positional2> ...`
        for (int pos_idx = 0; pos_idx < positional_count; ++pos_idx)
        {
            io::write_fmt(&result, "<%s> ", positionals[pos_idx].name.c_str());
        }
    }

    // Print out all the required options
    for (int opt_idx = 0; opt_idx < option_count; ++opt_idx)
    {
        if (options[opt_idx].required)
        {
            io::write_fmt(&result, "-%c ", options[opt_idx].short_name);
        }
    }

    if (option_count > 0)
    {
        io::write_fmt(&result, "[options...]");
    }

    // figure out the longest positional and use that to pad all the others
    int longest_positional = 0;
    for (int pos_idx = 0; pos_idx < positional_count; ++pos_idx)
    {
        longest_positional = math::max(positionals[pos_idx].name.size(), longest_positional);
    }

    longest_positional = math::max(min_line_width, longest_positional + 2);

    if (positional_count > 0)
    {
        io::write_fmt(&result, "\n\nPositional arguments:\n");
        // print out positionals with specific amount of spacing, i.e `positional1  help string`
        for (int pos_idx = 0; pos_idx < positional_count; ++pos_idx)
        {
            io::write_fmt(&result, " %s", positionals[pos_idx].name.c_str());
            const auto max_spacing = longest_positional - positionals[pos_idx].name.size();
            for (int space_idx = 0; space_idx < max_spacing; ++space_idx)
            {
                io::write_fmt(&result, " ");
            }
            io::write_fmt(&result, "%s\n", positionals[pos_idx].help.c_str());
        }
    }

    // figure out the longest option and use that to pad all the others
    i32 longest_opt = 0;
    for (int opt_idx = 0; opt_idx < option_count; ++opt_idx)
    {
        longest_opt = math::max(option_strlen(options[opt_idx]), longest_opt);
    }

    longest_opt = math::max(min_line_width, longest_opt + 2);

    io::write_fmt(&result, "\nOptions:\n -h, --help");
    for (int i = 0; i < longest_opt - 10; ++i)
    {
        io::write_fmt(&result, " ");
    }

    io::write_fmt(&result, "Returns this help message\n");

    if (option_count > 0)
    {
        // print out options, i.e `-o, --option1  help string`
        for (int opt_idx = 0; opt_idx < option_count; ++opt_idx)
        {
            io::write_fmt(&result, " ");

            // Write out short name
            if (options[opt_idx].short_name != '\0')
            {
                io::write_fmt(&result, "-%c, ", options[opt_idx].short_name);
            }

            // Write long name
            io::write_fmt(&result, "--%s", options[opt_idx].long_name.c_str());

            const auto max_spacing = longest_opt - option_strlen(options[opt_idx]);
            for (int space_idx = 0; space_idx < max_spacing; ++space_idx)
            {
                io::write_fmt(&result, " ");
            }

            // Write help string
            io::write_fmt(&result, "%s\n", options[opt_idx].help.c_str());
        }
    }

    return result;
}

Results parse(
    const i32 argc,
    char** const argv,
    const Positional* positionals,
    const i32 positional_count,
    const Option* options,
    const i32 option_count
)
{
#if BEE_OS_WINDOWS == 1
    constexpr auto slash_char = '\\';
#else
    constexpr auto slash_char = '/';
#endif // BEE_OS_WINDOWS == 1

    auto last_slash_pos = 0;
    const auto prog_len = str::length(argv[0]);
    for (int char_idx = 0; char_idx < prog_len; ++char_idx)
    {
        if (argv[0][char_idx] == slash_char)
        {
            last_slash_pos = char_idx;
        }
    }

    Results results{};
    results.argc = argc;
    results.argv = argv;
    results.program_name = &argv[0][last_slash_pos + 1];
    results.help_string = make_help_string(
        results.program_name.c_str(),
        positionals,
        positional_count,
        options,
        option_count
    );
    results.argv_parsed_count = 1; // program name

    if (argc <= 1)
    {
        results.help_requested = true;
        return results;
    }

    // look for a help flag and return if found, so the calling code can print a help string
    for (int argv_idx = 0; argv_idx < argc; ++argv_idx)
    {
        if (strcmp(argv[argv_idx], "--help") == 0 || strcmp(argv[argv_idx], "-h") == 0)
        {
            results.help_requested = true;
            results.argv_parsed_count += 1;
            return results;
        }
    }

    while (results.argv_parsed_count < argc)
    {
        // A solitary '--' argument indicates the command line should stop parsing
        if (str::compare_n(argv[results.argv_parsed_count], "--", str::length(argv[results.argv_parsed_count])) == 0)
        {
            results.argv_parsed_count += 1;
            break;
        }

        // try and parse as an option, if it's not one then it's a positional or invalid
        const auto new_arg_idx = parse_option(results.argv_parsed_count, argc, argv, options, option_count, &results);
        if (new_arg_idx != results.argv_parsed_count)
        {
            results.argv_parsed_count = new_arg_idx;
            continue;
        }

        if (!results.success)
        {
            return results;
        }

        if (results.positionals.size() >= positional_count)
        {
            results.success = false;
            results.error_message = "Too many positionals specified";
            return results;
        }

        results.positionals.push_back({results.argv_parsed_count, 1});
        ++results.argv_parsed_count;
    }

    // Check all required options were present
    for (int opt_idx = 0; opt_idx < option_count; ++opt_idx)
    {
        if (!options[opt_idx].required)
        {
            continue;
        }

        auto missing_due_to_exclusion = false;

        if (results.options.find(options[opt_idx].long_name) == nullptr)
        {
            for (auto& excluded_opt : options[opt_idx].excludes)
            {
                if (results.options.find(excluded_opt) != nullptr)
                {
                    missing_due_to_exclusion = true;
                    break;
                }
            }

            if (!missing_due_to_exclusion)
            {
                results.success = false;
                results.error_message = str::format("Missing required option: %s", options[opt_idx].long_name.c_str());
                return results;
            }
        }
    }

    results.success = true;
    return results;
}


bool has_option(const Results& results, const char* option_long_name)
{
    return results.options.find(option_long_name) != nullptr;
}


const char* get_option(const Results& results, const char* option_long_name, const i32 arg_index)
{
    const auto token = results.options.find(option_long_name);
    if (BEE_FAIL(token != nullptr))
    {
        return "";
    }

    if (BEE_FAIL(arg_index < token->value.count))
    {
        return "";
    }

    return results.argv[token->value.index + arg_index];
}


} // namespace cli
} // namespace bee
