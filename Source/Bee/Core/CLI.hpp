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


    char                   short_name { '\0' };    // for example: '-h'
    String                 long_name;              // for example: '--help'
    bool                   required { false };     // determines whether the option MUST be supplied
    String                 help;                   // help string to print for the option

    /*
     * number of arguments the option takes, a value of -1 indicates zero or more command_line and a value
     * of 0 indicates the option is just a flag with zero command_line
     */
    i32                    nargs { 0 };
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

    Results(Results&& other) noexcept;

    Results& operator=(Results&& other) noexcept;

    i32                                 argc { 0 };
    const char* const*                  argv { nullptr };
    bool                                success { true };
    bool                                help_requested { false };
    const char*                         requested_help_string { nullptr };
    i32                                 argv_parsed_count { 0 };
    DynamicArray<Token>                 positionals;
    DynamicHashMap<String, Token>       options;
    DynamicHashMap<String, Results>     subparsers;
    String                              program_name;
    String                              help_string;
    String                              error_message;

private:
    void move_construct(Results& other) noexcept;
};


struct BEE_CORE_API ParserDescriptor
{
    const char*         command_name { nullptr }; // string used to invoke the command. Only used for subparsers
    i32                 positional_count { 0 };
    const Positional*   positionals { nullptr };
    i32                 option_count { 0 };
    const Option*       options { nullptr };
    i32                 subparser_count { 0 };
    ParserDescriptor*   subparsers { nullptr };
};


BEE_CORE_API Results parse(i32 argc, char** argv, const ParserDescriptor& desc);

BEE_CORE_API bool has_option(const Results& results, const char* option_long_name);

BEE_CORE_API const char* get_option(const Results& results, const char* option_long_name, i32 arg_index = 0);

BEE_CORE_API const char* get_positional(const Results& results, const i32 positional_index);

BEE_CORE_API i32 get_remainder_count(const Results& results);

BEE_CORE_API const char* const* get_remainder(const Results& results);


} // namespace cli
} // namespace bee