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

TEST(EntityTests, create_world)
{
    WorldDescriptor desc{};
    World world(desc);
    auto entity = world.create_entity();
    ASSERT_TRUE(entity.is_valid());
    auto component = world.add_component<Position>(entity);
    ASSERT_FLOAT_EQ(component->x, 0.0f);
}