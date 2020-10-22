/*
 *  EnumStruct.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/NumericTypes.hpp"


#define BEE_ENUM_STRUCT(Name)                                                                           \
    i32 value { 0 };                                                                                    \
    Name() = default;                                                                                   \
    BEE_NOLINT(Name(Enum e)) : value(e) {}                                                              \
    Name(const Name& other) : value(other.value) {}                                                     \
    inline constexpr operator Name::Enum() const { return static_cast<Name::Enum>(value); }             \
    inline constexpr bool operator==(const Name& rhs) const { return value == rhs.value; }              \
    inline constexpr bool operator!=(const Name& rhs) const { return value != rhs.value; }              \
    inline constexpr bool operator<(const Name& rhs) const { return value < rhs.value; }                \
    inline constexpr bool operator>(const Name& rhs) const { return value > rhs.value; }                \
    inline constexpr bool operator<=(const Name& rhs) const { return value <= rhs.value; }              \
    inline constexpr bool operator>=(const Name& rhs) const { return value >= rhs.value; }
