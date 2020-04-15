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
template <typename T>
struct AllocatorPtr
{
    using pointer_t     = T*;
    using reference_t   = T&;

    pointer_t   pointer { nullptr };
    Allocator*  allocator { nullptr };

    AllocatorPtr() = default;

    AllocatorPtr(T* new_pointer, Allocator* new_allocator)
        : pointer(new_pointer),
          allocator(new_allocator)
    {}

    void deallocate()
    {
        if (pointer == nullptr && allocator == nullptr)
        {
            return;
        }

        BEE_ASSERT(pointer != nullptr && allocator != nullptr && allocator->is_valid(pointer));
        BEE_DELETE(allocator, pointer);
        allocator = nullptr;
        pointer = nullptr;
    }

    pointer_t operator->() const noexcept
    {
        return pointer;
    }

    reference_t operator*() const noexcept
    {
        BEE_ASSERT(pointer_ != nullptr);
        return *pointer_;
    }
};

template <typename LHSType, typename RHSType>
inline bool operator==(const AllocatorPtr<LHSType>& lhs, const AllocatorPtr<RHSType>& rhs) noexcept
{
    return lhs.pointer == rhs.pointer && lhs.allocator == rhs.allocator;
}

template <typename T>
inline bool operator==(const AllocatorPtr<T>& lhs, nullptr_t) noexcept
{
    return lhs.pointer == nullptr;
}

template <typename T>
inline bool operator==(nullptr_t, const AllocatorPtr<T>& rhs) noexcept
{
    return rhs.pointer == nullptr;
}

template <typename LHSType, typename RHSType>
inline bool operator!=(const AllocatorPtr<LHSType>& lhs, const AllocatorPtr<RHSType>& rhs) noexcept
{
    return lhs.pointer != rhs.pointer || lhs.allocator != rhs.allocator;
}

template <typename T>
inline bool operator!=(const AllocatorPtr<T>& lhs, nullptr_t) noexcept
{
    return lhs.pointer != nullptr; // NOLINT
}

template <typename T>
inline bool operator!=(nullptr_t, const AllocatorPtr<T>& rhs) noexcept
{
return nullptr != rhs;
}

/**
 * A unique pointer owned by an allocator instance - deallocates the owned pointer when ~UniquePtr() is called
 */
template <typename T>
class UniquePtr : public Noncopyable
{
public:
    using pointer_t     = typename AllocatorPtr<T>::pointer_t;
    using reference_t   = typename AllocatorPtr<T>::reference_t;

    constexpr UniquePtr() noexcept = default;

    constexpr UniquePtr(nullptr_t) noexcept // NOLINT
        : owned_(nullptr, nullptr)
    {}

    UniquePtr(pointer_t pointer, Allocator* allocator) noexcept
    {
        reset(pointer, allocator);
    }

    template <typename U>
    explicit UniquePtr(UniquePtr<U>&& other) noexcept
    {
        owned_.deallocate();

        auto other_ptr = other.release();
        owned_.allocator = other_ptr.allocator;
        owned_.pointer = other_ptr.pointer;
    }

    UniquePtr(UniquePtr<T>&& other) noexcept
    {
        owned_.deallocate();
        owned_ = other.release();
    }

    ~UniquePtr()
    {
        owned_.deallocate();
    }

    template <typename U>
    UniquePtr& operator=(UniquePtr<U>&& other) noexcept
    {
        owned_.deallocate();

        auto other_ptr = other.release();
        owned_.allocator = other_ptr.allocator;
        owned_.pointer = other_ptr.pointer;
        return *this;
    }

    UniquePtr& operator=(UniquePtr<T>&& other) noexcept
    {
        owned_.deallocate();
        owned_ = other.release();
        return *this;
    }

    inline void reset(pointer_t pointer, Allocator* allocator) noexcept
    {
        owned_.deallocate();
        new (&owned_) AllocatorPtr<T>(pointer, allocator);
    }

    inline AllocatorPtr<T> release() noexcept
    {
        auto ptr = owned_;
        owned_.pointer = nullptr;
        owned_.allocator = nullptr;
        return ptr;
    }

    inline pointer_t get() const noexcept
    {
        return owned_.pointer;
    }

    explicit operator bool() const noexcept
    {
        return owned_.allocator != nullptr && owned_.allocator->is_valid(owned_.pointer);
    }

    pointer_t operator->() const noexcept
    {
        return owned_.pointer;
    }

    reference_t operator*() const noexcept
    {
        BEE_ASSERT(owned_ != nullptr);
        return *owned_.pointer;
    }
private:
    AllocatorPtr<T> owned_;
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
    return lhs.owned_ == rhs.owned_;
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
