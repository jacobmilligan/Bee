/*
 *  TestComponents.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection-Deprecated.hpp"
#include "Bee/Core/Math/float4.hpp"
#include "Bee/Entity/Entity.hpp"

namespace bee {


struct BEE_REFLECT() Position
{
    float3 value;
};

struct BEE_REFLECT() Rotation
{
    float3 value;
};

struct BEE_REFLECT() Scale
{
    float3 value;
};

struct BEE_REFLECT() TestSystem final : public EntitySystem
{
    EntityComponentQuery    query;
    i32                     processed_entities { 0 };

    void init() override
    {
        query = get_or_create_query(read<Position>(), read_write<Scale>(), read_write<Rotation>());
    }

    struct TestJob final : public EntitySystemJob<TestJob>
    {
        i32* counter { nullptr };

        explicit TestJob(i32* entity_counter)
            : counter(entity_counter)
        {
            *counter = 0;
        }

        void for_each(const i32 count, const Entity* entities, const Position* positions, Scale* scales, Rotation* rotations)
        {
            for (int e = 0; e < count; ++e)
            {
                scales[e].value *= positions[e].value;
                rotations[e].value += positions[e].value;
            }

            *counter += count;
        }
    }; 

//    static void test_job_chunks(ComponentChunk* chunk)
//    {
//        auto entities = get_component_array<Entity>(chunk);
//        auto positions = get_component_array<const Position>(chunk);
//        auto scales = get_component_array<Scale>(chunk);
//        auto rotations = get_component_array<Rotation>(chunk);
//        test_job(chunk->count, entities, positions, scales, rotations);
//    }

    void execute() override
    {
        //        execute_jobs<TestJob>(query, &group);

        for_each_entity(query, [&](const Entity& entity, const Position& position, Scale& scale, Rotation& rotation)
        {
            scale.value *= position.value;
            rotation.value += position.value;
        });

        JobGroup group;
        execute_jobs<TestJob>(query, &group, &processed_entities);
        job_wait(&group);
    }
};


} // namespace bee