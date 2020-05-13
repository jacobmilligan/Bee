/*
 *  CommandLine.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/CLI.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Debug.hpp"

namespace bee {
namespace cli {


Results::Results(const char* dynamic_argv, Allocator* allocator)
    : dynamic_argv_(dynamic_argv, allocator),
      dynamic_argv_ptrs_(allocator)
{
    int offset = 0;
    char* cur_ptr = dynamic_argv_.data();

    auto skip_whitespace = [&]()
    {
        for (; offset < dynamic_argv_.size(); ++offset)
        {
            if (!str::is_space(dynamic_argv_[offset]))
            {
                break;
            }
        }
    };

    for (; offset < dynamic_argv_.size(); ++offset)
    {
        // Process strings
        if (dynamic_argv_[offset] == '"')
        {
            for (; offset < dynamic_argv_.size(); ++offset)
            {
                if (dynamic_argv_[offset] == '"')
                {
                    break;
                }
            }
        }

        if (str::is_space(dynamic_argv_[offset]))
        {
            dynamic_argv_[offset] = '\0';
            dynamic_argv_ptrs_.push_back(cur_ptr);
            ++offset;
            skip_whitespace();
            cur_ptr = dynamic_argv_.data() + offset;
        }
    }

    dynamic_argv_ptrs_.push_back(cur_ptr);
    argv = dynamic_argv_ptrs_.data();
    argc = dynamic_argv_ptrs_.size();
}

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
    subparsers = std::move(other.subparsers);
    program_name = std::move(other.program_name);
    help_string = std::move(other.help_string);
    requested_help_string = other.requested_help_string;
    error_message = std::move(other.error_message);
    argv_parsed_count = other.argv_parsed_count;
    dynamic_argv_ = std::move(other.dynamic_argv_);
    dynamic_argv_ptrs_ = std::move(other.dynamic_argv_ptrs_);

    other.argc = 0;
    other.argv = nullptr;
    other.success = false;
    other.help_requested = false;
    other.program_name = "";
    other.help_string = "";
    other.error_message = "";
    other.argv_parsed_count = 0;
    other.requested_help_string = nullptr;
}

void set_result_error(Results* results, const String& msg)
{
    results->success = false;
    results->error_message = msg;
}

void request_help(Results* results)
{
    results->help_requested = true;
    results->requested_help_string = results->help_string.c_str();
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

/*
 * Tries to parse the argument at `arg_idx` as an option (anything starting with -/--)
 * and returns the new `arg_idx` after parsing all of it's option arguments.
 * If an error was encountered, any the error results will be saved into `results`. -1 or results->success == false
 * indicates an error
 */
i32 parse_option(
    const i32 arg_idx,
    const i32 argc,
    char** const argv,
    const Option* options,
    const i32 option_count,
    Results* results
)
{
    if (option_count > 0 && options == nullptr)
    {
        set_result_error(results, "internal error: specified > 0 options but options list was nullptr");
        return -1;
    }

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
        set_result_error(results, str::format("Invalid option: %s", argv[arg_idx]));
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
                set_result_error(results, str::format(
                    "Invalid combination of options: %s and %s are mutually-exclusive",
                    options[found_option].long_name.c_str(),
                    argv[i]
                ));
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
        set_result_error(results, "Missing at least one argument for option: ");
        if (options[found_option].short_name != '\0')
        {
            io::write_fmt(&results->error_message, "-%c/", options[found_option].short_name);
        }
        io::write_fmt(&results->error_message, "--%s", options[found_option].long_name.c_str());
        return arg_idx;
    }

    if (options[found_option].nargs >= 0 && arg_count > options[found_option].nargs)
    {
        set_result_error(results, "Too many arguments supplied to ");
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

String make_help_string(const char* program_name, const ParserDescriptor& desc)
{
    constexpr auto min_line_width = 25;

    auto result = str::format("usage: %s ", program_name);

    if (desc.command_name != nullptr)
    {
        io::write_fmt(&result, "%s ", desc.command_name);
    }

    if (desc.subparser_count > 0)
    {
        result.append("<command> ");
    }

    if (desc.positional_count > 0)
    {
        // print out positionals with specific amount of spacing, i.e
        // `program <positional1> <positional2> ...`
        for (int pos_idx = 0; pos_idx < desc.positional_count; ++pos_idx)
        {
            io::write_fmt(&result, "<%s> ", desc.positionals[pos_idx].name.c_str());
        }
    }

    // Print out all the required options
    for (int opt_idx = 0; opt_idx < desc.option_count; ++opt_idx)
    {
        if (desc.options[opt_idx].required)
        {
            io::write_fmt(&result, "-%c ", desc.options[opt_idx].short_name);
        }
    }

    if (desc.option_count > 0)
    {
        io::write_fmt(&result, "[options...]");
    }

    // figure out the longest positional and use that to pad all the others
    int longest_positional = 0;
    for (int pos_idx = 0; pos_idx < desc.positional_count; ++pos_idx)
    {
        longest_positional = math::max(desc.positionals[pos_idx].name.size(), longest_positional);
    }

    longest_positional = math::max(min_line_width, longest_positional + 2);

    if (desc.positional_count > 0)
    {
        io::write_fmt(&result, "\n\nPositional arguments:\n");
        // print out positionals with specific amount of spacing, i.e `positional1  help string`
        for (int pos_idx = 0; pos_idx < desc.positional_count; ++pos_idx)
        {
            io::write_fmt(&result, " %s", desc.positionals[pos_idx].name.c_str());
            const auto max_spacing = longest_positional - desc.positionals[pos_idx].name.size();
            for (int space_idx = 0; space_idx < max_spacing; ++space_idx)
            {
                io::write_fmt(&result, " ");
            }
            io::write_fmt(&result, "%s\n", desc.positionals[pos_idx].help.c_str());
        }
    }

    // figure out the longest option and use that to pad all the others
    i32 longest_opt = 0;
    for (int opt_idx = 0; opt_idx < desc.option_count; ++opt_idx)
    {
        longest_opt = math::max(option_strlen(desc.options[opt_idx]), longest_opt);
    }

    longest_opt = math::max(min_line_width, longest_opt + 2);

    io::write_fmt(&result, "\nOptions:\n -h, --help");
    for (int i = 0; i < longest_opt - 10; ++i)
    {
        io::write_fmt(&result, " ");
    }

    io::write_fmt(&result, "Returns this help message\n");

    if (desc.option_count > 0)
    {
        // print out options, i.e `-o, --option1  help string`
        for (int opt_idx = 0; opt_idx < desc.option_count; ++opt_idx)
        {
            io::write_fmt(&result, " ");

            // Write out short name
            if (desc.options[opt_idx].short_name != '\0')
            {
                io::write_fmt(&result, "-%c, ", desc.options[opt_idx].short_name);
            }

            // Write long name
            io::write_fmt(&result, "--%s", desc.options[opt_idx].long_name.c_str());

            const auto max_spacing = longest_opt - option_strlen(desc.options[opt_idx]);
            for (int space_idx = 0; space_idx < max_spacing; ++space_idx)
            {
                io::write_fmt(&result, " ");
            }

            // Write help string
            io::write_fmt(&result, "%s\n", desc.options[opt_idx].help.c_str());
        }
    }

    if (desc.subparser_count > 0)
    {
        io::write_fmt(&result, "\nCommands:\n");

        for (int cmd_idx = 0; cmd_idx < desc.subparser_count; ++cmd_idx)
        {
            io::write_fmt(&result, "%s", desc.subparsers[cmd_idx].command_name);

            if (cmd_idx < desc.subparser_count - 1)
            {
                io::write_fmt(&result, ", ");
            }
        }
    }

    return result;
}

void parse_recursive(const char* prog_name, const i32 argc, char** const argv, const ParserDescriptor& desc, Results* results)
{
    results->argc = argc;
    results->argv = argv;
    results->program_name = prog_name;
    results->help_string = make_help_string(prog_name, desc);
    results->argv_parsed_count = 0; // program name

    if (argc <= 0)
    {
        request_help(results);
        return;
    }

    // Subparsers have precedence over the root command - see if the first command after this one is a subparser
    if (desc.subparser_count > 0)
    {
        if (desc.subparsers == nullptr)
        {
            set_result_error(results, "internal error: specified > 0 subparsers but subparsers array was nullptr");
            return;
        }

        int found_subparser = -1;
        for (int subparser = 0; subparser < desc.subparser_count; ++subparser)
        {
            if (str::compare(argv[0], desc.subparsers[subparser].command_name) == 0)
            {
                // Found a subparser
                found_subparser = subparser;
                break;
            }
        }

        // Process the subparser and not the rest of the command line
        if (found_subparser >= 0)
        {
            auto subparser_results = results->subparsers.insert(String(desc.subparsers[found_subparser].command_name), Results());
            parse_recursive(prog_name, argc - 1, argv + 1, desc.subparsers[found_subparser], &subparser_results->value);
            results->requested_help_string = subparser_results->value.requested_help_string;
            results->help_requested = subparser_results->value.help_requested;
            results->argv_parsed_count += subparser_results->value.argv_parsed_count;
            return;
        }
    }

    // look for a help flag and return if found, so the calling code can print a help string
    for (int argv_idx = 0; argv_idx < argc; ++argv_idx)
    {
        if (strcmp(argv[argv_idx], "--help") == 0 || strcmp(argv[argv_idx], "-h") == 0)
        {
            request_help(results);
            results->argv_parsed_count += 1;
            return;
        }
    }

    // No subparsers so we can process this command line normally
    while (results->argv_parsed_count < argc)
    {
        // A solitary '--' argument indicates the command line should stop parsing
        if (str::compare_n(argv[results->argv_parsed_count], "--", str::length(argv[results->argv_parsed_count])) == 0)
        {
            results->argv_parsed_count += 1;
            break;
        }

        // try and parse as an option, if it's not one then it's a positional or invalid
        const auto new_arg_idx = parse_option(results->argv_parsed_count, argc, argv, desc.options, desc.option_count, results);
        if (new_arg_idx >= 0 && new_arg_idx != results->argv_parsed_count)
        {
            results->argv_parsed_count = new_arg_idx;
            continue;
        }

        if (!results->success || new_arg_idx < 0)
        {
            return;
        }

        if (results->positionals.size() >= desc.positional_count)
        {
            set_result_error(results, "Too many positionals specified");
            return;
        }

        results->positionals.push_back({results->argv_parsed_count, 1});
        ++results->argv_parsed_count;
    }

    // Check all required options were present
    for (int opt_idx = 0; opt_idx < desc.option_count; ++opt_idx)
    {
        if (!desc.options[opt_idx].required)
        {
            continue;
        }

        auto missing_due_to_exclusion = false;

        if (results->options.find(desc.options[opt_idx].long_name) == nullptr)
        {
            for (auto& excluded_opt : desc.options[opt_idx].excludes)
            {
                if (results->options.find(excluded_opt) != nullptr)
                {
                    missing_due_to_exclusion = true;
                    break;
                }
            }

            if (!missing_due_to_exclusion)
            {
                set_result_error(results, str::format("Missing required option: %s", desc.options[opt_idx].long_name.c_str()));
                return;
            }
        }
    }

    results->success = true;
}

Results parse(const i32 argc, char** const argv, const ParserDescriptor& desc)
{
#if BEE_OS_WINDOWS == 1
    constexpr auto slash_char = '\\';
#else
    constexpr auto slash_char = '/';
#endif // BEE_OS_WINDOWS == 1

    const auto last_slash_pos = str::last_index_of(argv[0], slash_char);
    const char* prog_name = &argv[0][last_slash_pos + 1];

    Results results{};
    parse_recursive(prog_name, argc - 1, argv + 1, desc, &results);
    ++results.argv_parsed_count; // program name
    return results;
}

Results parse(const char* program_name, const char* command_line, const ParserDescriptor& desc, Allocator* allocator)
{
    Results results(command_line, allocator);
    auto argv = const_cast<char**>(results.argv);
    parse_recursive(program_name, results.argc, argv, desc, &results);
    return results;
}


bool has_option(const Results& results, const char* option_long_name)
{
    return results.options.find(option_long_name) != nullptr;
}

const char* get_positional(const Results& results, const i32 positional_index)
{
    if (BEE_FAIL(positional_index < results.positionals.size()))
    {
        return "";
    }

    return results.argv[results.positionals[positional_index].index];
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

i32 get_remainder_count(const Results& results)
{
    return results.argc - results.argv_parsed_count;
}

const char* const* get_remainder(const Results& results)
{
    static constexpr const char* empty_remainder = "";

    BEE_ASSERT(results.argv_parsed_count <= results.argc);
    if (results.argv_parsed_count == results.argc)
    {
        return &empty_remainder;
    }

    return results.argv + results.argv_parsed_count;
}


} // namespace cli
} // namespace bee
