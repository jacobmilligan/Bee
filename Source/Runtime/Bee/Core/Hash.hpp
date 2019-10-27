/*
 *  Hash.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/String.hpp"

#define XXH_STATIC_LINKING_ONLY
#include <xxhash.h>


namespace bee {


/*
 ******************************************************************************
 *
 * `get_hash` - calculates a hash of the bytes given in `input` using xxHash32
 *
 ******************************************************************************
 */
BEE_CORE_API u32 get_hash(const void* input, size_t length, u32 seed);


/*
 ***************************************************************************************
 *
 * `get_static_string_hash`
 *
 * Compile-time hashing of small strings. Useful as an alternative to RTTI or runtime
 * id-length strings. Uses fnv1a internally to compute the hash
 *
 ***************************************************************************************
 */

static constexpr u32 static_string_hash_seed_default = 0x811C9DC5u;

namespace detail {

static constexpr u32 static_string_hash_prime = 0x01000193u;

inline constexpr u32 fnv1a_compute(const char byte, const u32 hash_so_far) noexcept
{
    /*
     * Convert to unsigned long long and back to u32 to eliminate warning 4307 on MSVC without disabling globally.
     * Modulo arithmetic is actually desirable here.
     */
    return 1ull * (static_cast<u8>(byte) ^ hash_so_far) * static_string_hash_prime;
}

template <u32 Index>
inline constexpr u32 fnv1a(const char* string, u32 hash_so_far) noexcept;

// Index == 0 base case. Computes the hash of the first char
template <>
inline constexpr u32 fnv1a<0>(const char* string, const u32 hash_so_far) noexcept
{
    return fnv1a_compute(string[0], hash_so_far);
}

// Index > 0 case. The iteration is depth-first so as to start hashing from Index == 0 and backtrack to Index == Size-1
template <u32 Index>
inline constexpr u32 fnv1a(const char* string, const u32 hash_so_far) noexcept
{
    static_assert(Index > 0, "Invalid index");
    return fnv1a_compute(string[Index], fnv1a<Index - 1>(string, hash_so_far));
}


inline constexpr u32 runtime_fnv1a(const char* string, const i32 string_length, const u32 seed = static_string_hash_seed_default)
{
    constexpr u32 prime = detail::static_string_hash_prime;
    u32 hash = seed;

    for(int i = 0; i < string_length; ++i)
    {
        hash = hash ^ string[i];
        hash *= prime;
    }

    return hash;
}

} // namespace detail


/**
 * Computes a compile-time hash of a static string
 */
template <u32 Size>
inline constexpr u32 get_static_string_hash(const char(&input)[Size], const u32 seed = static_string_hash_seed_default)
{
    return detail::fnv1a<Size - 1>(input, seed);
}


/*
 ***********************************
 *
 * `Hash` - generic hash functors
 *
 ***********************************
 */


template <typename T>
struct Hash {
    inline u32 operator()(const T& key) const
    {
        return get_hash(&key, sizeof(T), 0xF00D);
    }
};

template <>
struct Hash<u8> {
    inline u32 operator()(const u8 key) const
    {
        return key;
    }
};

template <>
struct Hash<i8> {
    inline u32 operator()(const i8 key) const
    {
        return static_cast<u32>(key);
    }
};

template <>
struct Hash<u16> {
    inline u32 operator()(const u16 key) const
    {
        return key;
    }
};

template <>
struct Hash<i16> {
    inline u32 operator()(const i16 key) const
    {
        return static_cast<u32>(key);
    }
};

template <>
struct Hash<u32> {
    inline u32 operator()(const u32 key) const
    {
        return key;
    }
};

template <>
struct Hash<i32> {
    inline u32 operator()(const i32 key) const
    {
        return static_cast<u32>(key);
    }
};

template <>
struct Hash<u64> {
    inline u32 operator()(const u64 key) const
    {
        return static_cast<u32>(key);
    }
};

template <>
struct Hash<i64> {
    inline u32 operator()(const i64 key) const
    {
        return static_cast<u32>(key);
    }
};

template <>
struct Hash<String> {
    inline u32 operator()(const String& key) const
    {
        return get_hash(key.data(), key.size(), 0xF00D);
    }

    inline u32 operator()(const StringView& key) const
    {
        return get_hash(key.data(), key.size(), 0xF00D);
    }

    inline u32 operator()(const char* key) const
    {
        return get_hash(key, str::length(key), 0xF00D);
    }
};

template <>
struct Hash<StringView> {
    inline u32 operator()(const StringView& key) const
    {
        return get_hash(key.data(), key.size(), 0xF00D);
    }

    inline u32 operator()(const char* key) const
    {
        return get_hash(key, str::length(key), 0xF00D);
    }
};

template <>
struct Hash<const char*> {
    inline u32 operator()(const char* key) const
    {
        return get_hash(key, str::length(key), 0xF00D);
    }
};

template <>
struct Hash<const char* const> {
    inline u32 operator()(const char* const key) const
    {
        return get_hash(key, str::length(key), 0xF00D);
    }
};


template <typename T>
BEE_FORCE_INLINE u32 get_hash(const T& object)
{
    return Hash<T>{}(object);
}


class BEE_CORE_API HashState : public Noncopyable {
public:
    HashState();

    explicit HashState(const u32 seed);

    HashState(HashState&& other) noexcept ;

    ~HashState() = default;

    HashState& operator=(HashState&& other) noexcept;

    void add(const void* input, const u32 size);

    template <typename T>
    inline void add(const T& input)
    {
        add(&input, sizeof(T));
    }

    u32 end();
private:
    XXH32_state_t state_;
};


} // namespace bee
