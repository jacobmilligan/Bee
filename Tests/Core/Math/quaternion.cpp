/*
 *  quaternion.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Math/quaternion.hpp>

#include <GTest.hpp>

TEST(QuaternionTests, test_quat_multiplication)
{
    bee::quaternion quat_a(-0.69898f, 0.3235f, -0.9999f, 0.23456f);
    bee::quaternion quat_b(-0.90293f, 0.1000f, 0.2983f, 0.9f);
    const auto result = quat_a * quat_b;
    ASSERT_FLOAT_EQ(result.w, 0.685946167f);
    ASSERT_FLOAT_EQ(result.x, -1.33187509f);
    ASSERT_FLOAT_EQ(result.y, 0.426640004f);
    ASSERT_FLOAT_EQ(result.z, -0.644383192f);
}

TEST(QuaternionTests, test_quat_normalizing)
{
    bee::quaternion quat_a(1.0f, 2.0f, 3.0f, 4.0f);
    const auto result = normalize(quat_a);
    ASSERT_FLOAT_EQ(result.w, 0.182574185835055f);
    ASSERT_FLOAT_EQ(result.x, 0.365148371670111f);
    ASSERT_FLOAT_EQ(result.y, 0.547722557505166f);
    ASSERT_FLOAT_EQ(result.z, 0.730296743340221f);
}

TEST(QuaternionTests, test_matrix_to_quat)
{
    bee::float4x4 mat(0.2346987987f, 0.1239293f, 0.2398548956f, 0.982938f,
                      0.82873789f, 0.88928398f, 0.441293198f, 0.1230987f,
                      0.328293f, 0.123213123f, 0.999999f, 0.545987f,
                      0.559879f, 0.32123123f, 0.45098f, 0.454098098f);
    bee::quaternion quat(mat);
    ASSERT_FLOAT_EQ(quat.w, 0.970339119f);
    ASSERT_FLOAT_EQ(quat.x, -0.0987987742f);
    ASSERT_FLOAT_EQ(quat.y, -0.0274697319f);
    ASSERT_FLOAT_EQ(quat.z, 0.21892041f);
}

TEST(QuaternionTests, test_quat_to_matrix)
{
    bee::quaternion quat(0.23456f, -0.69898f, 0.3235f, -0.9999f);
    bee::float4x4 mat(quat);
    ASSERT_FLOAT_EQ(mat.m00, -1.2089045f);
    ASSERT_FLOAT_EQ(mat.m01, 0.0168330371f);
    ASSERT_FLOAT_EQ(mat.m02, 1.54958045f);
    ASSERT_FLOAT_EQ(mat.m03, 0.0f);

    ASSERT_FLOAT_EQ(mat.m10, -0.921313167f);
    ASSERT_FLOAT_EQ(mat.m11, -1.97674608f);
    ASSERT_FLOAT_EQ(mat.m12, -0.319029808f);
    ASSERT_FLOAT_EQ(mat.m13, 0.0f);

    ASSERT_FLOAT_EQ(mat.m20, 1.24605978f);
    ASSERT_FLOAT_EQ(mat.m21, -0.97484076f);
    ASSERT_FLOAT_EQ(mat.m22, -0.186450481f);
    ASSERT_FLOAT_EQ(mat.m23, 0.0f);

    ASSERT_FLOAT_EQ(mat.m30, 0.0f);
    ASSERT_FLOAT_EQ(mat.m31, 0.0f);
    ASSERT_FLOAT_EQ(mat.m32, 0.0f);
    ASSERT_FLOAT_EQ(mat.m33, 0.0f);
}

TEST(QuaternionTests, quat_normalize)
{
    bee::quaternion quat(-0.23f, 0.234f, -0.9987f, 0.22334f);
    const auto normalized = bee::normalize(quat);
    ASSERT_FLOAT_EQ(normalized.w, -0.214017063f);
    ASSERT_FLOAT_EQ(normalized.x, 0.21773909f);
    ASSERT_FLOAT_EQ(normalized.y, -0.929299355f);
    ASSERT_FLOAT_EQ(normalized.z, 0.207819879f);

    quat = { 0.0f, 0.0f, 0.0f, 0.0f };
    const auto normalized_unit = bee::normalize(quat);
    ASSERT_FLOAT_EQ(normalized_unit.w, 1.0f);
    ASSERT_FLOAT_EQ(normalized_unit.x, 0.0f);
    ASSERT_FLOAT_EQ(normalized_unit.y, 0.0f);
    ASSERT_FLOAT_EQ(normalized_unit.z, 0.0f);
}

TEST(QuaternionTests, quat_conjugate)
{
    bee::quaternion quat(-0.23f, 0.234f, -0.9987f, 0.22334f);
    const auto conjugate_result = bee::conjugate(quat);
    ASSERT_FLOAT_EQ(conjugate_result.w, -0.23f);
    ASSERT_FLOAT_EQ(conjugate_result.x, -0.234f);
    ASSERT_FLOAT_EQ(conjugate_result.y, 0.9987f);
    ASSERT_FLOAT_EQ(conjugate_result.z, -0.22334f);
}

TEST(QuaternionTests, slerp_is_correct)
{
    const auto t = 0.001f;
    bee::quaternion quat_a(0.23456f, -0.69898f, 0.3235f, -0.9999f);
    bee::quaternion quat_b(0.9f, -0.90293f, 0.1000f, 0.2983f);
    auto slerp_result = bee::slerp(quat_a, quat_b, t);
    ASSERT_FLOAT_EQ(slerp_result.w, 0.235455126f);
    ASSERT_FLOAT_EQ(slerp_result.x, -0.699565053f);
    ASSERT_FLOAT_EQ(slerp_result.y, 0.323398709f);
    ASSERT_FLOAT_EQ(slerp_result.z, -0.998875856f);
}

TEST(QuaternionTests, nlerp_is_correct)
{
    const auto t = 0.001f;
    bee::quaternion quat_a(0.23456f, -0.69898f, 0.3235f, -0.9999f);
    bee::quaternion quat_b(0.9f, -0.90293f, 0.1000f, 0.2983f);

    const auto nlerp_result = bee::nlerp(quat_a, quat_b, t);

    ASSERT_FLOAT_EQ(nlerp_result.w, 0.183350563f);
    ASSERT_FLOAT_EQ(nlerp_result.x, -0.544991076f);
    ASSERT_FLOAT_EQ(nlerp_result.y, 0.251983523f);
    ASSERT_FLOAT_EQ(nlerp_result.z, -0.778377533f);
}

TEST(QuaternionTests, angle_axis_is_correct)
{
    const auto result = bee::axis_angle(bee::float3(-0.23405f, 120.0f, 5.1f), 0.05f);
    ASSERT_FLOAT_EQ(result.w, 0.999687493f);
    ASSERT_FLOAT_EQ(result.x, -0.00585064059f);
    ASSERT_FLOAT_EQ(result.y, 2.99968767f);
    ASSERT_FLOAT_EQ(result.z, 0.127486721f);
}

TEST(QuaternionTests, make_rotation_works)
{
    const auto result = bee::make_rotation(bee::float3(1.0f, -2.0f, -23.0f), bee::float3(0.0f, 0.2345f, 90.0f));
    ASSERT_FLOAT_EQ(result.w, 0.0472791828f);
    ASSERT_FLOAT_EQ(result.x, -0.887873768f);
    ASSERT_FLOAT_EQ(result.y, -0.457649857f);
    ASSERT_FLOAT_EQ(result.z, 0.00119243201f);
}

TEST(QuaternionTests, look_rotation_is_correct)
{
    const auto result = bee::look_rotation(bee::float3(0.23f, 12.4f, -3.0429f), bee::float3(0.0f, 1.0f, 0.0f));
    ASSERT_FLOAT_EQ(result.w, 0.0296822451f);
    ASSERT_FLOAT_EQ(result.x, -0.0232631955f);
    ASSERT_FLOAT_EQ(result.y, 0.786512434f);
    ASSERT_FLOAT_EQ(result.z, 0.616422057f);
}