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

#define BEE_DEFINE_RAW_HANDLE(name, type, invalid_id) struct name : public bee::RawHandle<name, type, invalid_id> {     \
    using bee::RawHandle<name, type, invalid_id>::RawHandle;                                                                    \
}

#define BEE_DEFINE_RAW_HANDLE_U32(name) struct name : public bee::raw_handle_u32_t<name> {  \
    using bee::raw_handle_u32_t<name>::RawHandle;                                           \
}

#define BEE_DEFINE_RAW_HANDLE_I32(name) struct name : public bee::raw_handle_i32_t<name> {  \
    using bee::raw_handle_i32_t<name>::RawHandle;                                           \
}

#define BEE_DEFINE_OPAQUE_HANDLE(struct_format, function_format) typedef struct struct_format* struct_format##Handle


/**
 * `VersionedHandle` is a wrapper around an integer that encodes a 24-bit index and an 8-bit version in it's
 * id. This allows them to be used in array-based pools where objects are stored contiguously but
 * are often created and destroyed requiring version information to determine if a handle is stale or still
 * valid, i.e. the version encoded in `id` doesn't match the version of the actual stored object.
 */
template <typename Tag, typename IdType, IdType IndexBits, IdType VersionBits>
struct VersionedHandle
{
    using tag_t = Tag;

    static constexpr IdType index_bits     = IndexBits;
    static constexpr IdType version_bits   = VersionBits;
    static constexpr IdType version_mask   = (static_cast<IdType>(1) << version_bits) - static_cast<IdType>(1);
    static constexpr IdType index_mask     = (static_cast<IdType>(1) << index_bits) - static_cast<IdType>(1);
    static constexpr IdType min_version    = static_cast<IdType>(1);
    static constexpr IdType invalid_id     = limits::max<IdType>();

    IdType id { invalid_id };

    constexpr VersionedHandle() noexcept = default;

    explicit constexpr VersionedHandle(const IdType new_id) noexcept
        : id(new_id)
    {}

    constexpr VersionedHandle(const IdType index, const IdType version) noexcept
        : id((version << index_bits) | index)
    {}

    inline constexpr IdType index() const noexcept
    {
        return id & index_mask;
    }

    inline constexpr IdType version() const noexcept
    {
        return (id >> index_bits) & version_mask;
    }

    inline constexpr bool is_valid() const noexcept
    {
        return index() < index_mask && id < invalid_id;
    }
};

template <typename Tag, typename IdType, IdType IndexBits, IdType VersionBits>
inline constexpr bool operator==(const VersionedHandle<Tag, IdType, IndexBits, VersionBits>& lhs, const VersionedHandle<Tag, IdType, IndexBits, VersionBits>& rhs)
{
    return lhs.id == rhs.id;
}

template <typename Tag, typename IdType, IdType IndexBits, IdType VersionBits>
inline constexpr bool operator!=(const VersionedHandle<Tag, IdType, IndexBits, VersionBits>& lhs, const VersionedHandle<Tag, IdType, IndexBits, VersionBits>& rhs)
{
    return !(lhs == rhs);
}



template <typename Tag>
using versioned_handle_32_t = VersionedHandle<Tag, u32, 24u, 8u>;

template <typename Tag>
using versioned_handle_64_t = VersionedHandle<Tag, u64, 48u, 16u>;


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
#define BEE_VERSIONED_HANDLE_32(name) struct name : public bee::versioned_handle_32_t<name> {   \
    using bee::versioned_handle_32_t<name>::VersionedHandle;                                    \
}

#define BEE_VERSIONED_HANDLE_64(name) struct name : public bee::versioned_handle_64_t<name> {   \
    using bee::versioned_handle_64_t<name>::VersionedHandle;                                    \
}



} // namespace bee
