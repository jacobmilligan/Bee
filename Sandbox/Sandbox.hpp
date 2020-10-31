/*
 *  Sandbox.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once


namespace bee {


#define BEE_SANDBOX_MODULE_NAME "BEE_SANDBOX"

struct SandboxModule
{
    bool (*startup)() { nullptr };

    void (*shutdown)() { nullptr };

    bool (*tick)() { nullptr };

};


} // namespace bee