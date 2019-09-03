/*
 *  MainLoop.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Error.hpp"

#include <type_traits>

namespace bee {


#define BEE_ASSERT_LOOP_CALLBACK(info, callback)                                            \
    BEE_BEGIN_MACRO_BLOCK                                                                   \
        if (BEE_FAIL_F(info.callback != nullptr, "Missing app loop callback: " #callback))  \
        {                                                                                   \
            return EXIT_FAILURE;                                                            \
        }                                                                                   \
    BEE_END_MACRO_BLOCK


struct AppContext
{
    bool quit { false };
};

template <typename T>
struct AppCreateInfo
{
    const char* app_name { nullptr };

    int (*init)(T* ctx);
    void (*destroy)(T* ctx);
    void (*tick)(T* ctx);
};


int __internal_app_launch(const char* app_name);

void __internal_app_tick();

void __internal_app_shutdown();


template <typename ContextType>
int create_and_run_app(const AppCreateInfo<ContextType>& info)
{
    static_assert(std::is_base_of<AppContext, ContextType>::value, "Your game app context must derive from bee::AppContext");

    BEE_ASSERT_LOOP_CALLBACK(info, init);
    BEE_ASSERT_LOOP_CALLBACK(info, destroy);
    BEE_ASSERT_LOOP_CALLBACK(info, tick);

    auto init_result = __internal_app_launch(info.app_name);
    if (init_result != EXIT_SUCCESS)
    {
        return init_result;
    }

    ContextType ctx; // alive for the lifetime of the app

    init_result = info.init(&ctx);
    if (init_result != EXIT_SUCCESS)
    {
        return init_result;
    }

    while (!ctx.quit)
    {
        __internal_app_tick();
        info.tick(&ctx);
    }


    return EXIT_SUCCESS;
}


} // namespace bee