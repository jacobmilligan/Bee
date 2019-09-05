/*
 *  HashMap.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

/*
 * Generalized hash map - approx. 2.9x faster than std::unordered_map, tested on 10,000,000
 * insertions/lookups/deletions (see Tests/Performance/HashMap.cpp for details).
 * Uses open addressing, linear probing, approx. 50% load factor (relative to the size of the map).
 * Uses fibonacci hashing instead of integer modulo for 1. speed and 2. extra mixing for free on
 * hash functions
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Hash.hpp"

namespace bee {


template <typename T>
struct EqualTo {
    inline bool operator()(const T& lhs, const T& rhs) const
    {
        return lhs == rhs;
    }

    template <typename EquivalentType>
    inline bool operator()(const T& lhs, const EquivalentType& rhs) const
    {
        return lhs == rhs;
    }
};

template <typename KeyType, typename ValueType>
struct KeyValuePair {
    using key_t     = KeyType;
    using value_t   = ValueType;

    KeyType key;
    ValueType value;
};

template <
    typename        KeyType,
    typename        ValueType,
    ContainerMode   Mode,
    typename        Hasher = Hash<KeyType>,
    typename        KeyEqual = EqualTo<KeyType>
>
class HashMap : public Noncopyable
{
public:
    /*
     * TODO(Jacob):
     * - Copy constructor, assignment
     * - iterator
     */

    using map_t                         = HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>;
    using hash_t                        = Hasher;
    using key_t                         = KeyType;
    using key_value_pair_t              = KeyValuePair<KeyType, ValueType>;
    using value_t                       = ValueType;
    using key_equal_t                   = KeyEqual;

    static constexpr ContainerMode mode = Mode;

    class iterator
    {
    public:
        using difference_type   = std::ptrdiff_t;
        using reference         = key_value_pair_t&;
        using pointer           = key_value_pair_t*;

        explicit iterator(const map_t* map, const i32 node_index)
            : map_(map),
              node_idx_(node_index)
        {}

        iterator(const iterator& other)
            : map_(other.map_),
              node_idx_(other.node_idx_)
        {}

        iterator& operator=(const iterator& other)
        {
            map_ = other.map_;
            node_idx_ = other.node_idx_;
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return map_ == other.map_ && node_idx_ == other.node_idx_;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

        reference operator*()
        {
            return const_cast<map_t*>(map_)->node_storage_[node_idx_].kv;
        }

        pointer operator->()
        {
            return &const_cast<map_t*>(map_)->node_storage_[node_idx_].kv;
        }

        const reference operator*() const
        {
            return map_->node_storage_[node_idx_].kv;
        }

        const pointer operator->() const
        {
            return &map_->node_storage_[node_idx_].kv;
        }

        const iterator operator++(int)
        {
            iterator result(*this);
            ++*this;
            return result;
        }

        iterator& operator++()
        {
            ++node_idx_;
            for (; node_idx_ < map_->node_storage_.size(); ++node_idx_)
            {
                if (map_->node_storage_[node_idx_].active)
                {
                    break;
                }
            }
            return *this;
        }
    private:
        const map_t*    map_ { nullptr };
        i32             node_idx_ { 0 };
    };

    explicit HashMap(Allocator* allocator = system_allocator()) noexcept
        : HashMap(0, allocator)
    {}

    explicit HashMap(const i32 initial_bucket_count, Allocator* allocator = system_allocator()) noexcept;

    HashMap(std::initializer_list<key_value_pair_t> init, Allocator* allocator = system_allocator()) noexcept;

    HashMap(map_t&& other) noexcept;

    ~HashMap() = default;

    HashMap& operator=(map_t&& other) noexcept;

    value_t& operator[](const KeyType& key) noexcept;

    key_value_pair_t* insert(const key_value_pair_t& kv_pair);

    key_value_pair_t* insert(const key_t& key, const value_t& value);

    inline key_value_pair_t* find(const key_t& key);

    inline const key_value_pair_t* find(const key_t& key) const;

    /*
     * Heterogeneous lookups
     */
    template <typename EquivalentKey>
    inline key_value_pair_t* find(const EquivalentKey& key);

    template <typename EquivalentKey>
    inline const key_value_pair_t* find(const EquivalentKey& key) const;

    inline bool erase(const key_t& key);

    /*
     * Heterogeneous erase
     */
    template <typename EquivalentKey>
    inline bool erase(const EquivalentKey& key);

    inline void clear();

    void rehash(const i32 new_count);

    inline iterator begin() const
    {
        if (active_node_count_ == 0)
        {
            return end();
        }

        int first_node = 0;

        for (; first_node < node_storage_.capacity(); ++first_node)
        {
            if (node_storage_[first_node].active)
            {
                break;
            }
        }

        return iterator(this, first_node);
    }

    inline iterator end() const
    {
        return iterator(this, node_storage_.capacity());
    }

    inline i32 size() const
    {
        return sign_cast<i32>(active_node_count_);
    }
private:
    struct Node
    {
        bool                active { false };
        key_value_pair_t    kv;
    };

    using storage_t                     = Array<Node, mode>;
    static constexpr u32 min_capacity_  = 4;

    hash_t          hasher_ { hash_t() };
    key_equal_t     key_comparer_ { key_equal_t() };
    storage_t       node_storage_;
    u32             hash_shift_ { 32 };
    u32             load_factor_ { 0 };
    u32             active_node_count_ { 0 };

    key_value_pair_t* insert_no_construct(const key_t& key);

    template <typename EquivalentKey>
    u32 hash_key(const EquivalentKey& key, const u32 hash_shift, const u32 capacity) const;

    template <typename EquivalentKey>
    u32 find_key_index(const EquivalentKey& key) const;

    template <typename EquivalentKey>
    const key_value_pair_t* find_internal(const EquivalentKey& key) const;

    template <typename EquivalentKey>
    bool erase_internal(const EquivalentKey& key);

    inline void destroy_node(Node* node)
    {
        node->kv.value.~value_t();
        node->active = false;
    }

    inline constexpr u32 next_growth_capacity()
    {
        return math::max(min_capacity_, node_storage_.size() * 2u);
    }

    constexpr bool implicit_grow(const fixed_container_mode_t& fixed_capacity);

    constexpr bool implicit_grow(const dynamic_container_mode_t& fixed_capacity);
};

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
constexpr u32 HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::min_capacity_;

/*
 * Hashmap typedefs for convenience - fixed-capacity and dynamic-capacity versions
 */
template <
    typename        KeyType,
    typename        ValueType,
    typename        Hasher = Hash<KeyType>,
    typename        KeyEqual = EqualTo<KeyType>
>
using FixedHashMap = HashMap<KeyType, ValueType, ContainerMode::fixed_capacity, Hasher, KeyEqual>;

template <
    typename        KeyType,
    typename        ValueType,
    typename        Hasher = Hash<KeyType>,
    typename        KeyEqual = EqualTo<KeyType>
>
using DynamicHashMap = HashMap<KeyType, ValueType, ContainerMode::dynamic_capacity, Hasher, KeyEqual>;

/*
 *****************************************
 *
 * HashMap constructors and destructors
 *
 *****************************************
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::HashMap(const i32 initial_bucket_count, Allocator* allocator) noexcept
    : hasher_(hash_t()),
      key_comparer_(key_equal_t()),
      node_storage_(allocator),
      hash_shift_(32),
      load_factor_(0),
      active_node_count_(0)
{
    BEE_ASSERT_F(initial_bucket_count >= 0, "HashMap: `initial_bucket_count` must be >= 0");

    if (initial_bucket_count > 0)
    {
        rehash(math::to_next_pow2(sign_cast<u32>(initial_bucket_count)));
    }
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::HashMap(std::initializer_list<HashMap::key_value_pair_t> init, bee::Allocator* allocator) noexcept
    : HashMap(sign_cast<i32>(init.size()), allocator)
{
    for (auto& kv : init)
    {
        insert(kv);
    }
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::HashMap(HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>&& other) noexcept
    : hasher_(std::move(other.hasher_)),
      key_comparer_(std::move(other.key_comparer_)),
      node_storage_(std::move(other.node_storage_)),
      hash_shift_(other.hash_shift_),
      load_factor_(other.load_factor_),
      active_node_count_(other.active_node_count_)
{
    other.hash_shift_ = 32;
    other.load_factor_ = 0;
    other.active_node_count_ = 0;
}

/*
 *******************************
 *
 * operators - implementation
 *
 *******************************
 */

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>&
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::operator=(HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>&& other) noexcept
{
    hasher_ = std::move(other.hasher_);
    key_comparer_ = std::move(other.key_comparer_);
    node_storage_ = std::move(other.node_storage_);
    hash_shift_ = other.hash_shift_;
    load_factor_ = other.load_factor_;
    active_node_count_ = other.active_node_count_;

    other.hash_shift_ = 32;
    other.load_factor_ = 0;
    other.active_node_count_ = 0;

    return *this;
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
ValueType& HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::operator[](const KeyType& key) noexcept
{
    auto keyval = find(key);
    if (keyval != nullptr)
    {
        return keyval->value;
    }

    keyval = insert_no_construct(key);
    return keyval->value;
}


/*
 ******************************
 *
 * `insert` - implementations
 *
 ******************************
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
KeyValuePair<KeyType, ValueType>*
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::insert_no_construct(const KeyType& key)
{
    /*
     * Insertion is the only operation checked for container mode with hash maps as the client can rehash
     * as needed explicitly - we just don't want any implicit allocations happening
     */
    if (node_storage_.size() == 0)
    {
        if (!implicit_grow(container_mode_constant<Mode>{}))
        {
            return nullptr;
        }
    }

    auto next_slot_idx = find_key_index(key);

    if (BEE_FAIL_F(next_slot_idx < sign_cast<u32>(node_storage_.size()), "HashMap: unable to find a free slot for insertion"))
    {
        // must be a fixed-capacity HashMap with all nodes active
        return nullptr;
    }

    if (BEE_FAIL_F(!node_storage_[next_slot_idx].active, "HashMap: element with a duplicate key already exists"))
    {
        return nullptr;
    }

    if (active_node_count_ >= load_factor_)
    {
        // Need to grow the storage to insert the new value
        if (!implicit_grow(container_mode_constant<Mode>{}))
        {
            return nullptr;
        }
        next_slot_idx = find_key_index(key);
    }

    ++active_node_count_;

    auto node = &node_storage_[next_slot_idx];
    node->active = true;
    node->kv.key = key;

    return &node->kv;
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
KeyValuePair<KeyType, ValueType>*
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::insert(const KeyValuePair<KeyType, ValueType>& kv_pair)
{
    auto keyval = insert_no_construct(kv_pair.key);
    new (&keyval->value) ValueType(kv_pair.value);
    return keyval;
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
KeyValuePair<KeyType, ValueType>*
HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::insert(const KeyType& key, const ValueType& value)
{
    auto keyval = insert_no_construct(key);
    new (&keyval->value) ValueType(value);
    return keyval;
}

/*
 ****************************
 *
 * `find` - implementations
 *
 ****************************
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
KeyValuePair<KeyType, ValueType>* HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::find(const KeyType& key)
{
    return const_cast<key_value_pair_t*>(find_internal(key));
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
const KeyValuePair<KeyType, ValueType>* HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::find(const KeyType& key) const
{
    return find_internal(key);
}


/*
 **********************************************
 *
 * `find` - heterogeneous key implementations
 *
 **********************************************
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
template <typename EquivalentKey>
KeyValuePair<KeyType, ValueType>* HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::find(const EquivalentKey& key)
{
    return const_cast<key_value_pair_t*>(find_internal(key));
}

// const implementation
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
template <typename EquivalentKey>
const KeyValuePair<KeyType, ValueType>* HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::find(const EquivalentKey& key) const
{
    return find_internal(key);
}

/*
 ****************************
 *
 * `erase` - implementation
 *
 ****************************
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
template <typename EquivalentKey>
bool HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::erase_internal(const EquivalentKey& key)
{
    auto original_idx = find_key_index(key);
    auto node = &node_storage_[original_idx];
    if (!node->active)
    {
        return false;
    }

    auto cur_idx = original_idx;
    const auto max_idx = sign_cast<u32>(node_storage_.size()); // unsigned conversion: capacity is never negative
    while (true)
    {
        ++cur_idx;
        if (cur_idx >= max_idx)
        {
            cur_idx = 0;
        }

        node = &node_storage_[cur_idx];
        if (!node->active)
        {
            break;
        }

        // The position in the table the node would be at if no collisions had happened
        const auto natural_idx = hash_key(node->kv.key, hash_shift_, max_idx);

        // check if there's any vacant buckets between the natural and current indices
        // and reposition if this is the case (i.e. the positioning is invalid wrt clustering)
        if ((cur_idx > original_idx && (natural_idx <= original_idx || natural_idx > cur_idx))
            || (cur_idx < original_idx && (natural_idx <= original_idx && natural_idx > cur_idx)))
        {
            node_storage_[original_idx] = *node;
            original_idx = cur_idx;
        }
    }

    BEE_ASSERT_F(
        active_node_count_ > 0,
        "HashMap<T>: too many nodes were erased. This shouldn't happen"
    );

    --active_node_count_;
    destroy_node(&node_storage_[original_idx]);
    return true;
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
bool HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::erase(const KeyType& key)
{
    return erase_internal(key);
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
template <typename EquivalentKey>
bool HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::erase(const EquivalentKey& key)
{
    return erase_internal(key);
}

/*
 ****************************
 *
 * `clear` - implementation
 *
 ****************************
 */

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
void HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::clear()
{
    if (node_storage_.size() == 0)
    {
        return;
    }

    auto active_nodes_to_destroy = active_node_count_;
    Node* cur_node = nullptr;
    for (int cur_idx = 0; cur_idx < node_storage_.size(); ++cur_idx)
    {
        cur_node = &node_storage_[cur_idx];
        if (!cur_node->active)
        {
            continue;
        }

        destroy_node(cur_node);
        --active_nodes_to_destroy;
        if (active_nodes_to_destroy == 0)
        {
            break;
        }
    }

    node_storage_.clear();
    active_node_count_ = 0;
    hash_shift_ = 32;
    load_factor_ = 0;
}


/*
 ****************************
 *
 * `rehash` - implementations
 *
 ****************************
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
void HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::rehash(const i32 new_count)

{
    BEE_ASSERT_F(new_count >= 0, "HashMap: bucket count cannot be negative");

    if (BEE_FAIL_F(math::is_power_of_two(new_count) || new_count == 0, "HashMap: new capacity must be a power of 2 or zero"))
    {
        return;
    }

    const auto u32_new_count = sign_cast<u32>(new_count);

    // unsigned conversion: capacity is never negative
    if (new_count <= node_storage_.size() || u32_new_count < active_node_count_)
    {
        return;
    }

    /*
     * NOTE: Rehashing is valid for fixed-capacity hash maps as we want to allow the client to have the
     * ability to explicitly change this when needed - we still assert on insertion though
     */
    const auto new_hash_shift = 32u - math::log2i(u32_new_count);

    auto new_buckets = storage_t::with_size(new_count, Node{false, {}});
    u32 new_active_node_count = 0;

    for (int cur_node_idx = 0; cur_node_idx < node_storage_.size(); ++cur_node_idx)
    {
        auto old_node = &node_storage_[cur_node_idx];
        const auto hash_idx = hash_key(old_node->kv.key, new_hash_shift, u32_new_count);
        auto new_hash_idx = hash_idx;
        Node* new_node = nullptr;

        while (true)
        {
            new_node = new_buckets.data() + new_hash_idx;
            if (!new_node->active)
            {
                break;
            }
            ++new_hash_idx;
            if (new_hash_idx >= u32_new_count)
            {
                new_hash_idx = 0;
            }
            BEE_ASSERT_F(new_hash_idx != hash_idx, "Invalid HashMap state");
        }

        // Copy by value rather than memcpy in case T has virtuals
        *new_node = *old_node;

        if (new_node->active)
        {
            ++new_active_node_count;
        }
        if (new_active_node_count == active_node_count_)
        {
            break;
        }
    }

    hash_shift_ = new_hash_shift;
    BEE_ASSERT_F(u32_new_count == (1u << (32u - new_hash_shift)), "Invalid hash shift");
    load_factor_ = (u32_new_count + 1u) >> 1u;
    active_node_count_ = new_active_node_count;
    node_storage_.move_replace_no_destruct(std::move(new_buckets));
}


/*
 ************************************************************************************************************************************************
 *
 * Key hashing function:
 *
 * Uses fibonacci hashing to get the index which serves two purposes:
 * - it's faster that integer modulo
 * - it has the nice property of providing a better mapping from a large
 *   possible set of values (all possible keys ever) to a small set of values (keys we store)
 * see here for reference:
 * https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that-the-world-forgot-or-a-better-alternative-to-integer-modulo/
 *
 *************************************************************************************************************************************************
*/
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
template <typename EquivalentKey>
u32 HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::hash_key(
    const EquivalentKey& key,
    const u32 hash_shift,
    const u32 capacity
) const
{
    BEE_ASSERT_F((1u << (32u - hash_shift)) == capacity, "HashMap: Invalid hash shift");

    const auto hash = (2654435769u * hasher_(key)) >> hash_shift;

    BEE_ASSERT_F(hash < capacity, "HashMap: Invalid hash shift");

    return hash;
}


/*
 ******************************
 *
 * HashMap internal functions
 *
 ******************************
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
template <typename EquivalentKey>
u32 HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::find_key_index(const EquivalentKey& key) const
{
    const auto original_idx = hash_key(key, hash_shift_, sign_cast<u32>(node_storage_.size()));

    auto node = &node_storage_[original_idx];
    auto cur_idx = original_idx;
    i32 iterations = 0;

    for (; iterations < node_storage_.size(); ++iterations)
    {
        if (!node->active || key_comparer_(node->kv.key, key))
        {
            return cur_idx;
        }

        ++cur_idx;
        // unsigned call: capacity is never negative
        if (cur_idx >= sign_cast<u32>(node_storage_.size()))
        {
            cur_idx = 0;
        }
        node = &node_storage_[cur_idx];
    }

    return sign_cast<u32>(iterations);
}

template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
template <typename EquivalentKey>
const KeyValuePair<KeyType, ValueType>* HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::find_internal(const EquivalentKey& key) const
{
    if (node_storage_.size() == 0 || active_node_count_ == 0)
    {
        return nullptr;
    }

    auto key_idx = find_key_index(key);
    auto node = &node_storage_[key_idx];

    if (!node->active)
    {
        return nullptr;
    }

    return &node->kv;
}

/*
 * Capacity validation implementation for fixed-capacity hashmap
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
constexpr bool HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::implicit_grow(
    const fixed_container_mode_t& fixed_capacity
)
{
    return BEE_CHECK_F(active_node_count_ <= sign_cast<u32>(node_storage_.size()), "FixedHashMap: new capacity exceeded the fixed capacity of the HashMap");
}

/*
 * Capacity validation implementation for fixed-capacity hashmap - always returns true
 */
template <typename KeyType, typename ValueType, ContainerMode Mode, typename Hasher, typename KeyEqual>
constexpr bool HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>::implicit_grow(
    const dynamic_container_mode_t& dynamic_capacity
)
{
    rehash(next_growth_capacity());
    return true;
}

} // namespace bee
