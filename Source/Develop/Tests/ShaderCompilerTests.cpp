/*
 *  ShaderCompilerTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/ShaderCompiler/ShaderCompiler.hpp>
#include <Bee/Graphics/Shader.hpp>
#include <Bee/Core/Filesystem.hpp>

#include <gtest/gtest.h>

TEST(ShaderCompilerTests, test_triangle_shader)
{
    bee::ShaderCompiler compiler;
    const auto location = bee::fs::get_appdata().assets_root.join("Shaders").join("Triangle.bsc");
    const auto new_location = bee::fs::get_appdata().data_root.join("test-shader");
    bee::DynamicArray<bee::u8> data;
    bee::io::MemoryStream stream(&data);
    bee::AssetCompileSettings settings;
    bee::AssetCompileContext ctx(bee::AssetPlatform::vulkan, &location, &settings);
    ctx.temp_allocator = bee::temp_allocator();
    ctx.stream = &stream;
    auto result = compiler.compile(&ctx);

    ASSERT_EQ(result.status, bee::AssetCompilerStatus::success);
    ASSERT_EQ(result.compiled_type, bee::Type::from_static<bee::Shader>());

    stream.seek(0, bee::io::SeekOrigin::begin);
    bee::StreamSerializer serializer(&stream);
    bee::BSCModule module;
    bee::serialize(bee::SerializerMode::reading, &serializer, &module);

    ASSERT_STREQ(module.name, "Triangle");
    ASSERT_EQ(module.shader_count, 2);
}