/*
 *  Thread.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/String.hpp"


#if BEE_OS_UNIX == 1
    #include <pthread.h>
#endif // BEE_OS_*

#ifdef BEE_ENABLE_RELACY
    #include <relacy/context.hpp>
#endif // BEE_ENABLE_RELACY


namespace bee {


using thread_id_t = u64;


enum class ThreadPriority
{
    idle,
    lowest,
    below_normal,
    normal,
    above_normal,
    highest,
    time_critical,
    unknown
};


BEE_CORE_API u64 main_thread_id();


namespace current_thread {

/*
 * Thread helper functions
 */


BEE_CORE_API void sleep(u64 ticks_to_sleep);

#ifndef BEE_ENABLE_RELACY
    BEE_CORE_API thread_id_t id();
#else
    BEE_FORCE_INLINE thread_id_t id() { return static_cast<thread_id_t>(rl::thread_index()); }
#endif // BEE_ENABLE_RELACY

BEE_CORE_API void set_affinity(i32 cpu);

BEE_CORE_API void set_name(const char* name);

BEE_CORE_API void set_priority(ThreadPriority priority);

BEE_CORE_API void set_as_main();

BEE_CORE_API bool is_main();


} // namespace current_thread


#define BEE_ASSERT_MAIN_THREAD() BEE_ASSERT(::bee::current_thread::is_main())

#ifndef BEE_THREAD_MAX_NAME
    #if BEE_OS_WINDOWS == 1
        #define BEE_THREAD_MAX_NAME 64
    #else
        #define BEE_THREAD_MAX_NAME 16 // 16: based on the limit for pthread platforms
    #endif // BEE_OS_WINDOWS == 1
#endif // BEE_THREAD_MAX_NAME


/*
 * Abstracts a native thread type (pthread etc.)
 */

struct ThreadCreateInfo
{
    const char*     name { nullptr };
    ThreadPriority  priority { ThreadPriority::normal };

    // automatically registers the thread with the global temp allocator and unregisters when done
    bool            use_temp_allocator { false };
};


class BEE_CORE_API Thread : public Noncopyable
{
public:
    static constexpr i32 affinity_none = 0;

    using native_thread_t =
    #if BEE_OS_UNIX == 1
    pthread_t;
    #elif BEE_OS_WINDOWS == 1
    void*;
    #endif // BEE_OS_*

    using user_data_t   = void*;
    using function_t    = void(*)(user_data_t user_data);

    Thread() = default;

    template <typename CallableType, typename ArgType>
    Thread(const ThreadCreateInfo& create_info, CallableType&& callable, const ArgType& data)
    {
        using decayed_callable_t = std::decay_t<CallableType>;
        const auto params_size = sizeof(ExecuteParams) + sizeof(decayed_callable_t) + round_up(sizeof(ArgType), 64);
        auto params = static_cast<ExecuteParams*>(BEE_MALLOC(system_allocator(), params_size));

        params->invoker = [](void* function, void* arg)
        {
            (*static_cast<decayed_callable_t*>(function))(*static_cast<ArgType*>(arg));
        };

        params->destructor = [](void* function, void* arg)
        {
            static_cast<decayed_callable_t*>(function)->~decayed_callable_t();
            static_cast<ArgType*>(arg)->~ArgType();
        };

        auto params_bytes = reinterpret_cast<u8*>(params);
        params->function = params_bytes + sizeof(ExecuteParams);
        params->arg = params_bytes + sizeof(ExecuteParams) + sizeof(decayed_callable_t);

        new (params->function) decayed_callable_t { std::forward<CallableType>(callable) };
        new (params->arg) ArgType(data);

        init(create_info, params);
    }

    template <typename CallableType>
    Thread(const ThreadCreateInfo& create_info, CallableType&& callable)
    {
        using decayed_callable_t = std::decay_t<CallableType>;
        const auto params_size = sizeof(ExecuteParams) + sizeof(decayed_callable_t);
        auto params = static_cast<ExecuteParams*>(BEE_MALLOC(system_allocator(), params_size));

        params->invoker = [](void* function, void* /* arg */)
        {
            (*static_cast<decayed_callable_t*>(function))();
        };

        params->destructor = [](void* function, void* /* arg */)
        {
            static_cast<decayed_callable_t*>(function)->~decayed_callable_t();
        };

        auto params_bytes = reinterpret_cast<u8*>(params);
        params->function = params_bytes + sizeof(ExecuteParams);

        new (params->function) decayed_callable_t { std::forward<CallableType>(callable) };

        init(create_info, params);
    }

    ~Thread();

    Thread(Thread&& other) noexcept;

    Thread& operator=(Thread&& other) noexcept;

    inline const char* name() const
    {
        return name_.c_str();
    }

    void join();

    void detach();

    void set_affinity(i32 cpu);

    void set_priority(ThreadPriority priority);

    thread_id_t id() const;

    inline bool joinable() const
    {
        return native_thread_ != nullptr;
    }
private:

    #if BEE_OS_UNIX == 1
    using execute_cb_return_t = void*;
    #elif BEE_OS_WINDOWS == 1
    using execute_cb_return_t = unsigned long;
    #endif // BEE_OS_*

    struct ExecuteParams
    {
        void(*invoker)(void*, void*) { nullptr };
        void(*destructor)(void*, void*) { nullptr };

        void*   function { nullptr };
        void*   arg { nullptr };
        bool    register_with_temp_allocator { false };
    };

    StaticString<BEE_THREAD_MAX_NAME>   name_;
    native_thread_t                     native_thread_ { nullptr };

    void init(const ThreadCreateInfo& create_info, ExecuteParams* params);

    void move_construct(Thread& other) noexcept;

    void create_native_thread(ExecuteParams* params) noexcept;

    static execute_cb_return_t execute_cb(void* params);
};


} // namespace bee
