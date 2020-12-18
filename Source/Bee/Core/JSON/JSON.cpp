/*
 *  JSON.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Enum.hpp"
#include "Bee/Core/JSON/JSON.hpp"
#include "Bee/Core/IO.hpp"

#include <string.h>
#include <float.h> // for DBL_DECIMAL_DIG

namespace bee {
namespace json {


BEE_TRANSLATION_TABLE(get_type_name, ValueType, const char*, ValueType::unknown,
    "object",
    "array",
    "string",
    "number",
    "boolean",
    "null"
)

const char* Cursor::operator++() noexcept
{
    ++index;
    if (index >= source_size)
    {
        current = "\0";
        index = source_size;
    }
    else
    {
        ++current;
    }
    return current;
}

const char* Cursor::operator++(int) noexcept
{
    const auto old_char = current;
    ++*this;
    return old_char;
}

const char* Cursor::operator--() noexcept
{
    --index;
    if (index < 0)
    {
        index = 0;
        current = source;
    }
    else
    {
        --current;
    }
    return current;
}

const char* Cursor::operator--(int) noexcept
{
    const auto old_char = current;
    --*this;
    return old_char;
}

const char* Cursor::operator+=(const i32 value) noexcept
{
    index += value;
    if (index >= source_size)
    {
        index = source_size;
        current = "\0";
    }
    else
    {
        current += value;
    }
    return current;
}

const char* Cursor::operator-=(const i32 value) noexcept
{
    index -= value;
    if (index < 0)
    {
        index = 0;
        current = "\0";
    }
    else
    {
        current -= value;
    }
    return current;
}

Document::Error::Error(const ErrorCode error_code, const Cursor* cursor, const char arg_char)
    : code(error_code),
      current(*cursor->current),
      arg(arg_char)
{
    column = 1;
    line = 1;

    for (int i = 0; i < cursor->index; ++i)
    {
        if (cursor->source[i] == '\n')
        {
            column = 1;
            ++line;
            continue;
        }

        ++column;
    }
}


Document::Document(const ParseOptions& parse_options)
    : options_(parse_options),
      allocator_(parse_options.allocation_mode, parse_options.initial_capacity)
{}

Document::Document(Document&& other) noexcept
    : options_(other.options_),
      parse_error_(other.parse_error_),
      allocator_(BEE_MOVE(other.allocator_))
{
    other.options_ = ParseOptions{};
    other.parse_error_ = Error{};
}

Document& Document::operator=(Document&& other) noexcept
{
    options_ = other.options_;
    parse_error_ = other.parse_error_;
    allocator_ = BEE_MOVE(other.allocator_);

    other.options_ = ParseOptions{};
    other.parse_error_ = Error{};

    return *this;
}

String Document::get_error_string() const
{
    if (parse_error_.code == ErrorCode::none)
    {
        return "JSON parse success";
    }

    auto error = str::format("JSON parse error at: %d:%d: ", parse_error_.line, parse_error_.column);

    switch (parse_error_.code) {
        case ErrorCode::unexpected_character:
        {
            io::write_fmt(&error, "unexpected character '%c'", parse_error_.current);
            break;
        }
        case ErrorCode::expected_character:
        {
            io::write_fmt(&error, "unexpected character '%c'. Expected '%c' instead", parse_error_.current, parse_error_.arg);
            break;
        }
        case ErrorCode::out_of_memory:
        {
            io::write_fmt(&error, "unable to allocate memory for JSON value - out of memory");
            break;
        }
        case ErrorCode::expected_multiline_end:
        {
            io::write_fmt(&error, "expected to see a multiline end sequence (''')");
            break;
        }
        case ErrorCode::invalid_escape_sequence:
        {
            io::write_fmt(
                &error,
                "invalid escape sequence. expected one of '\\', '/', '\\n', '\\b', '\\f', '\\r', '\\t', '\\u' "
                "but found '\\%c' instead",
                parse_error_.arg
            );
            break;
        }
        case ErrorCode::number_missing_decimal:
        {
            io::write_fmt(&error, "found '.' but number was missing a decimal part");
            break;
        }
        case ErrorCode::numer_invalid_exponent:
        {
            io::write_fmt(&error, "found 'e' or 'E' but number was missing an exponent part");
            break;
        }
        case ErrorCode::invalid_allocation_data:
        {
            io::write_fmt(&error, "value allocation data was corrupt or invalid");
            break;
        }
        case ErrorCode::expected_whitespace_separator:
        {
            io::write_fmt(
                &error,
                "expected whitespace character for member or element separator (`require_commas` == false) "
                "but found '%c' instead",
                parse_error_.current
            );
            break;
        }
        default:
        {
            io::write_fmt(&error, "unknown error");
        }
    }

    return error;
}


/*
 * Parsing functions
 */

bool Document::parse(char* source)
{
    allocator_.reset();

    Cursor cursor{};
    cursor.index = 0;
    cursor.source_size = sign_cast<i32>(str::length(source));
    cursor.source = source;
    cursor.current = source;

    if (options_.require_root_element)
    {
        return parse_element(&cursor) && advance_on_char(&cursor, '\0');
    }

    // create an implicit root object
    const auto object_handle = allocator_.allocate(ValueType::object, nullptr, 0);
    const auto old_size = allocator_.size();

    if (!object_handle.is_valid())
    {
        parse_error_ = Error(ErrorCode::out_of_memory, &cursor);
        return false;
    }

    // parse the members as if the root exists
    if (!parse_members(&cursor, '\0'))
    {
        return false;
    }

    const auto object_data = allocator_.get(object_handle);
    if (BEE_FAIL(object_data->is_valid()))
    {
        parse_error_ = Error(ErrorCode::invalid_allocation_data, &cursor);
        return false;
    }

    // set object byte size
    object_data->contents.integer_value = allocator_.size() - old_size;
    return true;
}

bool Document::is_whitespace(char character)
{
    return str::is_space(character) || (options_.allow_comments && character == '#');
}

void Document::skip_whitespace(Cursor* cursor)
{
    while (is_whitespace(*cursor->current))
    {
        if (*cursor->current == '#' && options_.allow_comments)
        {
            while (!cursor->is_newline_or_eof())
            {
                ++(*cursor);
            }
            continue;
        }

        ++(*cursor);
    }
}

// valid as long as it matches [^,:[]{}\s]
bool Document::is_valid_unquoted_char(char character)
{
    switch (character)
    {
        case ',': case ':': case '[': case ']': case '{': case '}': case '\0': return false;
        default: return !is_whitespace(character);
    }
}

bool Document::is_quote(char character)
{
    return character == '"' || character == '\'';
}

bool Document::advance_on_char(Cursor* cursor, char character)
{
    if (*cursor->current == character)
    {
        ++(*cursor);
        return true;
    }

    parse_error_ = Error(ErrorCode::expected_character, cursor, character);
    return false;
}

bool Document::advance_on_element_separator(Cursor* cursor)
{
    if (options_.require_commas)
    {
        return advance_on_char(cursor, ',');
    }

    const auto previous_is_whitespace = cursor->index > 0 && is_whitespace(cursor->source[cursor->index - 1]);
    const auto current_is_whitespace = is_whitespace(*cursor->current);

    if (previous_is_whitespace || current_is_whitespace)
    {
        skip_whitespace(cursor);
        return true;
    }

    parse_error_ = Error(ErrorCode::expected_whitespace_separator, cursor);
    return false;
}

bool Document::parse_element(Cursor* cursor)
{
    skip_whitespace(cursor);
    if (!parse_value(cursor))
    {
        return false;
    }
    skip_whitespace(cursor);
    return true;
}

bool Document::parse_value(Cursor* cursor)
{
    switch (*cursor->current)
    {
        case '{':
            return parse_object(cursor);
        case '[':
            return parse_array(cursor);
        case '"':
        case '\'':
            return parse_string(cursor);
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-':
            return parse_number(cursor);
        case 't':
            return parse_true(cursor);
        case 'f':
            return parse_false(cursor);
        case 'n':
            return parse_null(cursor);
        default:
            break;
    }

    parse_error_ = Error(ErrorCode::unexpected_character, cursor);
    return false;
}

bool Document::parse_object(Cursor* cursor)
{
    if (!advance_on_char(cursor, '{'))
    {
        return false;
    }

    skip_whitespace(cursor);

    const auto object_handle = allocator_.allocate(ValueType::object, nullptr, 0);
    const auto old_size = allocator_.size();

    if (!object_handle.is_valid())
    {
        parse_error_ = Error(ErrorCode::out_of_memory, cursor);
        return false;
    }

    auto valid = true;
    if (*cursor->current != '}')
    {
        // non-empty object
        valid = parse_members(cursor, '}');
    }

    if (!valid)
    {
        return false;
    }

    const auto object_data = allocator_.get(object_handle);
    if (BEE_FAIL(object_data->is_valid()))
    {
        parse_error_ = Error(ErrorCode::invalid_allocation_data, cursor);
        return false;
    }

    if (!advance_on_char(cursor, '}'))
    {
        return false;
    }

    // set object byte size
    object_data->contents.integer_value = allocator_.size() - old_size;
    return true;
}

bool Document::parse_members(Cursor* cursor, const char end_char)
{
    while (cursor->index < cursor->source_size)
    {
        if (!parse_member(cursor))
        {
            return false;
        }

        if (*cursor->current == end_char)
        {
            break;
        }

        // TODO(Jacob): might need to double-check here that there's valid whitespace separating
        //  elements
        if (!advance_on_element_separator(cursor))
        {
            return false;
        }
    }

    return true;
}

bool Document::parse_member(Cursor* cursor)
{
    skip_whitespace(cursor);

    // ProcessHandle quoted keys normally even if `require_string_keys` is on
    if (options_.require_string_keys || (!options_.require_string_keys && is_quote(*cursor->current)))
    {
        // parse member key as string ws ':'
        if (!parse_string(cursor))
        {
            return false;
        }

        skip_whitespace(cursor);
        if (!advance_on_char(cursor, ':'))
        {
            return false;
        }
    }
    else
    {
        // parse member key as valid_unquoted_char+ ws ':'
        const auto str_ptr = cursor->current;
        while (is_valid_unquoted_char(*cursor->current))
        {
            ++(*cursor);
        }

        skip_whitespace(cursor);

        if (*cursor->current != ':')
        {
            parse_error_ = Error(ErrorCode::expected_character, cursor, ':');
            return false;
        }

        cursor->source[cursor->index] = '\0';
        ++(*cursor);

        if (!allocator_.allocate(ValueType::string, &str_ptr, sizeof(char*)).is_valid())
        {
            parse_error_ = Error(ErrorCode::out_of_memory, cursor);
            return false;
        }
    }

    return parse_element(cursor);
}

bool Document::parse_string(Cursor* cursor)
{
    if (*cursor->current != '"' && *cursor->current != '\'')
    {
        return false;
    }

    if (*cursor->current == '\'')
    {
        return parse_multiline_string(cursor);
    }

    if (!advance_on_char(cursor, '"'))
    {
        return false;
    }

    auto str_begin_ptr = cursor->current;
    auto str_end_ptr = cursor->source + cursor->index;

    while (cursor->index < cursor->source_size)
    {
        // Ensure that any literal newlines, line feeds etc. control characters
        // are parsed as an error in a string (must be in a single line)
        if (*cursor->current < 0x20)
        {
            parse_error_ = Error(ErrorCode::unexpected_character, cursor);
            return false;
        }

        // end of string
        if (*cursor->current == '"')
        {
            // null-terminate the string sequence
            *str_end_ptr = '\0';
            ++(*cursor);
            break;
        }

        ++(*cursor);

        // normal string content - add to current string
        if (cursor->source[cursor->index - 1] != '\\')
        {
            *str_end_ptr++ = cursor->source[cursor->index - 1];
            continue;
        }

        // TODO(Jacob): handle unicode sequences

        // escaped char sequence - advance offset but retain current string position to move in-place and replace with
        // unescaped character
        char unescaped_char;
        switch (*cursor->current)
        {
            case '"': unescaped_char = '"'; break;
            case '\\': unescaped_char = '\\'; break;
            case '/': unescaped_char = '/'; break;
            case 'n': unescaped_char = '\n'; break;
            case 'b': unescaped_char = '\b'; break;
            case 'f': unescaped_char = '\f'; break;
            case 'r': unescaped_char = '\r'; break;
            case 't': unescaped_char = '\t'; break;
            case 'u':
            default:
                parse_error_ = Error(ErrorCode::invalid_escape_sequence, cursor, *cursor->current);
                return false;
        }

        // advance past the escaped char
        ++(*cursor);
        *str_end_ptr++ = unescaped_char;
    }

    // allow for null-terminated part
    if (!allocator_.allocate(ValueType::string, &str_begin_ptr, sizeof(char*)).is_valid())
    {
        parse_error_ = Error(ErrorCode::out_of_memory, cursor);
        return false;
    }

    return true;
}

bool Document::parse_multiline_string(Cursor* cursor)
{
    if (!options_.allow_multiline_strings)
    {
        parse_error_ = Error(ErrorCode::unexpected_character, cursor);
        return false;
    }

    for (int i = 0; i < 3; ++i)
    {
        if (!advance_on_char(cursor, '\''))
        {
            return false;
        }
    }

    const auto begin = cursor->current;
    bool is_multiline_end = false;

    while (cursor->index < cursor->source_size && *cursor->current != '\0')
    {
        if (*cursor->current == '\'')
        {
            /*
             * try advance 3 quotes to find end of multiline and break
             * out of while loop if we find this sequence
             */
            is_multiline_end = cursor->index < cursor->source_size - 2
                && cursor->source[cursor->index + 1] == '\''
                && cursor->source[cursor->index + 2] == '\'';

            if (is_multiline_end)
            {
                break;
            }
        }

        ++(*cursor);
    }

    if (!is_multiline_end)
    {
        parse_error_ = Error(ErrorCode::expected_multiline_end, cursor);
        return false;
    }

    cursor->source[cursor->index] = '\0';

    if (!allocator_.allocate(ValueType::string, &begin, sizeof(char*)).is_valid())
    {
        parse_error_ = Error(ErrorCode::out_of_memory, cursor);
        return false;
    }

    (*cursor) += 3; // advance past the '''
    return true;
}


/*
 * Array's are parsed by first adding an `array` AST node, then parsing all child nodes linearly.
 * Once all child nodes are parsed, we reserve space for a buffer to store the byte offset of each of the
 * array's elements from the start of the allocator's memory to allow for constant-time lookup. At this
 * point, the AST will look like this in memory:
 *
 * | array header | child 0 | child 1 | ... | child N | uninitialized offset 0 | uninitialized offset 1 | ... | uninitialized offset N |
 *
 * Then, we move the offsets so they're stored alongside the array's root node by `memmove`ing the
 * children N bytes right and setting each of the offset elements so that the final AST looks like this:
 *
 * | array header | byte offset 0 | byte offset 1 | ... | byte offset N | child 0 | child 1 | ... | child N |
 */
bool Document::parse_array(Cursor* cursor)
{
    if (!advance_on_char(cursor, '['))
    {
        return false;
    }

    const auto old_size = allocator_.size();
    const auto array_handle = allocator_.allocate(ValueType::array, nullptr, 0);

    if (!array_handle.is_valid())
    {
        parse_error_ = Error(ErrorCode::out_of_memory, cursor);
        return false;
    }

    auto element_count = 0;
    if (*cursor->current != ']')
    {
        // non-empty array, parse elements
        while (cursor->index < cursor->source_size)
        {
            if (!parse_element(cursor))
            {
                return false;
            }

            ++element_count;
            if (*cursor->current == ']')
            {
                break;
            }

            if (!advance_on_element_separator(cursor))
            {
                return false;
            }
        }
    }

    if (!advance_on_char(cursor, ']'))
    {
        parse_error_ = Error(ErrorCode::expected_character, cursor, ']');
        return false;
    }

    auto array_data = allocator_.get(array_handle);
    if (BEE_FAIL(array_data->is_valid()))
    {
        parse_error_ = Error(ErrorCode::invalid_allocation_data, cursor);
        return false;
    }

    BEE_ASSERT_F(allocator_.size() >= old_size, "ValueAllocator: size somehow shrunk");

    // Add an extra element at the start of the offsets buffer to store the total element count
    const i32 offsets_size = (element_count + 1) * sizeof(i32);
    const i32 elements_size = allocator_.size() - old_size - sizeof(ValueData);
    const i32 elements_src = array_handle.id + sizeof(ValueData);
    const i32 elements_dst = array_handle.id + sizeof(ValueData) + offsets_size;

    // total array size - sizeof all element value's + the size needed to hold the element count & offsets buffer
    array_data->contents.integer_value = sign_cast<i64>(elements_size + offsets_size);

    // test for empty array
    if (elements_size <= 0)
    {
        return true;
    }

    /*
     * Reserve offsets **before** getting pointers to the internals as the memory could have moved if parsing
     * in dynamic allocation mode and an allocation has happened
     */
    allocator_.reserve(sign_cast<i32>(offsets_size));

    BEE_ASSERT(static_cast<size_t>(elements_dst - elements_src) == offsets_size);

    memmove(
        allocator_.data() + elements_dst, // to the right
        allocator_.data() + elements_src, // from the left
        sign_cast<size_t>(elements_size)
    );

    // The offsets start after the element count
    auto cur_offset = sign_cast<i32>(offsets_size + sizeof(ValueData));
    auto cur_member = allocator_.get({ array_handle.id + cur_offset });
    auto offsets_count_ptr = reinterpret_cast<i32*>(allocator_.data() + elements_src);
    auto offsets_buffer = offsets_count_ptr + 1;

    // iterate the child elements of array node of the parse tree and get their offsets
    *offsets_count_ptr = element_count;
    for (int elem_idx = 0; elem_idx < element_count; ++elem_idx)
    {
        offsets_buffer[elem_idx] = cur_offset;
        cur_offset += cur_member->has_children() ? cur_member->as_size() + sizeof(ValueData) : sizeof(ValueData);
        cur_member = allocator_.get({ array_handle.id + cur_offset });
    }

    return true;
}

bool Document::parse_number(Cursor* cursor)
{
    // parse either sign or zero
    auto sign = 1.0;

    if (*cursor->current == '-')
    {
        sign = -1.0;
        ++(*cursor);
    }

    // parse the int part
    auto int_part = 0;
    if (*cursor->current != '0')
    {
        while (*cursor->current >= '0' && *cursor->current <= '9')
        {
            int_part = 10 * int_part + (*cursor->current - '0');
            ++(*cursor);
        }
    }
    else
    {
        ++(*cursor);
    }

    i64 frac_part = 0;
    i64 frac_denom = 1;

    // parse the fractional part
    if (*cursor->current == '.')
    {
        ++(*cursor);

        if (*cursor->current < '0' || *cursor->current > '9')
        {
            parse_error_ = Error(ErrorCode::number_missing_decimal, cursor);
            return false;
        }

        frac_part = 0;
        while (*cursor->current >= '0' && *cursor->current <= '9')
        {
            frac_part = 10 * frac_part + (*cursor->current - '0');
            frac_denom *= 10;
            ++(*cursor);
        }
    }

    // parse the exponent part
    int exp_sign = 1;
    int exp_part = 0;
    if (*cursor->current == 'e' || *cursor->current == 'E')
    {
        ++(*cursor);
        if (*cursor->current == '-')
        {
            exp_sign = -1;
            ++(*cursor);
        }
        else if (*cursor->current == '+')
        {
            ++(*cursor);
        }

        if (*cursor->current < '0' || *cursor->current > '9')
        {
            parse_error_ = Error(ErrorCode::numer_invalid_exponent, cursor);
            return false;
        }

        exp_part = 0;
        while (*cursor->current >= '0' && *cursor->current <= '9')
        {
            exp_part = 10 * exp_part + (*cursor->current - '0');
            ++(*cursor);
        }
    }

    const auto coefficient_numer = static_cast<double>(frac_part);
    const auto coefficient = static_cast<double>(int_part) + coefficient_numer / static_cast<double>(frac_denom);
    // TODO(Jacob): possible optimization here with table lookup for pow10 instead of generic pow()
    const auto exp = math::pow(10.0, static_cast<double>(exp_sign * exp_part));
    const auto val = sign * coefficient * exp;

    return allocator_.allocate(ValueType::number, &val, sizeof(double)).is_valid();
}

bool Document::parse_true(Cursor* cursor)
{
    const auto is_valid = advance_on_char(cursor, 't')
                       && advance_on_char(cursor, 'r')
                       && advance_on_char(cursor, 'u')
                       && advance_on_char(cursor, 'e');
    if (!is_valid) {
        return false;
    }

    bool alloc_data = true;
    if (!allocator_.allocate(ValueType::boolean, &alloc_data, sizeof(bool)).is_valid()) {
        parse_error_ = Error(ErrorCode::out_of_memory, cursor);
        return false;
    }

    return true;
}

bool Document::parse_false(Cursor* cursor)
{
    const auto is_valid = advance_on_char(cursor, 'f')
                       && advance_on_char(cursor, 'a')
                       && advance_on_char(cursor, 'l')
                       && advance_on_char(cursor, 's')
                       && advance_on_char(cursor, 'e');

    if (!is_valid)
    {
        return false;
    }

    bool alloc_data = false;
    if (!allocator_.allocate(ValueType::boolean, &alloc_data, sizeof(bool)).is_valid())
    {
        parse_error_ = Error(ErrorCode::out_of_memory, cursor);
        return false;
    }

    return true;
}

bool Document::parse_null(Cursor* cursor)
{
    const auto is_valid = advance_on_char(cursor, 'n')
                       && advance_on_char(cursor, 'u')
                       && advance_on_char(cursor, 'l')
                       && advance_on_char(cursor, 'l');
    if (!is_valid)
    {
        return false;
    }

    return allocator_.allocate(ValueType::null, nullptr, 0).is_valid();
}

/*
 * Value allocation and handling
 */

ValueType Document::get_type(const ValueHandle& value) const
{
    const auto data = allocator_.get(value);
    if (data == nullptr || !data->is_valid())
    {
        return ValueType::unknown;
    }

    return data->type;
}

bool Document::has_member(const ValueHandle& root, const char* key) const
{
    return get_member(root, key).is_valid();
}

ValueHandle Document::get_member(const ValueHandle& root, const char* key) const
{
    auto root_data = allocator_.get(root);
    if (root_data == nullptr || !root_data->is_valid() || root_data->type != ValueType::object)
    {
        return ValueHandle{};
    }

    // go to first member in object
    const auto sizeof_value = sign_cast<i32>(sizeof(ValueData));
    const auto root_size = root_data->as_size();

    ValueHandle cur_member_handle { root.id + sizeof_value };
    auto cur_member = allocator_.get(cur_member_handle);

    ValueHandle result{};
    for (int cur_offset = 0; cur_offset < root_size; ++cur_offset)
    {
        if (strcmp(cur_member->as_string(), key) == 0)
        {
            // make sure we return the members value rather than the key
            result.id = cur_member_handle.id + sizeof_value;
            break;
        }

        // move past key to value to get size to advance
        cur_member_handle.id += sizeof_value;
        cur_member = allocator_.get(cur_member_handle);
        if (cur_member == nullptr || !cur_member->is_valid())
        {
            break;
        }

        // advance past all children of the element if it has any, otherwise advance past the element
        if (cur_member->has_children())
        {
            cur_member_handle.id += cur_member->as_size() + sizeof_value;
        }
        else
        {
            cur_member_handle.id += sizeof_value;
        }

        // advance past the value
        cur_member = allocator_.get(cur_member_handle);
        if (cur_member == nullptr || !cur_member->is_valid())
        {
            break;
        }
    }

    return result;
}

ValueType Document::get_member_type(const ValueHandle& root, const char* key) const
{
    const auto member = get_member(root, key);
    if (BEE_FAIL_F(member.is_valid(), "json::Document: no such member '%s'", key))
    {
        return ValueType::unknown;
    }

    return get_type(member);
}

ValueData Document::get_data(const ValueHandle& handle) const
{
    const auto data = allocator_.get(handle);

    if (BEE_FAIL_F(data != nullptr, "json::Document: value doesn't exist"))
    {
        return ValueData{};
    }

    return *data;
}

ValueData Document::get_member_data(const ValueHandle& root, const char* key) const
{
    const auto member = get_member(root, key);

    if (BEE_FAIL_F(member.is_valid(), "json::Document: unable to find member '%s'", key))
    {
        return ValueData{};
    }

    return get_data(member);
}


ValueHandle Document::get_element(const ValueHandle& array, const i32 index) const
{
    const auto array_data = allocator_.get(array);
    if (BEE_FAIL_F(array_data != nullptr && array_data->type == ValueType::array, "json::Document: invalid array handle given"))
    {
        return ValueHandle{};
    }

    // the offset buffer is past the array item and the offset buffer count item
    const auto offsets = reinterpret_cast<const i32*>(allocator_.data() + get_offset_buffer_begin(array));
    return ValueHandle { array.id + offsets[index] };
}

ValueData Document::get_element_data(const ValueHandle& array, const i32 index) const
{
    const auto array_data = allocator_.get(array);
    if (BEE_FAIL_F(array_data != nullptr && array_data->type == ValueType::array, "json::Document: invalid array handle given"))
    {
        return ValueData{};
    }

    // the offset buffer is past the array item and the offset buffer count item
    const auto offsets = reinterpret_cast<const i32*>(allocator_.data() + get_offset_buffer_begin(array));
    return *reinterpret_cast<const ValueData*>(allocator_.data() + array.id + offsets[index]);
}

object_range_t Document::get_members_range(const ValueHandle& root)
{
    return object_range_t(&allocator_, root);
}

const_object_range_t Document::get_members_range(const ValueHandle& root) const
{
    return const_object_range_t(&allocator_, root);
}

object_iterator Document::get_members_iterator(const ValueHandle& root)
{
    return object_iterator(&allocator_, root);
}

const_object_iterator Document::get_members_iterator(const ValueHandle& root) const
{
    return const_object_iterator(&allocator_, root);
}

ArrayRangeAdapter Document::get_elements_range(const ValueHandle& root)
{
    return ArrayRangeAdapter(&allocator_, root);
}

ArrayRangeAdapter Document::get_elements_range(const ValueHandle& root) const
{
    return ArrayRangeAdapter(&allocator_, root);
}

ArrayIterator Document::get_elements_iterator(const ValueHandle& root)
{
    return ArrayIterator(&allocator_, root, 0);
}

ArrayIterator Document::get_elements_iterator(const ValueHandle& root) const
{
    return ArrayIterator(&allocator_, root, 0);
}

/*
 *********************************
 *
 * Pretty printing to string
 *
 *********************************
 */
void visit(const ValueHandle& handle, String* dst, const Document& src_doc, const i32 indent, const i32 depth);

void write_to_string(String* dst, const Document& src_doc, const i32 indent)
{
    visit(src_doc.root(), dst, src_doc, indent, 1);
}

void write_indent(String* dst, const i32 indent_size, const i32 indent_count)
{
    io::write_fmt(dst, "%*s", indent_size * indent_count, "");
}

void visit(const ValueHandle& handle, String* dst, const Document& src_doc, const i32 indent, const i32 depth)
{
    const auto value = src_doc.get_data(handle);
    switch (value.type)
    {
        case ValueType::object:
        {
            io::write_fmt(dst, "{\n");
            for (auto& member : src_doc.get_members_range(handle))
            {
                // Key
                write_indent(dst, indent, depth);
                io::write_fmt(dst, "\"%s\"", member.key);
                io::write_fmt(dst, ": ");
                // Value
                visit(member.value, dst, src_doc, indent, depth + 1);
                dst->append(",\n");
            }
            // remove the last comma
            dst->remove(dst->size() - 2);

            io::write_fmt(dst, "\n");
            write_indent(dst, indent, depth - 1);
            io::write_fmt(dst, "}");
            break;
        }
        case ValueType::array:
        {
            io::write_fmt(dst, "[\n");
            for (auto elem : src_doc.get_elements_range(handle))
            {
                write_indent(dst, indent, depth);
                visit(elem, dst, src_doc, indent, depth + 1);
                dst->append(",\n");
            }
            // remove the last comma
            dst->remove(dst->size() - 2);

            io::write_fmt(dst, "\n");
            write_indent(dst, indent, depth - 1);
            io::write_fmt(dst, "]");
            break;
        }
        case ValueType::string:
        {
            auto escaped = String(value.as_string());
            str::replace(&escaped, "\n", "\\n");
            io::write_fmt(dst, "\"%s\"", escaped.c_str());
            break;
        }
        case ValueType::number:
        {
            io::write_fmt(dst, "%.*g", DBL_DECIMAL_DIG, value.as_number());
            break;
        }
        case ValueType::boolean:
        {
            io::write_fmt(dst, value.as_boolean() ? "true" : "false");
            break;
        }
        case ValueType::null:
        {
            io::write_fmt(dst, "null");
            break;
        }
        case ValueType::unknown:
            break;
    }
}


} // namespace json
} // namespace bee
