//
//  JSON.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 2018-12-24
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

/*
 ********************************************************************************************************
 *
 * # JSON
 *
 * This API is intended for use as an efficient reader_ for tools and plugins that output JSON with
 * relaxed syntax options (i.e. multiline strings, unquoted keys, no commas) such as ShaderCompiler that can't be
 * parsed using other libraries or API's such as RapidJSON. This should NOT be used to serialize/deserialize
 * standard JSON on disk - other libraries should be used for that purpose.
 *
 * ## Implementation details
 *
 * The parser is destructive - modifying the source string in-place to save copies. The AST is stored
 * as a linear buffer for cache locality as the major use of this API is iteration of nodes.
 * Memory is allocated using a stack allocator and can be a fixed size or dynamically growing. Object
 * member access is not constant time as it has to iterate over all members to find the right key. Array
 * access via `get_element` is constant time - Array's are stored alongside an 'offsets buffer' that
 * stores the byte offset of each array element AST node, as well as an element count, which means that
 * accessing an element consists of one array lookup to find the elements AST byte offset and another to
 * index into the AST and retrieve the node.
 *
 *********************************************************************************************************
 */

#pragma once

#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/JSON/Value.hpp"
#include "Bee/Core/String.hpp"


// TODO(Jacob):
//  - unicode support
//  - optimizations

namespace bee {
namespace json {


/*
 * Defines options for allowing different relaxed syntax requirements
 * and specifying the parsers allocation mode. By default each option
 * is set to be compliant with the JSON standard.
 */
struct ParseOptions {
    /*
     * Whether or not commas are required in objects and arrays to separate
     * elements.
     *
     * If set to false, the following JSON is allowed:
     *
     * {
     *   "name": "value"
     *   "name2": "value2"
     *   "array": [
     *      2
     *      4
     *      6
     *      1
     *    ]
     * }
     */
    bool            require_commas { true };

    /*
     * JSON requires that there be a single root value - an object, array,
     * string etc. If this is set to false, the root value is implied to be an
     * object and multiple top-level elements can exist which allows the following JSON:
     *
     * "name": "value",
     * "array": [1, 2, 5, "asd"],
     * "object": {
     *    "name": "object"
     * }
     *
     * whereas, if this was set to true (default) the above JSON would need to look like:
     *
     * {
     *   "name": "value",
     *   "array": [1, 2, 5, "asd"],
     *   "object": {
     *      "name": "object"
     *   }
     * }
     */
    bool            require_root_element { true };

    /*
     * The JSON standard requires that all keys be string values. If this is set
     * to false, that requirement is relaxed and keys can be naked identifiers
     * with the requirement that they match [a-zA-Z_][a-zA-A0-9_]*
     */
    bool            require_string_keys { true };

    /*
     * If set to true, single-line # comments are allowed, i.e:
     * {
     *   # this is a comment about the value
     *   "name": "value"
     * }
     */
    bool            allow_comments { false };

    /*
     * If set to true, multiline ''' raw strings are allowed, i.e:
     * {
     *   "code": '''
     *   int main(int argc, char** argv)
     *   {
     *     return 0;
     *   }
     *   '''
     * }
     * Escape characters are unsupported and not required inside these strings
     * as they're parsed as raw literals - considering everything between ''' pairs
     * to be part of the string
     */
    bool            allow_multiline_strings { false };

    /*
     * Determines the allocation mode the parser uses when allocating
     * memory for elements. By default this is set to dynamic - if fixed
     * allocation mode is used, `byte_capacity` must be assigned to the
     * internal buffers maximum memory capacity
     */
    AllocationMode  allocation_mode { AllocationMode::dynamic };

    // required to be set if `allocation_mode` is set to fixed
    i32             initial_capacity { 0 };
};


struct Cursor {
    i32             index { 0 };
    i32             source_size { 0 };
    const char*     current { nullptr };
    char*           source { nullptr };

    const char* operator++() noexcept;
    const char* operator++(int) noexcept;
    const char* operator--() noexcept;
    const char* operator--(int) noexcept;
    const char* operator+=(i32 value) noexcept;
    const char* operator-=(i32 value) noexcept;

    inline bool is_newline_or_eof()
    {
        return *current == '\n' || *current == '\0';
    }
};

enum class ErrorCode : i32 {
    none = 0,
    unexpected_character,
    expected_character,
    out_of_memory,
    expected_multiline_end,
    invalid_escape_sequence,
    number_missing_decimal,
    numer_invalid_exponent,
    invalid_allocation_data,
    expected_whitespace_separator
};


class BEE_API Document {
public:
    explicit Document(const ParseOptions& parse_options);
    Document(Document&& other) noexcept;
    Document& operator=(Document&& other) noexcept;

    bool parse(char* source);

    String get_error_string() const;

    inline ErrorCode get_error_code() const
    {
        return parse_error_.code;
    }

    inline ValueHandle root() const
    {
        return ValueHandle { 0 };
    }

    ValueType get_type(const ValueHandle& value) const;
    ValueData get_data(const ValueHandle& handle) const;

    /// Gets a handle to a member for a given key - has linear lookup time
    bool has_member(const ValueHandle& root, const char* key) const;
    ValueHandle get_member(const ValueHandle& root, const char* key) const;
    ValueType get_member_type(const ValueHandle& root, const char* key) const;
    ValueData get_member_data(const ValueHandle& root, const char* key) const;

    /// Gets an array element at the index given
    ValueHandle get_element(const ValueHandle& array, i32 index) const;
    ValueData get_element_data(const ValueHandle& array, const i32 index) const;

    // TODO(Jacob): need to allocate room for types of each value

    object_range_t get_members_range(const ValueHandle& root);

    const_object_range_t get_members_range(const ValueHandle& root) const;

    object_iterator get_members_iterator(const ValueHandle& root);

    const_object_iterator get_members_iterator(const ValueHandle& root) const;

    ArrayRangeAdapter get_elements_range(const ValueHandle& root);

    ArrayRangeAdapter get_elements_range(const ValueHandle& root) const;

    ArrayIterator get_elements_iterator(const ValueHandle& root);

    ArrayIterator get_elements_iterator(const ValueHandle& root) const;

    inline const ParseOptions& get_options() const
    {
        return options_;
    }

private:
    struct Error {
        i32         line { 0 };
        i32         column { 0 };
        ErrorCode   code { ErrorCode::none };
        char        current { '\0' };
        char        arg { '\0' };

        Error() = default;
        Error(ErrorCode error_code, const Cursor* cursor, char arg_char = '\0');
    };

    ParseOptions    options_;
    Error           parse_error_;
    ValueAllocator  allocator_;

    bool is_whitespace(char character);
    void skip_whitespace(Cursor* cursor);
    bool is_valid_unquoted_char(char character);
    bool is_quote(char character);
    bool advance_on_element_separator(Cursor* cursor);
    bool advance_on_char(Cursor* cursor, char character);

    bool parse_element(Cursor* cursor);
    bool parse_value(Cursor* cursor);
    bool parse_object(Cursor* cursor);
    bool parse_members(Cursor* cursor, char end_char);
    bool parse_member(Cursor* cursor);
    bool parse_array(Cursor* cursor);
    bool parse_string(Cursor* cursor);
    bool parse_multiline_string(Cursor* cursor);
    bool parse_number(Cursor* cursor);
    bool parse_true(Cursor* cursor);
    bool parse_false(Cursor* cursor);
    bool parse_null(Cursor* cursor);
};


void BEE_API write_to_string(String* dst, const Document& src_doc, i32 indent);


} // namespace json
} // namespace bee