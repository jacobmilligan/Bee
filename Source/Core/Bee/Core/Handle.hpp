/*
 *  Handle.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {


/**
 * A `RawHandle` is a wrapper around an unsigned integer handle with a minimum ID of zero and an invalid ID
 * of UINT_MAX. RawHandle's are most often used as indexes into arrays that don't require versioning or
 * opaque keys into hash tables
 */
#define BEE_RAW_HANDLE(name, IdType, InvalidId)                                             \
struct name                                                                                 \
{                                                                                           \
    using id_t                          = IdType;                                           \
    static constexpr IdType min_id      = static_cast<IdType>(0);                           \
    static constexpr IdType invalid_id  = InvalidId;                                        \
                                                                                            \
    IdType id { InvalidId };                                                                \
                                                                                            \
    constexpr name() noexcept = default;                                                    \
                                                                                            \
    explicit constexpr name(const IdType& new_id) noexcept                                  \
        : id(new_id)                                                                        \
    {}                                                                                      \
                                                                                            \
    inline constexpr bool is_valid() const { return id >= min_id && id != invalid_id; }     \
    inline constexpr bool operator==(const name& other) const { return id == other.id; }    \
    inline constexpr bool operator!=(const name& other) const { return id != other.id; }    \
}

#define BEE_RAW_HANDLE_I32(Name) BEE_RAW_HANDLE(Name, bee::i32, -1)

#define BEE_RAW_HANDLE_U32(Name) BEE_RAW_HANDLE(Name, bee::u32, bee::limits::max<bee::u32>())


/**
 * `HandleGenerator` generates integers that encode an index and a version in it's
 * id. This allows them to be used in array-based pools where objects are stored contiguously but
 * are often created and destroyed requiring version information to determine if a handle is stale or still
 * valid, i.e. the version encoded in `id` doesn't match the version of the actual stored object.
 */
template <typename IdType, IdType IndexBits, IdType VersionBits>
struct HandleGenerator
{
    using id_t                              = IdType;

    static constexpr IdType index_bits      = IndexBits;
    static constexpr IdType version_bits    = VersionBits;
    static constexpr IdType version_mask    = (static_cast<IdType>(1) << version_bits) - static_cast<IdType>(1);
    static constexpr IdType index_mask      = (static_cast<IdType>(1) << index_bits) - static_cast<IdType>(1);
    static constexpr IdType min_version     = static_cast<IdType>(1);
    static constexpr IdType invalid_id      = limits::max<IdType>();

    static constexpr IdType make_handle(const IdType& index, const IdType& version) noexcept
    {
        return (version << index_bits) | index;
    }

    static constexpr IdType get_version(const IdType& id) noexcept
    {
        return (id >> index_bits) & version_mask;
    }

    static constexpr IdType get_index(const IdType& id) noexcept
    {
        return id & index_mask;
    }

    static constexpr bool is_valid(const IdType& id) noexcept
    {
        return get_index(id) < index_mask && id < invalid_id;
    }
};

/**
 * Defines a new versioned handle struct type. This is a macro so as to allow bee-reflect to treat it
 * like a regular type - template magic doesn't play too nice with reflection and we don't want a
 * core type that's used everywhere like handles are to be a pain to work with
 */
#define BEE_VERSIONED_HANDLE(name, IdType, IndexBits, VersionBits)                              \
struct name                                                                                     \
{                                                                                               \
    using generator_t = bee::HandleGenerator<IdType, IndexBits, VersionBits>;                   \
                                                                                                \
    IdType id { generator_t::invalid_id };                                                      \
                                                                                                \
    constexpr name() noexcept = default;                                                        \
                                                                                                \
    explicit constexpr name(const IdType new_id) noexcept                                       \
        : id(new_id)                                                                            \
    {}                                                                                          \
                                                                                                \
    constexpr name(const IdType index, const IdType version) noexcept                           \
        : id(generator_t::make_handle(index, version))                                          \
    {}                                                                                          \
                                                                                                \
    inline constexpr IdType index() const noexcept { return generator_t::get_index(id); }       \
    inline constexpr IdType version() const noexcept { return generator_t::get_version(id); }   \
    inline constexpr bool is_valid() const noexcept { return generator_t::is_valid(id); }       \
    inline constexpr bool operator==(const name& other) const { return id == other.id; }        \
    inline constexpr bool operator!=(const name& other) const { return id != other.id; }        \
}


/**
 * Convenience macros to avoid having to overload constructors when doing CRTP for handle tagging, i.e:
 * ```
 * struct MyHandle : public VersionedHandle<MyHandle> {
 *     using VersionedHandle<MyHandle>::VersionedHandle;
 *     MyHandle(u32 new_id) : VersionedHandle<MyHandle>{new_id} {}
 * }
 * ```
 * would be necessary to do without these macros
 */
#define BEE_VERSIONED_HANDLE_32(Name) BEE_VERSIONED_HANDLE(Name, bee::u32, 24u, 8u)
#define BEE_VERSIONED_HANDLE_64(Name) BEE_VERSIONED_HANDLE(Name, bee::u64, 48u, 16u)


} // namespace bee
