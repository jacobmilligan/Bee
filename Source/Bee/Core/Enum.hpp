/*
 *  Flags.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

#if BEE_COMPILER_MSVC == 1
    #include <intrin.h>
#endif // BEE_COMPILER_MSVC == 1


namespace bee {

/**
 * # BEE_TRANSLATION_TABLE
 *
 * Defines a free function `func_name` that internally holds a static constexpr table for translating from `enum_type`
 * into `native_type`, i.e. bee::gpu::PixelFormat::bgra8 -> VK_FORMAT_B8G8R8A8_UNORM. Allows for branchless,
 * constant-time enum->enum translations in performance-sensitive code. If the non-native enum type is ever
 * changed, the function will static assert that the values have changed and the translation table defined with this
 * macro needs updating
 */
#define BEE_TRANSLATION_TABLE(func_name, enum_type, native_type, max_enum_value, ...)                                                   \
native_type func_name(const enum_type value)                                                                                     \
{                                                                                                                                       \
    static constexpr native_type translation_table[] = { __VA_ARGS__ };                                                                 \
    static constexpr size_t table_size = static_array_length(translation_table);                                                        \
    static_assert(table_size == static_cast<size_t>(max_enum_value),                                                                    \
                "Bee: error: the translation table for "#native_type                                                              \
                " is missing entries. Please update to sync with the "#enum_type" enum.");                                              \
    BEE_ASSERT_F_NO_DEBUG_BREAK(static_cast<size_t>(value) < static_cast<size_t>(max_enum_value),                                       \
                              "Invalid value for `"#enum_type"` to `"#native_type"` translation table given: `"#max_enum_value"`");     \
    return translation_table[static_cast<size_t>(value)];                                                                               \
}

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

#define BEE_FLAGS(E, T) enum class  E : T;                          \
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

/**
 * # count_trailing_zeroes
 *
 * Count trailing zeroes in a bitmask using each compilers builtin intrinsics.
 * For MSVC implementations see:
 * https://stackoverflow.com/questions/355967/how-to-use-msvc-intrinsics-to-get-the-equivalent-of-this-gcc-code
 */
BEE_FORCE_INLINE u32 count_trailing_zeroes(const u32 value)
{
#if BEE_COMPILER_GCC == 1 || BEE_COMPILER_CLANG == 1
    return __builtin_ctz(value);
#elif BEE_COMPILER_MSVC == 1
    // see: https://stackoverflow.com/questions/355967/how-to-use-msvc-intrinsics-to-get-the-equivalent-of-this-gcc-code
    unsigned long result = 0;
    if (_BitScanForward(&result, value))
    {
        return result;
    }
    return 32u;
#else
    #error Unsupported platform
#endif // BEE_COMPILER_*
};

/**
 * # count_leading_zeroes
 *
 * Count leading zeroes in a bitmask using each compilers builtin intrinsics.
 * For MSVC implementations see:
 * https://stackoverflow.com/questions/355967/how-to-use-msvc-intrinsics-to-get-the-equivalent-of-this-gcc-code
 */
BEE_FORCE_INLINE u32 count_leading_zeroes(const u32 value)
{
#if BEE_COMPILER_GCC == 1 || BEE_COMPILER_CLANG == 1
    return __builtin_clz(value);
#elif BEE_COMPILER_MSVC == 1
    unsigned long result = 0;
    if (_BitScanReverse(&result, value)) {
        return 31u - result;
    }
    return 32u;
#else
    #error Unsupported platform
#endif // BEE_COMPILER_*
}


/**
 * # for_each_flag
 *
 * Iterates all the flags set in an enum defined with BEE_FLAGS and
 * calls a function for each flag
 */
template <typename FlagType, typename FuncType>
constexpr void for_each_flag(const FlagType& flags, const FuncType& callback)
{
    // see: https://lemire.me/blog/2018/02/21/iterating-over-set-bits-quickly/
    auto bitmask = static_cast<u32>(flags);
    while (bitmask != 0)
    {
        const auto cur_bit = count_trailing_zeroes(bitmask);
        callback(static_cast<FlagType>(1u << cur_bit));
BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned
        bitmask ^= bitmask & -bitmask;
BEE_POP_WARNING
    }
}


/**
 * # decode_flag
 *
 * Returns `return_val` if the flag `flag` is present in the `flag_set`, otherwise returns 0
 */
template <typename FlagType, typename DecodedType>
constexpr DecodedType decode_flag(const FlagType flag_set, const FlagType flag, const DecodedType return_val)
{
    if ((flag_set & flag) != static_cast<FlagType>(0))
    {
        return return_val;
    }
    return static_cast<DecodedType>(0);
}

/**
 * # get_flag_if_true
 *
 * Returns `flag` if `predicate` is true, otherwise returns 0
 */
template <typename FlagType>
inline constexpr FlagType get_flag_if_true(const bool predicate, const FlagType flag)
{
    return predicate ? flag : static_cast<FlagType>(0);
}


} // namespace bee
