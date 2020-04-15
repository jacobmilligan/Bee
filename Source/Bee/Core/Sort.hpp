/*
 *  Sort.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {


/*
 *********************************************************************************************************
 *
 * Radix sort implementations based on:
 * - http://stereopsis.com/radix.html
 * - https://probablydance.com/2016/12/02/investigating-radix-sort/
 *
 * The implementations below are very similar to the ones outlined above.
 * We explicitly use one array to hold all the buckets rather than using
 * one array per bucket to minimize cache misses when going to the next
 * histogram bucket. In the second article above, going from i.e. bucket 0 to
 * bucket 1 incurs an L1 miss. This is in-line with the findings in the second
 * article above - note I experimented with using _mm_prefetch to explicitly fetch
 * cache lines but apparently modern hardware is far better these days and that particular
 * optimization didn't make a difference.
 *
 * According to my benchmarks this implementation of radix sort is ~2.5x faster than
 * the above implementations (the compiler is really good at vectorizing this code)
 * and also ~5x-10x faster than std::sort on all data sets I looked at (array sizes of 2-2^32 items).
 *
 *********************************************************************************************************
 */
template <typename T, typename KeyFunc>
inline void radix_sort8(T* inputs, T* outputs, const u64 count, KeyFunc&& key_func)
{
    using integral_t = u8;

    static constexpr auto histogram_buckets = 1u;
    static constexpr auto histogram_size = 256u * histogram_buckets;
    static constexpr auto histogram_mask = histogram_size - 1u;

    integral_t histogram[histogram_buckets * histogram_size] = {};

    auto bucket_0 = histogram;

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<integral_t>(key_func(inputs[i]));
        ++bucket_0[key & histogram_mask];
    }

    integral_t sum_0 = 0;
    integral_t total_sum = 0;

    for (int hist = 0; hist < histogram_size; ++hist)
    {
        const auto old_sum_0 = bucket_0[hist];
        bucket_0[hist] = total_sum;
        sum_0 += old_sum_0;
    }

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<u8>(key_func(inputs[i]));
        outputs[bucket_0[key]++] = std::move(inputs[i]);
    }
}

template <typename T, typename KeyFunc>
inline void radix_sort16(T* inputs, T* outputs, const u64 count, KeyFunc&& key_func)
{
    using integral_t = u16;

    static constexpr auto histogram_buckets = 2u;
    static constexpr auto histogram_size = 256u * histogram_buckets;
    static constexpr auto histogram_mask = histogram_size - 1u;

    integral_t histogram[histogram_buckets * histogram_size] = {};

    auto bucket_0 = histogram;
    auto bucket_1 = bucket_0 + histogram_size;

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<integral_t>(key_func(inputs[i]));

        ++bucket_0[key & histogram_mask];
        ++bucket_1[(key >> 8u) & histogram_mask];
    }

    integral_t sum_0 = 0;
    integral_t sum_1 = 0;

    integral_t total_sum = 0;
    for (int hist = 0; hist < histogram_size; ++hist)
    {
        const auto old_sum_0 = bucket_0[hist];
        const auto old_sum_1 = bucket_1[hist];
        bucket_0[hist] = total_sum;
        bucket_1[hist] = total_sum;
        sum_0 += old_sum_0;
        sum_1 += old_sum_1;
    }

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(inputs[i]));
        outputs[bucket_0[key]++] = std::move(inputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(outputs[i]) >> 8u);
        outputs[bucket_1[key]++] = std::move(outputs[i]);
    }
}

template <typename T, typename KeyFunc>
inline void radix_sort32(T* inputs, T* outputs, const u64 count, KeyFunc&& key_func)
{
    using integral_t = u32;

    static constexpr auto histogram_buckets = 4u;
    static constexpr auto histogram_size = 256u * histogram_buckets;
    static constexpr auto histogram_mask = histogram_size - 1u;

    integral_t histogram[histogram_buckets * histogram_size] = {};

    auto bucket_0 = histogram;
    auto bucket_1 = bucket_0 + histogram_size;
    auto bucket_2 = bucket_1 + histogram_size;
    auto bucket_3 = bucket_2 + histogram_size;

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<integral_t>(key_func(inputs[i]));

        ++bucket_0[key & histogram_mask];
        ++bucket_1[(key >> 8u) & histogram_mask];
        ++bucket_2[(key >> 16u) & histogram_mask];
        ++bucket_3[(key >> 24u) & histogram_mask];
    }

    integral_t sum_0 = 0;
    integral_t sum_1 = 0;
    integral_t sum_2 = 0;
    integral_t sum_3 = 0;

    integral_t total_sum = 0;
    for (int hist = 0; hist < histogram_size; ++hist)
    {
        const auto old_sum_0 = bucket_0[hist];
        const auto old_sum_1 = bucket_1[hist];
        const auto old_sum_2 = bucket_2[hist];
        const auto old_sum_3 = bucket_3[hist];
        bucket_0[hist] = total_sum;
        bucket_1[hist] = total_sum;
        bucket_2[hist] = total_sum;
        bucket_3[hist] = total_sum;
        sum_0 += old_sum_0;
        sum_1 += old_sum_1;
        sum_2 += old_sum_2;
        sum_3 += old_sum_3;
    }

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(inputs[i]));
        outputs[bucket_0[key]++] = std::move(inputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(outputs[i]) >> 8u);
        outputs[bucket_1[key]++] = std::move(outputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(inputs[i]) >> 16u);
        outputs[bucket_2[key]++] = std::move(inputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(outputs[i]) >> 24u);
        outputs[bucket_3[key]++] = std::move(outputs[i]);
    }
}

template <typename T, typename KeyFunc>
inline void radix_sort64(T* inputs, T* outputs, const u64 count, KeyFunc&& key_func)
{
    using integral_t = u64;

    static constexpr auto histogram_buckets = 8u;
    static constexpr auto histogram_size = 256u * histogram_buckets;
    static constexpr auto histogram_mask = histogram_size - 1u;

    integral_t histogram[histogram_buckets * histogram_size] = {};

    auto bucket_0 = histogram;
    auto bucket_1 = bucket_0 + histogram_size;
    auto bucket_2 = bucket_1 + histogram_size;
    auto bucket_3 = bucket_2 + histogram_size;
    auto bucket_4 = bucket_3 + histogram_size;
    auto bucket_5 = bucket_4 + histogram_size;
    auto bucket_6 = bucket_5 + histogram_size;
    auto bucket_7 = bucket_6 + histogram_size;

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<integral_t>(key_func(inputs[i]));

        ++bucket_0[key & histogram_mask];
        ++bucket_1[(key >> 8u) & histogram_mask];
        ++bucket_2[(key >> 16u) & histogram_mask];
        ++bucket_3[(key >> 24u) & histogram_mask];
        ++bucket_4[(key >> 32u) & histogram_mask];
        ++bucket_5[(key >> 40u) & histogram_mask];
        ++bucket_6[(key >> 48u) & histogram_mask];
        ++bucket_7[(key >> 56u) & histogram_mask];
    }

    integral_t sum_0 = 0;
    integral_t sum_1 = 0;
    integral_t sum_2 = 0;
    integral_t sum_3 = 0;
    integral_t sum_4 = 0;
    integral_t sum_5 = 0;
    integral_t sum_6 = 0;
    integral_t sum_7 = 0;

    integral_t total_sum = 0;
    for (int hist = 0; hist < histogram_size; ++hist)
    {
        const auto old_sum_0 = bucket_0[hist];
        const auto old_sum_1 = bucket_1[hist];
        const auto old_sum_2 = bucket_2[hist];
        const auto old_sum_3 = bucket_3[hist];
        const auto old_sum_4 = bucket_4[hist];
        const auto old_sum_5 = bucket_5[hist];
        const auto old_sum_6 = bucket_6[hist];
        const auto old_sum_7 = bucket_7[hist];
        bucket_0[hist] = total_sum;
        bucket_1[hist] = total_sum;
        bucket_2[hist] = total_sum;
        bucket_3[hist] = total_sum;
        bucket_4[hist] = total_sum;
        bucket_5[hist] = total_sum;
        bucket_6[hist] = total_sum;
        bucket_7[hist] = total_sum;
        sum_0 += old_sum_0;
        sum_1 += old_sum_1;
        sum_2 += old_sum_2;
        sum_3 += old_sum_3;
        sum_4 += old_sum_4;
        sum_5 += old_sum_5;
        sum_6 += old_sum_6;
        sum_7 += old_sum_7;
    }

    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(inputs[i]));
        outputs[bucket_0[key]++] = std::move(inputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(outputs[i]) >> 8u);
        outputs[bucket_1[key]++] = std::move(outputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(inputs[i]) >> 16u);
        outputs[bucket_2[key]++] = std::move(inputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(outputs[i]) >> 24u);
        outputs[bucket_3[key]++] = std::move(outputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(inputs[i]) >> 32u);
        outputs[bucket_4[key]++] = std::move(inputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(outputs[i]) >> 40u);
        outputs[bucket_5[key]++] = std::move(outputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(inputs[i]) >> 48u);
        outputs[bucket_6[key]++] = std::move(inputs[i]);
    }
    for (int i = 0; i < count; ++i)
    {
        const auto key = static_cast<uint8_t>(key_func(outputs[i]) >> 56u);
        outputs[bucket_7[key]++] = std::move(outputs[i]);
    }
}


template <typename T, typename KeyFunc>
void radix_sort(T* inputs, T* outputs, const u64 count, KeyFunc&& key_func)
{
    if (count <= (1u << 8u))
    {
        return radix_sort8(inputs, outputs, count, std::forward<KeyFunc>(key_func));
    }

    if (count <= (1u << 16u))
    {
        return radix_sort16(inputs, outputs, count, std::forward<KeyFunc>(key_func));
    }

    if (count <= (1llu << 32llu))
    {
        return radix_sort32(inputs, outputs, count, std::forward<KeyFunc>(key_func));
    }

    return radix_sort64(inputs, outputs, count, std::forward<KeyFunc>(key_func));
}

template <typename T>
void radix_sort(T* inputs, T* outputs, const u64 count)
{
    return radix_sort(inputs, outputs, count, [](const T& value) { return value; });
}


} // namespace bee
