/*
 *  CodeGen.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/IO.hpp"

#include <time.h>
#include <llvm/ADT/ArrayRef.h>

namespace bee {
namespace reflect {


struct ReflectedFile;

enum class CodegenMode
{
    cpp,
    inl,
    templates_only
};

void dump_reflection_module(const StringView& name, const Path& path, const ReflectedFile* files, const i32 count);


} // namespace reflect
} // namespace bee