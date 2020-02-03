/*
 *  EntityTests.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "TestComponents.hpp"

#include <gtest/gtest.h>

using namespace bee;

class EntityTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        bee::JobSystemInitInfo info{};
        bee::job_system_init(info);
    }

    void TearDown() override
    {
        bee::job_system_shutdown();
    }
};

TEST_F(EntityTests, basic_entity_test)
{
    WorldDescriptor desc{};
    World world(desc);
    auto entity = world.create_entity();
    ASSERT_TRUE(entity.is_valid());
    auto component = world.add_component<Position>(entity, float3{ 1.0f, 1.0f, 1.0f });
    ASSERT_FLOAT_EQ(component->value.x, 1.0f);

    world.destroy_entity(entity);

    ASSERT_FALSE(math::approximately_equal(component->value.x, 1.0f));
    const auto archetype = world.get_archetype<Position>();
    const Type* test_types[] = { get_type<Entity>(), get_type<Position>() };
    ASSERT_TRUE(archetype.is_valid());
    ASSERT_EQ(archetype.id, get_archetype_hash(test_types, static_array_length(test_types)));
}

TEST_F(EntityTests, bulk_create_destroy_entities)
{
    WorldDescriptor desc{};
    World world(desc);

    auto archetype = world.create_archetype<Position, Rotation, Scale>();

    ASSERT_TRUE(archetype.is_valid());

    auto entities = FixedArray<Entity>::with_size(1u << 16u);
    world.create_entities(archetype, entities.data(), entities.size());

    ASSERT_EQ(world.alive_count(), static_cast<i64>(1u << 16u));

    for (const auto& entity : entities)
    {
        auto position = world.get_component<Position>(entity);
        auto rotation = world.get_component<Rotation>(entity);
        auto scale = world.get_component<Scale>(entity);

        position->value = float3(3.14f);
        rotation->value = float3(3.14f);
        scale->value = float3(3.14f);
    }

    for (const auto& entity : entities)
    {
        auto position = world.get_component<Position>(entity);
        auto rotation = world.get_component<Rotation>(entity);
        auto scale = world.get_component<Scale>(entity);

        ASSERT_EQ(position->value, float3(3.14f));
        ASSERT_EQ(rotation->value, float3(3.14f));
        ASSERT_EQ(scale->value, float3(3.14f));
    }

    world.destroy_entities(entities.data(), entities.size());
    ASSERT_EQ(world.alive_count(), 0);
}

TEST_F(EntityTests, system_iteration)
{
    WorldDescriptor desc{};
    World world(desc);
    world.add_system<TestSystem>();
    auto archetype = world.create_archetype<Position, Rotation, Scale>();

    auto entities = FixedArray<Entity>::with_size(1u << 16u);
    world.create_entities(archetype, entities.data(), entities.size());

    for (const auto entity : enumerate(entities))
    {
        ASSERT_TRUE(entity.value.is_valid());
        auto pos = world.get_component<Position>(entity.value);
        auto scale = world.get_component<Scale>(entity.value);
        pos->value = float3(static_cast<float>(entity.index));
        scale->value = pos->value;
    }

    auto system = world.get_system<TestSystem>();
    system->execute();

    ASSERT_EQ(system->processed_entities, entities.size());

    for (const auto entity : enumerate(entities))
    {
        auto pos = world.get_component<Position>(entity.value);
        auto rot = world.get_component<Rotation>(entity.value);
        auto scale = world.get_component<Scale>(entity.value);

        ASSERT_EQ(scale->value, pos->value * pos->value * pos->value);
        ASSERT_EQ(rot->value, pos->value * 2.0f);
    }
}
