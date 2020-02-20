/*
 *  Renderer.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Math/Math.hpp"


namespace bee {


struct BEE_REFLECT() RenderData
{

};

struct RenderModule
{
    virtual void execute() = 0;
};




} // namespace bee