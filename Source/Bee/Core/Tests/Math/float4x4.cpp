/*
 *  float4x4.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Math/float4x4.hpp>

#include <gtest/gtest.h>

void compare_float4x4(const bee::float4x4& a, const bee::float4x4& b)
{
    for (int e = 0; e < bee::float4x4::num_elements; ++e) {
        ASSERT_FLOAT_EQ(a[e], b[e]) << "Index: " << e << " test: "
                                    << ::testing::UnitTest::GetInstance()->current_test_info()->name();
    }
}

TEST(float4x4Tests, matrix_vector_multiplication)
{
    bee::float4x4 mat(1.0f, 2.0f, 3.0f, 4.0f,
                      5.0f, 6.0f, 7.0f, 8.0f,
                      9.0f, 10.0f, 11.0f, 12.0f,
                      13.0f, 14.0f, 15.0f, 16.0f);
    bee::float4 vec(2.0f, 4.0f, 6.0f, 8.0f);
    auto result = mat * vec;
    ASSERT_FLOAT_EQ(result.x, 180.0f);
    ASSERT_FLOAT_EQ(result.y, 200.0f);
    ASSERT_FLOAT_EQ(result.z, 220.0f);
    ASSERT_FLOAT_EQ(result.w, 240.0f);
}

TEST(float4x4Tests, matrix_matrix_multiplication)
{
    bee::float4x4 mat1(1.0f, 2.0f, 3.0f, 4.0f,
                       5.0f, 6.0f, 7.0f, 8.0f,
                       9.0f, 10.0f, 11.0f, 12.0f,
                       13.0f, 14.0f, 15.0f, 16.0f);
    bee::float4x4 mat2(1.0f, 2.0f, 3.0f, 4.0f,
                       5.0f, 6.0f, 7.0f, 8.0f,
                       9.0f, 10.0f, 11.0f, 12.0f,
                       13.0f, 14.0f, 15.0f, 16.0f);
    bee::float4x4 expected(90.0f, 100.0f, 110.0f, 120.0f,
                           202.0f, 228.0f, 254.0f, 280.0f,
                           314.0f, 356.0f, 398.0f, 440.0f,
                           426.0f, 484.0f, 542.0f, 600.0f);
    auto result = mat1 * mat2;
    compare_float4x4(result, expected);
}

TEST(float4x4Tests, translation_is_correct)
{
    auto translation = bee::translate({2.0f, 34.0f, 23.5f});
    bee::float4x4 expected(1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 0.0f,
                           2.0f, 34.0f, 23.5f, 1.0f);
    compare_float4x4(translation, expected);
    auto mul = translation * bee::float4(1.0f, 2.0f, 3.0f, 1.0f);
    ASSERT_FLOAT_EQ(mul.x, 3.0f);
    ASSERT_FLOAT_EQ(mul.y, 36.0f);
    ASSERT_FLOAT_EQ(mul.z, 26.5f);
    ASSERT_FLOAT_EQ(mul.w, 1.0f);
}

TEST(float4x4Tests, scale_is_correct)
{
    auto scale = bee::scale({12.0f, 9.2f, 3.1f});
    bee::float4x4 expected(12.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 9.2f, 0.0f, 0.0f,
                           0.0f, 0.0f, 3.1f, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f);
    compare_float4x4(scale, expected);
    auto mul = scale * bee::float4(6.0f, 3.0f, 21.0f, 1.0f);
    ASSERT_FLOAT_EQ(mul.x, 72.0f);
    ASSERT_FLOAT_EQ(mul.y, 27.6f);
    ASSERT_FLOAT_EQ(mul.z, 65.1f);
    ASSERT_FLOAT_EQ(mul.w, 1.0f);
}

TEST(float4x4Tests, rotation_is_correct)
{
    auto rotation = bee::rotate(bee::math::deg_to_rad(10.0f), {2.0f, 5.32f, 1.1f});
    ASSERT_FLOAT_EQ(rotation.m00, 0.986621081f);
    ASSERT_FLOAT_EQ(rotation.m01, 0.0378193744f);
    ASSERT_FLOAT_EQ(rotation.m02, -0.158582896f);
    ASSERT_FLOAT_EQ(rotation.m03, 0.0f);
    ASSERT_FLOAT_EQ(rotation.m10, -0.028172452f);
    ASSERT_FLOAT_EQ(rotation.m11, 0.997638106f);
    ASSERT_FLOAT_EQ(rotation.m12, 0.0626454726f);
    ASSERT_FLOAT_EQ(rotation.m13, 0.0f);
    ASSERT_FLOAT_EQ(rotation.m20, 0.160577565f);
    ASSERT_FLOAT_EQ(rotation.m21, -0.0573396645f);
    ASSERT_FLOAT_EQ(rotation.m22, 0.985356271f);
    ASSERT_FLOAT_EQ(rotation.m23, 0.0f);
    ASSERT_FLOAT_EQ(rotation.m30, 0.0f);
    ASSERT_FLOAT_EQ(rotation.m31, 0.0f);
    ASSERT_FLOAT_EQ(rotation.m32, 0.0f);
    ASSERT_FLOAT_EQ(rotation.m33, 1.0f);

    auto mul = rotation * bee::float4(8.0f, 4.0f, 2.0f, 1.0f);
    ASSERT_FLOAT_EQ(mul.x, 8.10143375f);
    ASSERT_FLOAT_EQ(mul.y, 4.17842817f);
    ASSERT_FLOAT_EQ(mul.z, 0.952631235f);
    ASSERT_FLOAT_EQ(mul.w, 1.0f);
}

TEST(float4x4Tests, look_at_is_correct)
{
    bee::float3 eye(1.0f, 5.5f, 2.0f);
    bee::float3 target(100.0f, 2.0f, 12.0f);
    bee::float3 up(0.0f, 1.0f, 0.0f);
    auto look_at = bee::look_at(eye, target, up);

    ASSERT_FLOAT_EQ(look_at.m00, 0.100498706f);
    ASSERT_FLOAT_EQ(look_at.m01, 0.0349748358f);
    ASSERT_FLOAT_EQ(look_at.m02, 0.99432224f);
    ASSERT_FLOAT_EQ(look_at.m03, 0.0f);
    ASSERT_FLOAT_EQ(look_at.m10, 0.0f);
    ASSERT_FLOAT_EQ(look_at.m11, 0.999381899f);
    ASSERT_FLOAT_EQ(look_at.m12, -0.0351528078f);
    ASSERT_FLOAT_EQ(look_at.m13, 0.0f);
    ASSERT_FLOAT_EQ(look_at.m20, -0.994937181f);
    ASSERT_FLOAT_EQ(look_at.m21, 0.00353281177f);
    ASSERT_FLOAT_EQ(look_at.m22, 0.100436591f);
    ASSERT_FLOAT_EQ(look_at.m23, 0.0f);
    ASSERT_FLOAT_EQ(look_at.m30, 1.88937569f);
    ASSERT_FLOAT_EQ(look_at.m31, -5.53864145f);
    ASSERT_FLOAT_EQ(look_at.m32, -1.0018549f);
    ASSERT_FLOAT_EQ(look_at.m33, 1.0f);
}

TEST(float4x4Tests, perspective_projection_is_correct)
{
    const auto fov = bee::math::deg_to_rad(90.0f);
    const auto proj = bee::perspective(fov, 2.0f, 5.0f, 15.0f);

    const bee::float4x4 expected_proj(0.5f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 1.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 1.5f, 1.0f,
                                      0.0f, 0.0f, -7.5f, 0.0f);

    compare_float4x4(proj, expected_proj);

    auto vec = bee::float4(5.0f, 5.0f, -15.0f, 1.0f);
    auto result = proj * vec;
    ASSERT_FLOAT_EQ(result.x, 2.5f);
    ASSERT_FLOAT_EQ(result.y, 5.0f);
    ASSERT_FLOAT_EQ(result.z, -30.0f);
    ASSERT_FLOAT_EQ(result.w, -15.0f);
}

TEST(float4x4Tests, ortho_is_correct)
{
    const auto ortho_result = bee::ortho(0.23f, 23.0f, 0.0f, 12.0f, -9.0f, 12.0f);
    const bee::float4x4 expected(0.0878348723f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 0.166666672f, 0.0f, 0.0f,
                                 0.0f, 0.0f, -0.095238097f, 0.0f,
                                 -1.02020204f, -1.0f, -0.142857149f, 1.0f);
    compare_float4x4(ortho_result, expected);
}