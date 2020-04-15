/*
 *  Function.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Meta.hpp"


namespace bee {


/*
 * Invoke adapted from minimal implementation as described here:
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4169.html
 */

// member function specialization with a reference or value instance
template <typename CallableType, typename ClassType, typename... Args>
constexpr auto invoke(CallableType&& callable, ClassType&& class_instance, Args&&... args) noexcept ->
typename std::enable_if_t<
    std::is_member_function_pointer_v<typename std::decay_t<CallableType>>, // is a member function pointer
    decltype(((std::forward<ClassType>(class_instance)).*callable)(std::forward<Args>(args)...))
>
{
    return ((std::forward<ClassType>(class_instance)).*callable)(std::forward<Args>(args)...);
}

// member function specialization with a reference/pointer parameter
template <typename CallableType, typename ClassType, typename... Args>
constexpr auto invoke(CallableType&& callable, ClassType&& class_instance, Args&&... args) noexcept ->
typename std::enable_if_t<
    std::is_member_function_pointer_v<typename std::decay_t<CallableType>>, // is a member function pointer
    decltype(((*std::forward<ClassType>(class_instance)).*callable)(std::forward<Args>(args)...))
>
{
    return ((*std::forward<ClassType>(class_instance)).*callable)(std::forward<Args>(args)...);
}

// Free-function specialization - Not a member pointer implies also not a member function pointer
template <typename CallableType, typename... Args>
constexpr auto invoke(CallableType&& callable, Args&&... args) noexcept ->
typename std::enable_if_t<
    !std::is_member_pointer_v<typename std::decay_t<CallableType>>,
    decltype(std::forward<CallableType>(callable)(std::forward<Args>(args)...))
>
{
    return std::forward<CallableType>(callable)(std::forward<Args>(args)...);
}


namespace detail {


static constexpr i32 __default_function_buffer_size = 16;

template <i32 Size>
using __function_storage_t = u8[Size];

template <typename CallableType, typename... Args>
using __invoke_result_t = decltype(invoke(std::declval<CallableType>(), std::declval<Args>()...));

template <typename VoidType, typename CallableType, typename... Args>
struct __is_invocable
{
    static constexpr bool value = false;
};


template <typename CallableType, typename... Args>
struct __is_invocable<
    void_t<detail::__invoke_result_t<CallableType, Args...>>,
    CallableType,
    Args...
>
{
    static constexpr bool value = true;
};

template <typename VoidType, typename ReturnType, typename CallableType, typename... Args>
struct __is_invocable_r {
    static constexpr bool value = false;
};

template <typename ReturnType, typename CallableType, typename... Args>
struct __is_invocable_r<
    void_t<detail::__invoke_result_t<CallableType, Args...>>,
    ReturnType,
    CallableType,
    Args...
>
{
    static constexpr bool value = std::is_convertible<detail::__invoke_result_t<CallableType, Args...>, ReturnType>::value;
};


} // namespace detail

/**
 * Determines whether the given callable `CallableType` can be invoked with the set of arguments `Args`
 */
template <typename CallableType, typename... Args>
struct is_invocable
{
    static constexpr auto value = detail::__is_invocable<void, CallableType, Args...>::value;
};

template <typename CallableType, typename... Args>
constexpr bool is_invocable_v = is_invocable<CallableType, Args...>::value;

/**
 * Determines whether the given callable `CallableType` can be invoked with the set of arguments `Args` and returns a
 * value with type `ReturnType`
 */
template <typename ReturnType, typename CallableType, typename... Args>
struct is_invocable_r
{
    static constexpr auto value = detail::__is_invocable_r<void, ReturnType, CallableType, Args...>::value;
};

template <typename ReturnType, typename CallableType, typename... Args>
constexpr bool is_invocable_r_v = is_invocable_r<ReturnType, CallableType, Args...>::value;


template <
    typename Signature,
    i32 BufferSize = detail::__default_function_buffer_size,
    i32 Alignment = alignof(detail::__function_storage_t<BufferSize>)
>
class Function; // undefined

/**
 * A function wrapper object with fixed-size, statically allocated storage (i.e. a c-style array) and user-defined
 * memory alignment. By default, the internal buffer size is 32 bytes with alignment equivalent to alignof(u8[32]).
 */
template <typename ReturnType, typename... Args, i32 BufferSize, i32 Alignment>
class Function<ReturnType(Args...), BufferSize, Alignment> {
private:
    using storage_t = detail::__function_storage_t<BufferSize>;
    using destructor_t = void(*)(storage_t& storage);
    using invoker_t = ReturnType(*)(storage_t& storage, Args&&... args);
    using this_t = Function<ReturnType(Args...), BufferSize, Alignment>;

    template <typename CallableType>
    using enable_if_callable_t = std::enable_if_t<!std::is_same_v<std::decay_t<CallableType>, this_t>>;
public:
    Function() noexcept = default;

    template <typename CallableType, typename = enable_if_callable_t<CallableType>>
    Function(CallableType&& callable) noexcept
    {
        using decayed_callable_t = std::decay_t<CallableType>;

        static_assert(sizeof(decayed_callable_t) <= BufferSize, "bee::Function: callable size was too large for the internal buffer");
        static_assert(!std::is_same_v<decayed_callable_t, Function>, "bee::Function: Cannot assign a bee::Function object to another bee::Function");
        static_assert(std::is_convertible_v<decayed_callable_t, Function>, "bee::Function: Callable parameter must be convertible to a bee::Function");
        static_assert(is_invocable_r<ReturnType, decayed_callable_t, Args...>::value, "bee::Function: Callable parameter must be invocable with the given return type and arguments");

        destructor_ = [](storage_t& storage)
        {
            auto stored_callable = reinterpret_cast<decayed_callable_t*>(storage);
            stored_callable->~decayed_callable_t();
        };

        invoker_ = [](storage_t& storage, Args&& ... args) -> ReturnType
        {
            auto stored_callable = *reinterpret_cast<decayed_callable_t*>(storage);
            return stored_callable(std::forward<Args>(args)...);
        };

        new (storage_) decayed_callable_t { std::forward<CallableType>(callable) };
    }

    Function(const Function& other) noexcept
    {
        copy_construct(other);
    }

    Function(Function&& other) noexcept
    {
        move_construct(other);
    }

    ~Function() noexcept
    {
        destruct_storage();
    }

    Function& operator=(const Function<ReturnType(Args...), BufferSize, Alignment>& other) noexcept
    {
        copy_construct(other);
        return *this;
    }

    Function& operator=(Function<ReturnType(Args...), BufferSize, Alignment>&& other) noexcept
    {
        move_construct(other);
        return *this;
    }

    ReturnType operator()(Args... args) noexcept
    {
        BEE_ASSERT_F(invoker_ != nullptr, "Attempted to call an undefined or invalid bee::Function object");
        return invoker_(storage_, std::forward<Args>(args)...);
    }
private:
    alignas(Alignment) storage_t    storage_{}; // must come first to ensure byte-alignment of function
    destructor_t                    destructor_ { nullptr };
    invoker_t                       invoker_ { nullptr };

    void destruct_storage()
    {
        if (destructor_ != nullptr)
        {
            destructor_(storage_);
        }
    }

    void copy_construct(const Function<ReturnType(Args...), BufferSize, Alignment>& other)
    {
        destruct_storage();
        memcpy(storage_, other.storage_, sign_cast<size_t>(BufferSize));
        invoker_ = other.invoker_;
        destructor_ = other.destructor_;
    }

    void move_construct(Function<ReturnType(Args...), BufferSize, Alignment>& other)
    {
        destruct_storage();
        memcpy(storage_, other.storage_, sign_cast<size_t>(BufferSize));
        memset(other.storage_, 0, sign_cast<size_t>(BufferSize));
        invoker_ = std::move(other.invoker_);
        destructor_ = std::move(other.destructor_);
    }
};


} // namespace bee