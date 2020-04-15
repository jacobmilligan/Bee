/*
 *  Component.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Jobs/JobTypes.hpp"
#include "Bee/Core/Containers/HashMap.hpp"


namespace bee {


BEE_RAW_HANDLE_U32(ArchetypeHandle);

struct Type;
struct Archetype;


struct ComponentChunk
{
    ComponentChunk* next { nullptr };
    ComponentChunk* previous { nullptr };

    size_t          allocated_size { 0 };
    size_t          bytes_per_entity { 0 };
    i32             capacity { 0 };
    i32             count { 0 };
    Archetype*      archetype { nullptr };
    u8*             data { nullptr };
};


struct Archetype
{
    u32             hash { 0 };
    size_t          chunk_size { 0 };
    size_t          entity_size { 0 };
    i32             type_count { 0 };
    const Type**    types { nullptr };
    size_t*         offsets { nullptr };
    i32             chunk_count { 0 };
    // TODO(Jacob): measure and see if this would be better as an array vs a linked list
    //  (would require realloc of the archetype to be viable so might be slower)
    ComponentChunk* first_chunk { nullptr };
    ComponentChunk* last_chunk { nullptr };
};


class ChunkAllocator final : public Allocator
{
public:
    BEE_ALLOCATOR_DO_NOT_TRACK

    ChunkAllocator() = default;

    ChunkAllocator(const size_t chunk_size, const size_t chunk_alignment);

    ~ChunkAllocator() override;

    bool is_valid(const void* ptr) const override;

    void* allocate(size_t size, size_t alignment) override;

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override;

    void deallocate(void* ptr) override;

    inline size_t max_allocation_size() const
    {
        return chunk_size_ - sizeof(AllocHeader) - sizeof(ChunkHeader);
    }
private:
    static constexpr u32 header_signature = 0x23464829;

    struct ChunkHeader
    {
        ChunkHeader*    next { nullptr };
        u32             signature { header_signature };
        u8*             data { nullptr };
        size_t          size { 0 };
    };

    struct AllocHeader
    {
        ChunkHeader*    chunk { nullptr };
        size_t          size { 0 };
    };

    size_t          chunk_size_ { 0 };
    size_t          chunk_alignment_ { 0 };
    ChunkHeader*    first_ { nullptr };
    ChunkHeader*    last_ { nullptr };
    ChunkHeader*    free_ { nullptr };

    static AllocHeader* get_alloc_header(void* ptr);

    static const AllocHeader* get_alloc_header(const void* ptr);
};

enum class EntityComponentAccess
{
    read_only,
    read_write
};

struct EntityComponentDependencyMap
{
    struct DependencyInfo
    {
        JobGroup ro_deps;
        JobGroup rw_deps;
    };

    ReaderWriterMutex                       mutex;
    ChunkAllocator                          allocator;
    DynamicHashMap<u32, DependencyInfo*>    type_dependencies;
    DependencyInfo                          all_dependencies_;

    EntityComponentDependencyMap();

    void add_type_if_not_registered(const Type* type);

    void add_dependencies(const EntityComponentAccess access, JobGroup* group, const Span<const Type*>& read_types, const Span<const Type*>& write_types);
};


} // namespace bee