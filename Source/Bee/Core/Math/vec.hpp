//
//  VectorBase.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 28/9/18
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {


template <typename ValueType, i32 NumComponents>
struct vec {
    using value_t                               = ValueType;
    using component_array_t                     = float[NumComponents];
    static constexpr i32 num_components         = NumComponents;
};

template <typename VectorType, typename PtrType>
inline VectorType make_vector_from_ptr(const PtrType* ptr)
{
    static_assert(sizeof(PtrType) <= sizeof(typename VectorType::value_t),
        "make_vector_from_ptr: sizeof(PtrType) must be <= sizeof(VectorType::value_t) "
        "otherwise a narrowing conversion would occur resulting in a loss of, or incorrect, data");

    VectorType vector;
    const auto byte_stride = sizeof(typename VectorType::value_t) * VectorType::num_components;
    memcpy(vector.components, ptr, byte_stride);
    return vector;
}


} // namespace bee