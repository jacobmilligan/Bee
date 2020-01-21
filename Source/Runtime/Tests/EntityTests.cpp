/*
 *  EntityTests.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "TestComponents.hpp"

#include <Bee/Entity/Entity.hpp>

#include <gtest/gtest.h>

using namespace bee;

TEST(EntityTests, basic_entity_test)
{
    WorldDescriptor desc{};
    World world(desc);
    auto entity = world.create_entity();
    ASSERT_TRUE(entity.is_valid());
    auto component = world.add_component<Position>(entity, 1.0f, 1.0f, 1.0f);
    ASSERT_FLOAT_EQ(component->x, 1.0f);

    world.destroy_entity(entity);

    ASSERT_FALSE(math::approximately_equal(component->x, 1.0f));

    const auto archetype = world.get_archetype<Position>();
    const Type* test_types[] = { get_type<Entity>(), get_type<Position>() };
    ASSERT_TRUE(archetype.is_valid());
    ASSERT_EQ(archetype.id, get_archetype_hash(test_types, static_array_length(test_types)));
}

TEST(EntityTests, bulk_create_destroy_entities)
{
    WorldDescriptor desc{};
    World world(desc);
    auto entities = FixedArray<Entity>::with_size(1u << 16u);
    world.create_entities(entities.data(), entities.size());
    ASSERT_EQ(world.alive_count(), static_cast<i64>(1u << 16u));
    world.destroy_entities(entities.data(), entities.size());
    ASSERT_EQ(world.alive_count(), 0);
    ASSERT_EQ(world.archetype_count(), 0);
}