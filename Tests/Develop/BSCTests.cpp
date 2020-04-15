/*
 *  BSCTests.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include <Bee/ShaderCompiler/Parse.hpp>
#include <Bee/Core/Filesystem.hpp>

#include <gtest/gtest.h>

using namespace bee;

TEST(BscTests, test_ast)
{
    const auto path = fs::get_appdata().assets_root.join("Shaders/BscTestShader.bsc");
    ASSERT_TRUE(path.exists());

    auto file = fs::read(path);

    ASSERT_FALSE(file.empty());

    auto hlsl_index = str::first_index_of(file, "code:");
    auto hlsl_begin = file.c_str() + hlsl_index;
    auto bracket_index = str::first_index_of(hlsl_begin, '{');
    hlsl_begin = hlsl_begin + bracket_index + 1;
    auto hlsl_end = hlsl_begin;
    int scope_count = 0;

    while (scope_count >= 0)
    {
        if (hlsl_end == file.end())
        {
            break;
        }

        ++hlsl_end;

        if (*hlsl_end == '{')
        {
            ++scope_count;
            continue;
        }

        if (*hlsl_end == '}')
        {
            --scope_count;
            continue;
        }
    }

    StringView code(hlsl_begin, hlsl_end);

    BscParser parser;
    BscModule module;
    const auto success = parser.parse(file.view(), &module);
    ASSERT_TRUE(success);
    ASSERT_EQ(module.render_passes.size(), 1);
    ASSERT_EQ(module.pipeline_states.size(), 1);
    ASSERT_EQ(module.raster_states.size(), 1);
    ASSERT_EQ(module.depth_stencil_states.size(), 1);
    ASSERT_EQ(module.multisample_states.size(), 1);
    ASSERT_EQ(module.shaders.size(), 1);

    auto& render_pass = module.render_passes[0];
    ASSERT_EQ(render_pass.identifier, "DefaultPass");
    ASSERT_EQ(render_pass.data.attachments.size(), 1);
    ASSERT_EQ(render_pass.data.subpasses.size(), 1);

    auto& attachment = module.render_passes[0].data.attachments[0];
    auto& subpass = module.render_passes[0].data.subpasses[0];

    // Attachment MainColor
    ASSERT_EQ(attachment.identifier, "MainColor");
    ASSERT_EQ(attachment.data.type, AttachmentType::color);
    ASSERT_EQ(attachment.data.format, PixelFormat::rgba16i);
    ASSERT_EQ(attachment.data.load_op, LoadOp::clear);
    ASSERT_EQ(attachment.data.store_op, StoreOp::store);
    ASSERT_EQ(attachment.data.samples, 4u);

    // SubPass DefaultSubPass
    ASSERT_EQ(subpass.identifier, "DefaultSubPass");
    ASSERT_EQ(subpass.data.input_attachments.size(), 0);
    ASSERT_EQ(subpass.data.color_attachments.size(), 1);
    ASSERT_EQ(subpass.data.resolve_attachments.size(), 0);
    ASSERT_EQ(subpass.data.preserve_attachments.size(), 0);
    ASSERT_EQ(subpass.data.color_attachments[0], "MainColor");

    // RasterState DefaultRasterState
    auto& rso = module.raster_states[0];
    ASSERT_EQ(rso.identifier, "DefaultRasterState");
    ASSERT_EQ(rso.data.front_face_ccw, true);
    ASSERT_EQ(rso.data.cull_mode, CullMode::back);

    // DepthStencilState DefaultDSS
    auto& dsso = module.depth_stencil_states[0];
    ASSERT_EQ(dsso.identifier, "DefaultDSS");
    ASSERT_EQ(dsso.data.depth_test_enabled, true);
    ASSERT_EQ(dsso.data.front_face_stencil.fail_op, StencilOp::zero);
    ASSERT_EQ(dsso.data.front_face_stencil.pass_op, StencilOp::replace);
    ASSERT_EQ(dsso.data.front_face_stencil.read_mask, 2u);

    // MultisampleState DefaultMSS
    auto& msso = module.multisample_states[0];
    ASSERT_EQ(msso.identifier, "DefaultMSS");
    ASSERT_EQ(msso.data.sample_count, 2u);
    ASSERT_EQ(msso.data.sample_shading_enabled, true);
    ASSERT_EQ(msso.data.sample_shading, 1.0f);
    ASSERT_EQ(msso.data.sample_mask, 2u);
    ASSERT_EQ(msso.data.alpha_to_one_enabled, true);
    ASSERT_EQ(msso.data.alpha_to_coverage_enabled, true);

    auto& shader = module.shaders[0];
    ASSERT_EQ(shader.identifier, "TriangleShader");
    ASSERT_EQ(shader.data.stages[0], "vert");
    ASSERT_EQ(shader.data.stages[1], "frag");
    ASSERT_EQ(shader.data.code, code);

    // PipelineState DefaultPipelineState
    auto& pso = module.pipeline_states[0];
    ASSERT_EQ(pso.identifier, "DefaultPipelineState");
    ASSERT_EQ(pso.data.primitive_type, PrimitiveType::triangle);
    ASSERT_EQ(pso.data.render_pass, render_pass.identifier);
    ASSERT_EQ(pso.data.subpass, subpass.identifier);
    ASSERT_EQ(pso.data.raster_state, rso.identifier);
    ASSERT_EQ(pso.data.depth_stencil_state, dsso.identifier);
    ASSERT_EQ(pso.data.multisample_state, msso.identifier);
    ASSERT_EQ(pso.data.vertex_stage, shader.identifier);
    ASSERT_EQ(pso.data.fragment_stage, shader.identifier);

    Shader asset;
    const auto result = bsc_resolve_module(module, &asset);
    ASSERT_TRUE(result) << result.to_string().c_str();

    ASSERT_EQ(asset.subshaders().size(), 1);
    ASSERT_EQ(asset.pipelines().size(), 1);
    ASSERT_EQ(asset.passes().size(), 1);
}