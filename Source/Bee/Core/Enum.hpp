/*
 *  Enum.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"


namespace bee {


/**
 * # BEE_SCOPED_TRANSLATION_TABLE
 *
 * Defines a static constexpr table for translating from `enum_type` into `native_type`, i.e.
 * bee::gpu::PixelFormat::bgra8 -> VK_FORMAT_B8G8R8A8_UNORM. Allows for branchless,
 * constant-time enum->enum translations in performance-sensitive code. If the non-native enum type is ever
 * changed, the function will static assert that the values have changed and the translation table defined with this
 * macro needs updating
 */
#define BEE_TRANSLATION_TABLE(VALUE, ENUM_TYPE, NATIVE_TYPE, MAX_ENUM_VALUE, ...)                                                  \
    static constexpr NATIVE_TYPE translation_table[] = { __VA_ARGS__ };                                                             \
    static constexpr size_t table_size = static_array_length(translation_table);                                                    \
    static_assert(table_size == static_cast<size_t>(MAX_ENUM_VALUE),                                                                \
                "Bee: error: the translation table for "#NATIVE_TYPE                                                                \
                " is missing entries. Please update to sync with the "#ENUM_TYPE" enum.");                                          \
    BEE_ASSERT_F_NO_DEBUG_BREAK(static_cast<size_t>(VALUE) < static_cast<size_t>(MAX_ENUM_VALUE),                                   \
                              "Invalid value for `"#ENUM_TYPE"` to `"#NATIVE_TYPE"` translation table given: `"#MAX_ENUM_VALUE"`"); \
    return translation_table[static_cast<int>(VALUE)];

#define BEE_TRANSLATION_TABLE_FUNC(NAME, ENUM_TYPE, NATIVE_TYPE, MAX_ENUM_VALUE, ...) \
    NATIVE_TYPE NAME(const ENUM_TYPE value) { BEE_TRANSLATION_TABLE(value, ENUM_TYPE, NATIVE_TYPE, MAX_ENUM_VALUE, __VA_ARGS__); }
/**
 * # BEE_FLAGS
 *
 * Defines a scoped enum class `E` with the underlying type `T` with bitwise operator
 * overloads: `~`, `|`, `^`, `&`
 */
#define __BEE_ENUM_FLAG_OPERATOR(type, underlying, op)                                                \
    inline constexpr type operator op(const type lhs, const type rhs) noexcept                      \
    {                                                                                               \
        return static_cast<type>(static_cast<underlying>(lhs) op static_cast<underlying>(rhs));     \
    }                                                                                               \
                                                                                                    \
    inline constexpr type operator op##=(type& lhs, const type rhs) noexcept                        \
    {                                                                                               \
        (lhs) = (lhs) op (rhs);                                                                     \
        return (lhs);                                                                               \
    }

#define BEE_FLAGS(E, T) enum class E : T;                           \
    inline constexpr T underlying_flag_t(E cls) noexcept            \
    {                                                               \
        return static_cast<T>(cls);                                 \
    }                                                               \
    template <E Value>                                              \
    inline constexpr T flag_index() noexcept                        \
    {                                                               \
        return static_cast<T>(1u << static_cast<T>(Value));         \
    }                                                               \
    inline constexpr E operator~(E cls) noexcept                    \
    {                                                               \
        return static_cast<E>(~underlying_flag_t(cls));             \
    }                                                               \
    __BEE_ENUM_FLAG_OPERATOR(E, T, |)                               \
    __BEE_ENUM_FLAG_OPERATOR(E, T, ^)                               \
    __BEE_ENUM_FLAG_OPERATOR(E, T, &)                               \
    enum class E : T

#define BEE_REFLECTED_FLAGS(E, T, ...) enum class E : T;            \
    inline constexpr T underlying_flag_t(E cls) noexcept            \
    {                                                               \
        return static_cast<T>(cls);                                 \
    }                                                               \
    template <E Value>                                              \
    inline constexpr T flag_index() noexcept                        \
    {                                                               \
        return static_cast<T>(1u << static_cast<T>(Value));         \
    }                                                               \
    inline constexpr E operator~(E cls) noexcept                    \
    {                                                               \
        return static_cast<E>(~underlying_flag_t(cls));             \
    }                                                               \
    __BEE_ENUM_FLAG_OPERATOR(E, T, |)                               \
    __BEE_ENUM_FLAG_OPERATOR(E, T, ^)                               \
    __BEE_ENUM_FLAG_OPERATOR(E, T, &)                               \
    enum class BEE_REFLECT(flags, __VA_ARGS__) E : T

#define BEE_ENUM_STRUCT(Name)                                                                           \
    Enum value;                                                                                         \
    Name() = default;                                                                                   \
    Name(const int i) : value(static_cast<Enum>(i)) {}                                                  \
    Name(const Enum e) : value(e) {}                                                                    \
    Name(const Name& other) : value(other.value) {}                                                     \
    inline constexpr operator Enum() const { return value; }                                            \
    inline constexpr bool operator==(const Name& rhs) const { return value == rhs.value; }              \
    inline constexpr bool operator!=(const Name& rhs) const { return value != rhs.value; }              \
    inline constexpr bool operator<(const Name& rhs) const { return value < rhs.value; }                \
    inline constexpr bool operator>(const Name& rhs) const { return value > rhs.value; }                \
    inline constexpr bool operator<=(const Name& rhs) const { return value <= rhs.value; }              \
    inline constexpr bool operator>=(const Name& rhs) const { return value >= rhs.value; }              \
    inline constexpr bool operator==(const Enum& rhs) const { return value == rhs; }                    \
    inline constexpr bool operator!=(const Enum& rhs) const { return value != rhs; }                    \
    inline constexpr bool operator<(const Enum& rhs) const { return value < rhs; }                      \
    inline constexpr bool operator>(const Enum& rhs) const { return value > rhs; }                      \
    inline constexpr bool operator<=(const Enum& rhs) const { return value <= rhs; }                    \
    inline constexpr bool operator>=(const Enum& rhs) const { return value >= rhs; }



} // namespace bee
