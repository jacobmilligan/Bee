/*
 *  HashTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Hash.hpp>

#include <GTest.hpp>

BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4307)
template <size_t Size>
inline const uint32_t get_runtime_string_hash_for_testing(const char(&data)[Size]) {

    uint32_t hash = bee::static_string_hash_seed_default;
    uint32_t prime = bee::detail::static_string_hash_prime;

    for(int i = 0; i < Size; ++i)
    {
        uint8_t value = data[i];
        hash = hash ^ value;
        hash *= prime;
    }

    return hash;

} //hash_32_fnv1a
BEE_POP_WARNING


TEST(HashTests, compile_time_string_hashing)
{
    const auto compile_time_hash = bee::get_static_string_hash("Hashing a string for unit testing");
    const auto runtime_hash = bee::detail::runtime_fnv1a("Hashing a string for unit testing");
    ASSERT_EQ(compile_time_hash, runtime_hash);
}