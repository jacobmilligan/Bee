/*
 *  SmartPointers.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Atomic.hpp"


namespace bee {


/*
 * Pointer types - These are not designed to be STL compatible but instead for Bees memory/allocator
 * model.
 * TODO(Jacob): STL compatibility header
 */


/**
 * A unique pointer owned by an allocator instance - deallocates the owned pointer when ~UniquePtr() is called
 */
template <typename T>
class UniquePtr : public Noncopyable
{
public:
    using pointer_t     = T*;
    using reference_t   = T&;

    constexpr UniquePtr() noexcept = default;

    constexpr UniquePtr(nullptr_t) noexcept // NOLINT
        : pointer_(nullptr),
          allocator_(nullptr)
    {}

    UniquePtr(const UniquePtr<T>& other) = delete;

    UniquePtr(pointer_t pointer, Allocator* allocator) noexcept
    {
        reset(pointer, allocator);
    }

    UniquePtr(UniquePtr<T>&& other) noexcept
    {
        reset(other.pointer_, other.allocator_);
        other.pointer_ = nullptr;
        other.allocator_ = nullptr;
    }

    ~UniquePtr()
    {
        if (allocator_ != nullptr && pointer_ != nullptr && allocator_->is_valid(pointer_))
        {
            BEE_DELETE(allocator_, pointer_);
        }
    }

    UniquePtr<T>& operator=(const UniquePtr<T>& other) = delete;

    UniquePtr<T>& operator=(UniquePtr<T>&& other) noexcept
    {
        reset(other.pointer_, other.allocator_);
        other.pointer_ = nullptr;
        other.allocator_ = nullptr;
        return *this;
    }

    inline void reset(pointer_t pointer, Allocator* allocator) noexcept
    {
        BEE_ASSERT(allocator != nullptr);
        BEE_ASSERT(allocator->is_valid(pointer));

        if (allocator_ != nullptr && pointer_ != nullptr && allocator_->is_valid(pointer_))
        {
            BEE_DELETE(allocator_, pointer_);
        }

        pointer_ = pointer;
        allocator_ = allocator;
    }

    inline pointer_t get() const noexcept
    {
        return pointer_;
    }

    explicit operator bool() const noexcept
    {
        return allocator_ != nullptr && allocator_->is_valid(pointer_);
    }

    pointer_t operator->() const noexcept
    {
        return pointer_;
    }

    reference_t operator*() const noexcept
    {
        BEE_ASSERT(pointer_ != nullptr);
        return *pointer_;
    }
private:
    pointer_t   pointer_ { nullptr };
    Allocator*  allocator_ { nullptr };
};


/*
 ************************************************************
 *
 * UniquePtr - Non-member operators and functions
 *
 ************************************************************
 */
template <typename LHSType, typename RHSType>
inline bool operator==(const UniquePtr<LHSType>& lhs, const UniquePtr<RHSType>& rhs) noexcept
{
    return lhs.pointer_ == rhs.pointer_ && lhs.allocator_ == rhs.allocator_;
}

template <typename T>
inline bool operator==(const UniquePtr<T>& lhs, nullptr_t) noexcept
{
    return !lhs;
}

template <typename T>
inline bool operator==(nullptr_t, const UniquePtr<T>& rhs) noexcept
{
    return !rhs;
}

template <typename LHSType, typename RHSType>
inline bool operator!=(const UniquePtr<LHSType>& lhs, const UniquePtr<RHSType>& rhs) noexcept
{
    return !(lhs == rhs); // NOLINT
}

template <typename T>
inline bool operator!=(const UniquePtr<T>& lhs, nullptr_t) noexcept
{
    return !(lhs == nullptr); // NOLINT
}

template <typename T>
inline bool operator!=(nullptr_t, const UniquePtr<T>& rhs) noexcept
{
    return !(nullptr == rhs);
}


/*
 ************************************************************
 *
 * UniquePtr - Helper functions
 *
 ************************************************************
 */
template <typename T, typename... Args>
inline UniquePtr<T> make_unique(Allocator* allocator, Args&& ...args) noexcept
{
    BEE_ASSERT(allocator != nullptr);
    return UniquePtr<T>(BEE_NEW(allocator, T)(std::forward<Args>(args)...), allocator);
}

template <typename T, typename... Args>
inline UniquePtr<T> make_unique(Allocator& allocator, Args&& ...args) noexcept
{
    return make_unique<T>(&allocator, std::forward<Args>(args)...);
}


/**
 * `RefCountPtr` - intrusive, reference counted wrapper around some data. Calls `add_ref` and `release_ref` on the
 * objects ptr. This means that the type `T` must have these two functions defined to be valid
 */
template <typename T>
class RefCountPtr
{
public:
    using pointer_t     = T*;
    using reference_t   = T&;

    constexpr RefCountPtr() noexcept = default;

    constexpr RefCountPtr(nullptr_t) noexcept // NOLINT
        : pointer_(nullptr)
    {}

    RefCountPtr(pointer_t pointer) noexcept // NOLINT
    {
        reset(pointer);
    }

    RefCountPtr(const RefCountPtr<T>& other)
    {
        reset(other.pointer_);
    }

    RefCountPtr(RefCountPtr<T>&& other) noexcept
    {
        reset();
        swap(other);
    }

    ~RefCountPtr()
    {
        reset();
    }

    RefCountPtr<T>& operator=(const RefCountPtr<T>& other)
    {
        reset(other.pointer_);
        return *this;
    }

    RefCountPtr<T>& operator=(RefCountPtr<T>&& other) noexcept
    {
        reset();
        swap(other);
        return *this;
    }

    inline void reset() noexcept
    {
        if (pointer_ != nullptr)
        {
            pointer_->release_ref();
        }
        pointer_ = nullptr;
    }

    inline void reset(pointer_t pointer) noexcept
    {
        reset();

        if (pointer != nullptr)
        {
            pointer->add_ref();
        }

        pointer_ = pointer;
    }

    inline pointer_t get() const noexcept
    {
        return pointer_;
    }

    explicit operator bool() const noexcept
    {
        return pointer_ != nullptr;
    }

    pointer_t operator->() const noexcept
    {
        return get();
    }

    reference_t operator*() const noexcept
    {
        BEE_ASSERT(pointer_ != nullptr);
        return *get();
    }

    void swap(RefCountPtr<T>& other) noexcept
    {
        auto temp = other.pointer_;
        other.pointer_ = pointer_;
        pointer_ = temp;
    }
private:
    pointer_t pointer_ { nullptr };
};

template <typename T>
inline bool operator==(const RefCountPtr<T>& lhs, const RefCountPtr<T>& rhs)
{
    return lhs.get() == rhs.get();
}

template <typename T>
inline bool operator!=(const RefCountPtr<T>& lhs, const RefCountPtr<T>& rhs)
{
    return !(lhs == rhs);
}

template <typename T>
inline bool operator==(const RefCountPtr<T>& lhs, nullptr_t rhs)
{
    return lhs.get() == rhs;
}

template <typename T>
inline bool operator!=(const RefCountPtr<T>& lhs, nullptr_t rhs)
{
    return lhs.get() != rhs;
}

template <typename T>
inline bool operator==(nullptr_t rhs, const RefCountPtr<T>& lhs)
{
    return lhs == rhs.get();
}

template <typename T>
inline bool operator!=(nullptr_t rhs, const RefCountPtr<T>& lhs)
{
    return lhs != rhs.get();
}


template <typename T>
class RefCounter
{
public:
    RefCounter() = default;

    virtual ~RefCounter() {}

    void add_ref()
    {
        ++refcount_;
    }

    void release_ref()
    {
        BEE_ASSERT_F(refcount_ > 0, "`release_ref` was called on an object with zero reference counts");

        --refcount_;
        if (refcount_ == 0)
        {
            refcount_ = 0;
            static_cast<T*>(this)->~T();
        }
    }

    i32 refcount() const
    {
        return refcount_;
    }
private:
    i32 refcount_ { 0 };
};


template <typename T>
class AtomicRefCounter
{
public:
    AtomicRefCounter() = default;

    virtual ~AtomicRefCounter() {}

    void add_ref()
    {
        refcount_.fetch_add(1, std::memory_order_release);
    }

    void release_ref()
    {
        const auto count = refcount_.fetch_sub(1, std::memory_order_release);
        BEE_ASSERT_F(count >= 1, "`release_ref` was called on an object with zero reference counts");

        if (count <= 1)
        {
            refcount_.store(0, std::memory_order_release);
            static_cast<T*>(this)->~T();
        }
    }

    i32 refcount() const
    {
        return refcount_.load(std::memory_order_acquire);
    }
private:
    std::atomic_int32_t refcount_ { 0 };
};


} // namespace bee
