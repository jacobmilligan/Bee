/*
 *  Base64.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/String.hpp"


namespace bee {


BEE_CORE_API i32 base64_encode(char* dst, const i32 dst_size, const u8* src, const size_t src_size);

BEE_CORE_API i32 base64_encode(String* dst, const u8* src, const size_t src_size);

BEE_CORE_API i32 base64_encode_size(const size_t byte_count);

BEE_CORE_API i32 base64_decode(u8* dst, const i32 dst_size, const StringView& src);

BEE_CORE_API i32 base64_decode(DynamicArray<u8>* dst, const StringView& src);

BEE_CORE_API i32 base64_decode_size(const StringView& src);


} // namespace bee