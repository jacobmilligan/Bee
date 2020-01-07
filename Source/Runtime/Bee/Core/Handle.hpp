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
 * The base struct type that all handles derive from
 */
template <typename IDType>
struct HandleBase {
    IDType id { static_cast<IDType>(0) };
};

template <typename IDType>
inline constexpr bool operator==(const HandleBase<IDType>& lhs, const HandleBase<IDType>& rhs)
{
    return lhs.id == rhs.id;
}

template <typename IDType>
inline constexpr bool operator!=(const HandleBase<IDType>& lhs, const HandleBase<IDType>& rhs)
{
    return !(lhs == rhs);
}

/**
 * A `RawHandle` is a wrapper around an unsigned integer handle with a minimum ID of zero and an invalid ID
 * of UINT_MAX. RawHandle's are most often used as indexes into arrays that don't require versioning or
 * opaque keys into hash tables
 */
template <typename Tag, typename IDType, IDType InvalidID>
struct RawHandle : public HandleBase<IDType> {
    using tag_t                         = Tag;
    using id_t                          = IDType;

    static constexpr IDType min_id      = static_cast<IDType>(0);
    static constexpr IDType invalid_id  = InvalidID;

    explicit constexpr RawHandle(const id_t& new_id = invalid_id) noexcept;

    inline constexpr bool is_valid() const noexcept;
};

template <typename Tag, typename IDType, IDType InvalidID>
constexpr RawHandle<Tag, IDType, InvalidID>::RawHandle(const IDType& new_id) noexcept
    : HandleBase<IDType> { new_id }
{}

template <typename Tag, typename IDType, IDType InvalidID>
inline constexpr bool RawHandle<Tag, IDType, InvalidID>::is_valid() const noexcept
{
    return HandleBase<IDType>::id >= 0 && HandleBase<IDType>::id != invalid_id;
}

template <typename Tag>
using raw_handle_u32_t = RawHandle<Tag, u32, limits::max<u32>()>;

template <typename Tag>
using raw_handle_i32_t = RawHandle<Tag, i32, -1>;

#define BEE_DEFINE_RAW_HANDLE(name, type, invalid_id) struct name##Handle : public bee::RawHandle<name##Handle, type, invalid_id> {     \
    using bee::RawHandle<name##Handle, type, invalid_id>::RawHandle;                                                                    \
}

#define BEE_DEFINE_RAW_HANDLE_U32(name) struct name##Handle : public bee::raw_handle_u32_t<name##Handle> {         \
    using bee::raw_handle_u32_t<name##Handle>::RawHandle;                                                          \
}

#define BEE_DEFINE_RAW_HANDLE_I32(name) struct name##Handle : public bee::raw_handle_i32_t<name##Handle> {         \
    using bee::raw_handle_i32_t<name##Handle>::RawHandle;                                                          \
}

#define BEE_DEFINE_OPAQUE_HANDLE(struct_format, function_format) typedef struct struct_format* struct_format##Handle


/**
 * `VersionedHandle` is a wrapper around an integer that encodes a 24-bit index and an 8-bit version in it's
 * id. This allows them to be used in array-based pools where objects are stored contiguously but
 * are often created and destroyed requiring version information to determine if a handle is stale or still
 * valid, i.e. the version encoded in `id` doesn't match the version of the actual stored object.
 */
template <typename Tag>
struct VersionedHandle : public HandleBase<u32> {
    using tag_t                         = Tag;

    static constexpr u32 index_bits     = 24u;
    static constexpr u32 version_bits   = 8u;
    static constexpr u32 version_mask   = (1u << version_bits) - 1u;
    static constexpr u32 index_mask     = (1u << index_bits) - 1u;
    static constexpr u32 min_version    = 1u;
    static constexpr u32 min_id         = 1u;
    static constexpr u32 invalid_id     = 0u;

    constexpr VersionedHandle(u32 new_id = invalid_id) noexcept;

    constexpr VersionedHandle(u32 new_id, u32 new_version) noexcept;

    inline constexpr u32 index() const noexcept;
    inline constexpr u32 version() const noexcept;
    inline constexpr bool is_valid() const noexcept;
};

template <typename Tag>
constexpr VersionedHandle<Tag>::VersionedHandle(const u32 new_id) noexcept
    : HandleBase { new_id }
{}

template <typename Tag>
constexpr VersionedHandle<Tag>::VersionedHandle(const u32 index, const u32 version) noexcept
    : HandleBase { (version << index_bits) | index }
{}

template <typename Tag>
inline constexpr u32 VersionedHandle<Tag>::index() const noexcept
{
    return id & index_mask;
}

template <typename Tag>
inline constexpr u32 VersionedHandle<Tag>::version() const noexcept
{
    return (id >> index_bits) & version_mask;
}

template <typename Tag>
inline constexpr bool VersionedHandle<Tag>::is_valid() const noexcept
{
    return index() < index_mask && id > min_id;
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
#define BEE_DEFINE_VERSIONED_HANDLE(name) struct name##Handle : public bee::VersionedHandle<name##Handle> { \
    using bee::VersionedHandle<name##Handle>::VersionedHandle;                                              \
}

} // namespace bee
