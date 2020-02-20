/*
 *  RenderGraphTests.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include <Bee/Graphics/RenderGraph.hpp>

#include <gtest/gtest.h>

TEST(RenderGraphTests, dynamic_pass_simple_test)
{
    bee::RenderGraph graph;
    auto builder = graph.add_pass("test builder");

    bee::TextureCreateInfo texture_info{};
    texture_info.type = bee::TextureType::tex2d;
    texture_info.width = 100;
    texture_info.height = 100;
    texture_info.format = bee::PixelFormat::bgra8;
    texture_info.sample_count = 1;
    const auto target = builder.create_texture("target", texture_info);
    builder.write_color(target, bee::LoadOp::clear, bee::StoreOp::store);

    builder.set_execute_function([&](bee::RenderGraphExecuteContext* ctx)
    {
        const auto texture = ctx->get_texture(target);
//        cmd.draw
    });
}