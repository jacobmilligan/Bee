/*
 *  Reflection.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/BSC.hpp"

namespace bee {


bool reflect_shader(BSCModule* module, BSCShaderType type, const u8* spirv, i32 spirv_length, Allocator* allocator = system_allocator());


} // namespace bee