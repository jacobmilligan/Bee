/*
 *  Lex.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderPipeline/Parser/Lex.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Math/Math.hpp"


namespace bee {


struct BscTokenDef
{
    BscTokenKind    kind { BscTokenKind::invalid };
    const char*     text { nullptr };
    i32             text_length { 0 };

    BscTokenDef(const BscTokenKind def_kind, const char* def_text) noexcept
        : kind(def_kind),
          text(def_text),
          text_length(str::length(def_text))
    {}
};

/*
 * Token string to enum lookup table
 */
#define BSC_KEYWORD(X) { BscTokenKind::X, #X },
static BscTokenDef g_keywords[] = {
    BSC_KEYWORDS
};
#undef BSC_KEYWORD

/*
 * Token name table
 */
#define BSC_KEYWORD(X) #X,
#define BSC_CHAR_TOK(X, Char) #X,
#define BSC_TOK(X) #X,
static const char* g_token_names[] = {
    BSC_ALL_TOKENS
    "token_count"
};
#undef BSC_KEYWORD
#undef BSC_CHAR_TOK
#undef BSC_TOK

template <i32 Size>
i32 find_token_def(const StringView& name, BscTokenDef(&tokens)[Size])
{
    const auto index = find_index_if(tokens, [&](const BscTokenDef& def)
    {
        return str::compare_n(name, def.text, math::min(def.text_length, name.size())) == 0;
    });

    return index;
}

const char* get_token_name(const BscTokenKind kind)
{
    BEE_ASSERT(static_cast<i32>(kind) < static_array_length(g_token_names));

    return g_token_names[static_cast<i32>(kind)];
}

bool is_keyword(const char* text, const i32 text_length, BscTokenKind* dst)
{
    const auto index = find_index_if(g_keywords, [&](const BscTokenDef& def)
    {
        return str::compare_n(def.text, text, math::min(def.text_length, text_length)) == 0;
    });

    if (index >= 0)
    {
        *dst = g_keywords[index].kind;
        return true;
    }

    return false;
}

bool is_keyword(const char* text_begin, const char* text_end, BscTokenKind* dst)
{
    return is_keyword(text_begin, static_cast<i32>(text_end - text_begin), dst);
}


String BscError::to_string(Allocator* allocator) const
{
    String result(allocator);
    str::format(&result, "BSC error at: %d:%d: ", line, column);

    switch (code)
    {
        case BscErrorCode::unexpected_character:
        {
            str::format(&result, "unexpected character '%c'", text[0]);
            break;
        }
        case BscErrorCode::expected_character:
        {
            str::format(&result, "unexpected character '%c'. Expected '%c' instead", error_char, char_param);
            break;
        }
        case BscErrorCode::expected_end_of_multiline_comment:
        {
            str::format(&result, "expected end of multiline comment");
            break;
        }
        case BscErrorCode::invalid_object_type:
        {
            str::format(&result, "invalid object type");
            break;
        }
        case BscErrorCode::unexpected_eof:
        {
            str::format(&result, "unexpected end of file");
            break;
        }
        case BscErrorCode::invalid_object_field:
        {
            str::format(&result, "invalid field");
            break;
        }
        case BscErrorCode::expected_boolean:
        {
            str::format(&result, "expected boolean");
            break;
        }
        case BscErrorCode::expected_digit:
        {
            str::format(&result, "expected digit (0-9)");
            break;
        }
        case BscErrorCode::expected_decimal:
        {
            str::format(&result, "floating point number was missing a decimal part after the '.'");
            break;
        }
        case BscErrorCode::invalid_field_value:
        {
            str::format(&result, "invalid field value");
            break;
        }
        case BscErrorCode::unexpected_token_kind:
        {
            str::format(&result, "unexpected %s token", get_token_name(token_param));
        }
        case BscErrorCode::none:
        {
            str::format(&result, "no error");
            break;
        }
        case BscErrorCode::number_too_long:
        {
            str::format(&result, "number is too long to be represented in the supported integer or floating point formats");
            break;
        }
        case BscErrorCode::invalid_number_format:
        {
            str::format(&result, "invalid number format");
            break;
        }
        case BscErrorCode::invalid_layout_name:
        {
            str::format(&result, "invalid layout name");
            break;
        }
        default:
        {
            BEE_UNREACHABLE("Error code not translated");
        }
    }

    str::format(&result, "\n\t`%" BEE_PRIsv "`", BEE_FMT_SV(text));

    return BEE_MOVE(result);
}


/*
 ************************************
 *
 * BscLexer - implementation
 *
 ************************************
 */
BscLexer::BscLexer(const StringView& source)
    : source_(source),
      current_(source.begin())
{}

bool BscLexer::skip_whitespace()
{
    while (!is_eof(current_))
    {
        if (str::is_space(*current_))
        {
            advance();
            continue;
        }

        if (*current_ == '/' && offset() < source_.size() - 1 && (current_[1] == '/' || current_[1] == '*'))
        {
            if (!skip_comment())
            {
                return false;
            }

            continue;
        }

        // end of whitespace
        break;
    }

    return true;
}

bool BscLexer::advance_valid(const i32 count)
{
    for (int i = 0; i < count; ++i)
    {
        ++current_;
        ++column_;

        if (is_eof(current_))
        {
            return false;
        }

        if (*current_ == '\n')
        {
            ++line_;
            column_ = 0;
        }
    }

    return true;
}

void BscLexer::advance(const i32 count)
{
    advance_valid(count);
}

bool BscLexer::skip_comment()
{
    const char* begin = current_;

    if (offset() >= source_.size() - 1)
    {
        return true;
    }

    const auto single_line = current_[0] == '/' && current_[1] == '/';
    const auto multiline = current_[0] == '/' && current_[1] == '*';

    if (!single_line && !multiline)
    {
        return true;
    }

    if (!advance_valid(2))
    {
        return false;
    }

    if (single_line)
    {
        while (*current_ != '\n' && !is_eof(current_))
        {
            advance();
        }

        return true;
    }

    // multiline
    while (offset() < source_.size() - 1)
    {
        if (current_[0] == '*' && current_[1] == '/')
        {
            // successful multiline comment parse
            return advance_valid(2);
        }

        advance();
    }

    // malformed multiline comment

    report_error(BscErrorCode::expected_end_of_multiline_comment, begin);
    return false;
}

bool BscLexer::report_error(const BscErrorCode code, const char* begin)
{
    return report_error(code, begin, begin[0], '\0');
}

bool BscLexer::report_error(const BscErrorCode code, const BscTokenKind token_kind, const char* begin)
{
    return report_error(code, token_kind, begin, begin[0], '\0');
}

bool BscLexer::report_error(const BscErrorCode code, const char* begin, const char error_char, const char char_param)
{
    return report_error(code, BscTokenKind::invalid, begin, error_char, char_param);
}

bool BscLexer::report_error(const BscErrorCode code, const BscTokenKind token_kind, const char* begin, const char error_char, const char char_param)
{
    error_.code = code;
    error_.text = StringView(begin, static_cast<i32>(current_ - begin));
    error_.line = 1;
    error_.column = 1;
    error_.token_param = token_kind;

    const char* reader = source_.begin();
    while (reader != begin)
    {
        if (*reader == '\n')
        {
            ++error_.line;
            error_.column = 0;
        }

        ++reader;
        ++error_.column;
    }

    return false;
}

bool BscLexer::is_char_token(const char c, BscToken* dst)
{
    switch (c)
    {
#define BSC_CHAR_TOK(X, Char) case Char:                                                \
        {                                                                               \
            if (dst != nullptr)                                                         \
            {                                                                           \
                *dst = BscToken(BscTokenKind::X, current_, current_, line_, column_);   \
            }                                                                           \
            return true;                                                                \
        }

        BSC_CHAR_TOKENS

#undef BSC_CHAR_TOK

        default:
        {
            break;
        }
    }

    return false;
}

bool BscLexer::consume(BscToken* tok)
{
    if (!skip_whitespace())
    {
        return false;
    }

    new (tok) BscToken (BscTokenKind::invalid, current_, current_, line_, column_);

    // check EOF
    if (is_eof(current_))
    {
        tok->kind = BscTokenKind::eof;
        return false;
    }

    // parse as if it's a single character token, i.e. '{'
    if (is_char_token(*current_, tok))
    {
        advance();
        return true;
    }

    switch (*current_)
    {
        case '\"':
        {
            return consume_as_string_literal(tok);
        }

        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
        case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
        case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
        case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
        case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
        case 'v': case 'w': case 'x': case 'y': case 'z':
        case '_':
        {
            if (!consume_as_identifier(tok))
            {
                return false;
            }

            if (is_keyword(tok->begin, tok->end, &tok->kind))
            {
                return tok;
            }

            StringView ident(tok->begin, tok->end);

            if (ident == "true")
            {
                tok->kind = BscTokenKind::bool_true;
            }
            else if (ident == "false")
            {
                tok->kind = BscTokenKind::bool_false;
            }

            break;
        }

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-':
        {
            return consume_as_number(tok);
        }

        default:
        {
            report_error(BscErrorCode::unexpected_character, tok->begin);
            break;
        }
    }

    return true;
}

bool BscLexer::consume_as(const BscTokenKind kind, BscToken* tok)
{
    if (!consume(tok))
    {
        return false;
    }

    if (tok->kind == kind)
    {
        return true;
    }

    return report_error(BscErrorCode::expected_character, tok->kind, tok->begin);
}

bool BscLexer::consume_code(BscToken* tok)
{
    int scope_count = 0;

    new (tok) BscToken(BscTokenKind::code, current_, current_, line_, column_);

    while (scope_count >= 0)
    {
        advance();

        if (*current_ == '{')
        {
            ++scope_count;
            continue;
        }

        if (*current_ == '}')
        {
            --scope_count;
            continue;
        }

        if (is_eof(current_))
        {
            tok->kind = BscTokenKind::invalid;
            return report_error(BscErrorCode::unexpected_eof, tok->begin);
        }
    }

    tok->end = current_ - 1;
    return true;
}

bool BscLexer::peek(BscToken *tok)
{
    auto current = current_;
    auto line = line_;
    auto column = column_;
    auto error = error_;

    const auto is_bad = consume(tok);

    current_ = current;
    line_ = line;
    column_ = column;
    error_ = error;

    return is_bad;
}

bool BscLexer::consume_as_identifier(BscToken* tok)
{
    if (!str::is_alpha(*current_) && *current_ != '_')
    {
        return report_error(BscErrorCode::unexpected_character, tok->begin);
    }

    if (!advance_valid())
    {
        return false;
    }

    while (str::is_alpha(*current_) || str::is_digit(*current_) || *current_ == '_')
    {
        // just handle as normal key
        if (!advance_valid())
        {
            return false;
        }
    }

    tok->end = current_;
    tok->kind = BscTokenKind::identifier;
    return true;
}

bool BscLexer::consume_as_number(bee::BscToken* tok)
{
    auto dot_already_seen = false;

    bool is_signed = *current_ == '-';

    if (is_signed)
    {
        advance();
    }

    while (str::is_digit(*current_) || *current_ == '.')
    {
        if (*current_ == '.')
        {
            if (dot_already_seen)
            {
                return report_error(BscErrorCode::unexpected_character, tok->begin, *current_, '.');
            }

            if (!str::is_digit(lookahead()))
            {
                return report_error(BscErrorCode::expected_decimal, tok->begin, lookahead(), '\0');
            }

            dot_already_seen = true;
        }

        advance();
    }

    tok->end = current_;

    if (!dot_already_seen)
    {
        tok->kind = is_signed ? BscTokenKind::signed_int : BscTokenKind::unsigned_int;
    }
    else
    {
        tok->kind = BscTokenKind::floating_point;
    }

    return true;
}

bool BscLexer::consume_as_string_literal(BscToken* tok)
{
    if (*current_ != '\"')
    {
        return false;
    }

    const auto quotes_begin = current_;

    ++current_;

    tok->begin = current_;

    while (*current_ != '\"')
    {
        if (is_eof(current_))
        {
            return report_error(BscErrorCode::expected_character, quotes_begin, *current_, '\"');
        }

        ++current_;
    }

    tok->end = current_ - 1;

    return true;
}


} // namespace bee