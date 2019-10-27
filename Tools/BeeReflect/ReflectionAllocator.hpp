/*
 *  ReflectionAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/ReflectionV2.hpp"

#include <llvm/ADT/StringRef.h>

namespace bee {


class ReflectionAllocator
{
public:
    ReflectionAllocator(const size_t type_capacity, const size_t name_capacity)
        : type_allocator_(type_capacity),
          name_allocator_(name_capacity)
    {}

    template <typename T>
    T* allocate_type()
    {
        static_assert(std::is_base_of_v<Type, T>, "Cannot allocate a type that doesn't derive from bee::Type");
        return BEE_NEW(type_allocator_, T);
    }

    const char* allocate_name(const llvm::StringRef& src)
    {
        static constexpr const char* empty_string = "";

        auto data = static_cast<char*>(BEE_MALLOC(name_allocator_, src.size() + 1));
        if (data == nullptr)
        {
            return empty_string;
        }

        memcpy(data, src.data(), src.size());
        data[src.size()] = '\0';
        return data;
    }
private:
    LinearAllocator         type_allocator_;
    LinearAllocator         name_allocator_;
};


} // namespace bee