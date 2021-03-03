/*
 *  CommandLine.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

namespace bee {
namespace cli {


struct Positional
{
    String     name;
    String     help;

    Positional() = default;

    Positional(const StringView& pos_name, const StringView& pos_help)
        : name(pos_name),
          help(pos_help)
    {}
};

struct Option
{
    Option() = default;

    Option(const char short_name_char, const char* long_name_str, const bool is_required, const char* help_str, const i32 arg_count)
        : short_name(short_name_char),
          long_name(long_name_str),
          required(is_required),
          help(help_str),
          nargs(arg_count)
    {}

    Option(const char short_name_char, const char* long_name_str, const bool is_required, const char* help_str, const i32 arg_count, const std::initializer_list<const char*>& mutually_exclusive_options)
        : Option(short_name_char, long_name_str, is_required, help_str, arg_count)
    {
        for (const auto& opt : mutually_exclusive_options)
        {
            excludes.emplace_back(opt);
        }
    }

    /*
     * number of arguments the option takes, a value of -1 indicates zero or more command_line and a value
     * of 0 indicates the option is just a flag with zero command_line
     */
    i32                    nargs { 0 };

    bool                   required { false };     // determines whether the option MUST be supplied
    char                   short_name { '\0' };    // for example: '-h'

    BEE_PAD(2);
    String                 long_name;              // for example: '--help'
    String                 help;                   // help string to print for the option

    // an array of the names of options this option is mutually exclusive with
    DynamicArray<String>   excludes;
};

struct Token
{
    i32 index { -1 };
    i32 count { 0 };
};

struct BEE_CORE_API Results : public Noncopyable
{
    Results() = default;

    Results(const char* dynamic_argv, Allocator* allocator);

    Results(Results&& other) noexcept;

    Results& operator=(Results&& other) noexcept;

    i32                                 argv_parsed_count { 0 };
    i32                                 argc { 0 };
    const char* const*                  argv { nullptr };
    const char*                         requested_help_string { nullptr };
    String                              program_name;
    String                              help_string;
    String                              error_message;
    DynamicArray<Token>                 positionals;
    DynamicHashMap<String, Token>       options;
    DynamicHashMap<String, Results>     subparsers;
    bool                                success { true };
    bool                                help_requested { false };
    BEE_PAD(6);

private:
    String                              dynamic_argv_;
    DynamicArray<char*>                 dynamic_argv_ptrs_;

    void move_construct(Results& other) noexcept;
};


struct BEE_CORE_API ParserDescriptor
{
    i32                     positional_count { 0 };
    i32                     option_count { 0 };
    i32                     subparser_count { 0 };
    BEE_PAD(4);
    const char*             command_name { nullptr }; // string used to invoke the command. Only used for subparsers
    const Positional*       positionals { nullptr };
    const Option*           options { nullptr };
    const ParserDescriptor* subparsers { nullptr };

    ParserDescriptor() = default;

    ParserDescriptor(
        const char* name,
        const i32 new_pos_count,
        const Positional* new_positionals,
        const i32 new_option_count,
        const Option* new_options,
        const i32 new_subparser_count = 0,
        const ParserDescriptor* new_subparsers = nullptr
    ) : command_name(name),
        positional_count(new_pos_count),
        positionals(new_positionals),
        option_count(new_option_count),
        options(new_options),
        subparser_count(new_subparser_count),
        subparsers(new_subparsers)
    {}

    template <i32 SubparserCount>
    ParserDescriptor(
        const char* name,
        const ParserDescriptor(&subparser_buffer)[SubparserCount]
    ) : command_name(name),
        subparser_count(SubparserCount),
        subparsers(subparser_buffer)
    {}

    template <i32 PositionalsSize>
    ParserDescriptor(
        const char* name,
        const Positional(&positionals_buffer)[PositionalsSize]
    ) : command_name(name),
        positional_count(PositionalsSize),
        positionals(positionals_buffer)
    {}

    template <i32 PositionalsSize, i32 SubparserCount>
    ParserDescriptor(
        const char* name,
        const Positional(&positionals_buffer)[PositionalsSize],
        const ParserDescriptor(&subparser_buffer)[SubparserCount]
    ) : command_name(name),
        positional_count(PositionalsSize),
        positionals(positionals_buffer),
        subparser_count(SubparserCount),
        subparsers(subparser_buffer)
    {}

    template <i32 OptionsSize>
    ParserDescriptor(
        const char* name,
        const Option(&optionals_buffer)[OptionsSize]
    ) : command_name(name),
        option_count(OptionsSize),
        options(optionals_buffer)
    {}

    template <i32 OptionsSize, i32 SubparserCount>
    ParserDescriptor(
        const char* name,
        const Option(&optionals_buffer)[OptionsSize],
        const ParserDescriptor(&subparser_buffer)[SubparserCount]
    ) : command_name(name),
        option_count(OptionsSize),
        options(optionals_buffer),
        subparser_count(SubparserCount),
        subparsers(subparser_buffer)
    {}

    template <i32 PositionalsSize, i32 OptionsSize>
    ParserDescriptor(
        const char* name,
        const Positional(&positionals_buffer)[PositionalsSize],
        const Option(&optionals_buffer)[OptionsSize]
    ) : command_name(name),
        positional_count(PositionalsSize),
        positionals(positionals_buffer),
        option_count(OptionsSize),
        options(optionals_buffer)
    {}

    template <i32 PositionalsSize, i32 OptionsSize, i32 SubparserCount>
    ParserDescriptor(
        const char* name,
        const Positional(&positionals_buffer)[PositionalsSize],
        const Option(&optionals_buffer)[OptionsSize],
        const ParserDescriptor(&subparser_buffer)[SubparserCount]
    ) : command_name(name),
        positional_count(PositionalsSize),
        positionals(positionals_buffer),
        option_count(OptionsSize),
        options(optionals_buffer),
        subparser_count(SubparserCount),
        subparsers(subparser_buffer)
    {}
};


BEE_CORE_API Results parse(i32 argc, char** argv, const ParserDescriptor& desc);

BEE_CORE_API Results parse(i32 argc, const char** argv, const ParserDescriptor& desc);

BEE_CORE_API Results parse(const char* program_name, const char* command_line, const ParserDescriptor& desc, Allocator* allocator = system_allocator());

BEE_CORE_API bool has_option(const Results& results, const char* option_long_name);

BEE_CORE_API const char* get_option(const Results& results, const char* option_long_name, i32 arg_index = 0);

BEE_CORE_API i32 get_option_count(const Results& results, const char* option_long_name);

BEE_CORE_API const char* get_positional(const Results& results, const i32 positional_index);

BEE_CORE_API i32 get_remainder_count(const Results& results);

BEE_CORE_API const char* const* get_remainder(const Results& results);


} // namespace cli
} // namespace bee