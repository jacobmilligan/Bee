/*
 *  Lex.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/String.hpp"


namespace bee {


#define BSC_KEYWORDS                \
    BSC_KEYWORD(RenderPass)         \
    BSC_KEYWORD(RasterState)        \
    BSC_KEYWORD(MultisampleState)   \
    BSC_KEYWORD(DepthStencilState)  \
    BSC_KEYWORD(PipelineState)      \
    BSC_KEYWORD(SamplerState)       \
    BSC_KEYWORD(Attachment)         \
    BSC_KEYWORD(SubPass)            \
    BSC_KEYWORD(Shader)

#define BSC_CHAR_TOKENS                         \
    BSC_CHAR_TOK(open_bracket, '{')             \
    BSC_CHAR_TOK(close_bracket, '}')            \
    BSC_CHAR_TOK(open_square_bracket, '[')      \
    BSC_CHAR_TOK(close_square_bracket, ']')     \
    BSC_CHAR_TOK(colon, ':')                    \
    BSC_CHAR_TOK(comma, ',')

#define BSC_TOKENS          \
    BSC_TOK(invalid)        \
    BSC_TOK(eof)            \
    BSC_TOK(identifier)     \
    BSC_TOK(enum_const)     \
    BSC_TOK(bool_true)      \
    BSC_TOK(bool_false)     \
    BSC_TOK(signed_int)     \
    BSC_TOK(unsigned_int)   \
    BSC_TOK(floating_point) \
    BSC_TOK(string_literal) \
    BSC_TOK(code)           \
    BSC_TOK(resource_layouts)


#define BSC_ALL_TOKENS  \
    BSC_KEYWORDS        \
    BSC_CHAR_TOKENS     \
    BSC_TOKENS


using bsc_ident_t = StaticString<128>;


enum class BscTokenKind
{
#define BSC_KEYWORD(X) X,
#define BSC_CHAR_TOK(X, Char) X,
#define BSC_TOK(X) X,

    BSC_ALL_TOKENS
    token_count

#undef BSC_KEYWORD
#undef BSC_CHAR_TOK
#undef BSC_TOK
};

enum class BscErrorCode
{
    unexpected_character,
    expected_character,
    expected_end_of_multiline_comment,
    invalid_object_type,
    unexpected_eof,
    invalid_object_field,
    expected_boolean,
    expected_digit,
    expected_decimal,
    invalid_field_value,
    too_many_fields,
    array_too_large,
    unexpected_token_kind,
    number_too_long,
    invalid_number_format,
    none
};

struct BscToken
{
    BscTokenKind    kind { BscTokenKind::invalid };
    const char*     begin { nullptr };
    const char*     end { nullptr };
    i32             line { 0 };
    i32             column { 0 };

    BscToken() = default;

    BscToken(const BscTokenKind tok_kind, const char* tok_begin, const char* tok_end, const i32 tok_line, const i32 tok_col)
        : kind(tok_kind),
          begin(tok_begin),
          end(tok_end),
          line(tok_line),
          column(tok_col)
    {}

    inline bool is_valid() const
    {
        return kind != BscTokenKind::invalid;
    }
};

struct BscError
{
    BscErrorCode    code { BscErrorCode::none };
    StringView      text;
    char            error_char { '\0' };
    char            char_param { '\0' };
    BscTokenKind    token_param { BscTokenKind::invalid };
    i32             line { 0 };
    i32             column { 0 };

    String to_string(Allocator* allocator = system_allocator()) const;
};

/**
 ************************************
 *
 * # BscLexer
 *
 * Tokenizes the source BSC string
 * on demand rather processing the
 * whole text into a stream
 *
 ************************************
 */
class BscLexer
{
public:
    BscLexer() = default;

    explicit BscLexer(const StringView& source);

    bool consume(BscToken* tok);

    bool consume_as(const BscTokenKind kind, BscToken* tok);

    bool consume_code(BscToken* tok);

    bool peek(BscToken* tok);

    bool advance_valid(const i32 count = 1);

    inline i32 offset() const
    {
        return static_cast<i32>(current_ - source_.begin());
    }

    inline const BscError& get_error() const
    {
        return error_;
    }

    inline bool is_valid() const
    {
        return !is_eof(current_);
    }

    inline i32 line() const
    {
        return line_;
    }

    inline i32 column() const
    {
        return line_;
    }

    const char* current() const
    {
        return current_;
    }

private:
    StringView  source_;
    const char* current_ { nullptr };
    i32         line_ { 0 };
    i32         column_ { 0 };
    BscError    error_;

    void advance(const i32 count = 1);

    bool skip_whitespace();

    bool skip_comment();

    bool report_error(const BscErrorCode code, const char* begin);

    bool report_error(const BscErrorCode code, const BscTokenKind token_kind, const char* begin);

    bool report_error(const BscErrorCode code, const char* begin, const char error_char, const char char_param);

    bool report_error(const BscErrorCode code, const BscTokenKind token_kind, const char* begin, const char error_char, const char char_param);

    bool is_char_token(const char c, BscToken* dst = nullptr);

    inline bool is_eof(const char* ptr) const
    {
        return ptr == source_.end() || *ptr == '\0';
    }

    inline char lookahead() const
    {
        return current_ < source_.end() ? current_[1] : '\0';
    }

    bool consume_as_identifier(BscToken* tok);

    bool consume_as_number(BscToken* tok);

    bool consume_as_string_literal(BscToken* tok);
};


} // namespace bee