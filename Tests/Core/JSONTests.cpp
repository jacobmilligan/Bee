/*
 *  JSONTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/JSON/JSON.hpp>

#include <GTest.hpp>

// TODO(Jacob):
//  - negatives
//  - scientific notation
//  - unicode
//  - leading zeros disallowed
//  - double and integer overflow/underflow
//  - escape chars
//  - unfinished string and escapes
//  - non-printable characters
//  - error codes and strings

TEST(JSONTests, empty_object)
{
    char json_str[] = "{}";
    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success);

    ASSERT_EQ(doc.get_type(doc.root()), bee::json::ValueType::object);
    ASSERT_TRUE(doc.get_data(doc.root()).has_children());
}

TEST(JSONTests, object_string_keys)
{
    char json_str[] = R"(
{
    "key": 23,
    "key2": "string value",
    "key3": true,
    "key4": null,
    "key5": false
})";
    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    ASSERT_EQ(doc.get_type(doc.root()), bee::json::ValueType::object);
    ASSERT_TRUE(doc.get_data(doc.root()).has_children());

    ASSERT_DOUBLE_EQ(doc.get_member_data(doc.root(), "key").as_number(), 23.0);
    ASSERT_STREQ(doc.get_member_data(doc.root(), "key2").as_string(), "string value");
    ASSERT_TRUE(doc.get_member_data(doc.root(), "key3").as_boolean());
    ASSERT_EQ(doc.get_member_data(doc.root(), "key4").type, bee::json::ValueType::null);
    ASSERT_FALSE(doc.get_member_data(doc.root(), "key5").as_boolean());
}

TEST(JSONTests, nested_objects)
{
    char json_str[] = R"(
{
    "lvl1": {
        "lvl2": {
            "lvl3": {
                "number": 23
            },
            "boolean": true
        },
        "null": null
    }
})";

    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    const auto lvl1 = doc.get_member(doc.root(), "lvl1");
    ASSERT_TRUE(lvl1.is_valid());

    const auto lvl2 = doc.get_member(lvl1, "lvl2");
    ASSERT_TRUE(lvl2.is_valid());

    const auto lvl3 = doc.get_member(lvl2, "lvl3");
    ASSERT_TRUE(lvl3.is_valid());

    ASSERT_FALSE(doc.has_member(doc.root(), "lvl2"));
    ASSERT_FALSE(doc.has_member(lvl1, "lvl3"));
    ASSERT_FALSE(doc.has_member(lvl2, "lvl1"));

    ASSERT_EQ(doc.get_member_type(lvl1, "null"), bee::json::ValueType::null);
    ASSERT_TRUE(doc.get_member_data(lvl2, "boolean").as_boolean());
    ASSERT_DOUBLE_EQ(doc.get_member_data(lvl3, "number").as_number(), 23.0);
}

TEST(JSONTests, empty_array)
{
    char json_str[] = "[]";
    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    ASSERT_EQ(doc.get_type(doc.root()), bee::json::ValueType::array);
    ASSERT_TRUE(doc.get_data(doc.root()).has_children());
}

TEST(JSONTests, array_elements)
{
    char json_str[] = "[1, 2, 3, 5]";
    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    ASSERT_TRUE(doc.get_data(doc.root()).has_children());
    ASSERT_DOUBLE_EQ(doc.get_element_data(doc.root(), 0).as_number(), 1.0);
}

TEST(JSONTests, nested_array_elements)
{
    char json_str[] = "[1, [2, [3, [4]]]]";
    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    auto cur_handle = doc.root();

    static constexpr double expected_values[] = { 1.0, 2.0, 3.0, 4.0 };

    for (double val : expected_values) {
        ASSERT_TRUE(cur_handle.is_valid());
        ASSERT_TRUE(doc.get_data(cur_handle).has_children());
        ASSERT_DOUBLE_EQ(doc.get_element_data(cur_handle, 0).as_number(), val);

        cur_handle = doc.get_element(cur_handle, 1);
    }
}

TEST(JSONTests, combo_of_objects_and_arrays)
{
    char json_str[] = R"(
[
    1,
    [
        2,
        [
            3,
            [
                4,
                {
                    "value": [
                        1,
                        2,
                        3,
                        4,
                        {
                            "number": 2
                        }
                    ]
                }
            ]
        ]
    ]
])";
    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    auto cur_handle = doc.root();

    static constexpr double expected_values[] = { 1.0, 2.0, 3.0, 4.0 };

    for (double val : expected_values) {
        ASSERT_TRUE(cur_handle.is_valid());
        ASSERT_TRUE(doc.get_data(cur_handle).has_children());
        ASSERT_DOUBLE_EQ(doc.get_element_data(cur_handle, 0).as_number(), val);

        cur_handle = doc.get_element(cur_handle, 1);
    }

    const auto object_element = doc.get_data(cur_handle);
    ASSERT_TRUE(object_element.is_valid());
    ASSERT_TRUE(object_element.has_children());
    ASSERT_EQ(object_element.type, bee::json::ValueType::object);

    cur_handle = doc.get_member(cur_handle, "value");
    const auto object_member = doc.get_data(cur_handle);
    ASSERT_TRUE(object_member.is_valid());
    ASSERT_TRUE(object_member.has_children());
    ASSERT_EQ(object_member.type, bee::json::ValueType::array);

    for (int i = 0; i < 4; ++i) {
        const auto element = doc.get_element_data(cur_handle, i);
        ASSERT_TRUE(element.is_valid());
        ASSERT_EQ(element.type, bee::json::ValueType::number);
        ASSERT_DOUBLE_EQ(element.as_number(), expected_values[i]);
    }

    cur_handle = doc.get_element(cur_handle, 4);
    const auto deep_object = doc.get_data(cur_handle);
    ASSERT_TRUE(deep_object.is_valid());
    ASSERT_TRUE(deep_object.has_children());
    ASSERT_EQ(deep_object.type, bee::json::ValueType::object);

    const auto number_member = doc.get_member_data(cur_handle, "number");
    ASSERT_TRUE(number_member.is_valid());
    ASSERT_EQ(number_member.type, bee::json::ValueType::number);
    ASSERT_DOUBLE_EQ(number_member.as_number(), 2.0);
}

TEST(JSONTests, without_commas_requires_whitespace)
{
    char json_str[] = R"(
{
    "lvl1-a": {

        "lvl2": {

            "boolean": true"bad":null

            "boolean2": false

        }

        "null": null
    }

    "lvl1-b": 42
})";

    bee::json::ParseOptions options{};
    options.require_commas = false;

    bee::json::Document doc(options);
    const auto success = doc.parse(json_str);
    ASSERT_FALSE(success);

    bee::String error = "JSON parse error at: 7:28: expected whitespace character for member or "
                        "element separator (`require_commas` == false) but found '\"' instead";
    ASSERT_EQ(error, doc.get_error_string());
};

TEST(JSONTests, without_commas_succeeds)
{
    char json_str[] = R"(
{
    "lvl1-a": {

        "lvl2": {

            "boolean": true "good": 23

            "boolean2": false

        }

        "null": null
    }

    "lvl1-b": 42
})";

    bee::json::ParseOptions options{};
    options.require_commas = false;

    bee::json::Document doc(options);
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    const auto lvl1a = doc.get_member(doc.root(), "lvl1-a");
    const auto lvl1a_val = doc.get_data(lvl1a);
    ASSERT_EQ(lvl1a_val.type, bee::json::ValueType::object);
    ASSERT_TRUE(lvl1a_val.has_children());

    ASSERT_DOUBLE_EQ(doc.get_member_data(doc.root(), "lvl1-b").as_number(), 42.0);

    const auto lvl2 = doc.get_member(lvl1a, "lvl2");
    const auto lvl2_val = doc.get_data(lvl2);
    ASSERT_EQ(lvl2_val.type, bee::json::ValueType::object);
    ASSERT_TRUE(lvl2_val.has_children());

    ASSERT_EQ(doc.get_member_type(lvl1a, "null"), bee::json::ValueType::null);

    ASSERT_TRUE(doc.has_member(lvl2, "boolean"));
    ASSERT_TRUE(doc.has_member(lvl2, "good"));
    ASSERT_TRUE(doc.has_member(lvl2, "boolean2"));

    ASSERT_TRUE(doc.get_member_data(lvl2, "boolean").as_boolean());
    ASSERT_DOUBLE_EQ(doc.get_member_data(lvl2, "good").as_number(), 23.0);
    ASSERT_FALSE(doc.get_member_data(lvl2, "boolean2").as_boolean());
};

TEST(JSONTests, without_root_element)
{
    char json_str[] = R"({"key": 23, "key2": "string value", "key4": null, "key5": false})";
    char json_without_root[1024] = {0};
    memcpy(json_without_root, json_str + 1, strlen(json_str) - 2);

    bee::json::ParseOptions options{};
    bee::json::Document doc(options);
    auto success = doc.parse(json_without_root);
    ASSERT_FALSE(success) << doc.get_error_string().c_str();

    // clear the string after in-place parsing modification
    memcpy(json_without_root, json_str + 1, strlen(json_str) - 2);
    options.require_root_element = false;
    bee::json::Document doc2(options);
    success = doc2.parse(json_without_root);
    ASSERT_TRUE(success) << doc2.get_error_string().c_str();

    ASSERT_DOUBLE_EQ(doc2.get_member_data(doc2.root(), "key").as_number(), 23.0);
    ASSERT_STREQ(doc2.get_member_data(doc2.root(), "key2").as_string(), "string value");
    ASSERT_EQ(doc2.get_member_type(doc2.root(), "key4"), bee::json::ValueType::null);
    ASSERT_FALSE(doc2.get_member_data(doc2.root(), "key5").as_boolean());
}

TEST(JSONTests, without_string_keys)
{
    char json_str[] = R"({key: 23, key2: "string value", key4: null, key5: false})";
    char json_without_string_keys[1024] = {0};
    strcpy(json_without_string_keys, json_str);

    bee::json::ParseOptions options{};
    bee::json::Document doc(options);
    auto success = doc.parse(json_without_string_keys);
    ASSERT_FALSE(success) << doc.get_error_string().c_str();

    strcpy(json_without_string_keys, json_str);
    options.require_string_keys = false;
    bee::json::Document doc2(options);
    success = doc2.parse(json_without_string_keys);
    ASSERT_TRUE(success) << doc2.get_error_string().c_str();

    ASSERT_DOUBLE_EQ(doc2.get_member_data(doc2.root(), "key").as_number(), 23.0);
    ASSERT_STREQ(doc2.get_member_data(doc2.root(), "key2").as_string(), "string value");
    ASSERT_EQ(doc2.get_member_type(doc2.root(), "key4"), bee::json::ValueType::null);
    ASSERT_FALSE(doc2.get_member_data(doc2.root(), "key5").as_boolean());
}

TEST(JSONTests, with_comments)
{
    char json_str[] = R"( # let's begin with a comment, ey
{
    "key": 23, # comment here
    "key2": "string value",
    # comment here, too!
    "key4": null,
    "key5": false
})";
    char json_with_comments[1024] = {0};
    strcpy(json_with_comments, json_str);

    bee::json::ParseOptions options{};
    bee::json::Document doc(options);
    auto success = doc.parse(json_with_comments);
    ASSERT_FALSE(success) << doc.get_error_string().c_str();

    strcpy(json_with_comments, json_str);
    options.allow_comments = true;
    bee::json::Document doc2(options);
    success = doc2.parse(json_with_comments);
    ASSERT_TRUE(success) << doc2.get_error_string().c_str();

    ASSERT_DOUBLE_EQ(doc2.get_member_data(doc2.root(), "key").as_number(), 23.0);
    ASSERT_STREQ(doc2.get_member_data(doc2.root(), "key2").as_string(), "string value");
    ASSERT_EQ(doc2.get_member_type(doc2.root(), "key4"), bee::json::ValueType::null);
    ASSERT_FALSE(doc2.get_member_data(doc2.root(), "key5").as_boolean());
}

TEST(JSONTests, with_multiline_strings)
{
    const char* raw_shader = R"(
        cbuffer Params : register(b0) {
            float4x4 mvp;
        };

        // testing apostrophe in middle of string
'
'
'
        struct AppData {
            float4 position: POSITION;
            float4 color: COLOR;
        };

        struct FragIn {
            float4 position: SV_POSITION;
            float4 color: COLOR;
        };

        FragIn vert(AppData IN)
        {
            FragIn OUT;
            OUT.position = mul(mvp, IN.position);
            OUT.color = IN.color;
            return OUT;
        }

        float4 frag(FragIn IN): SV_TARGET
        {
            return IN.color;
        }
)";

    const char* raw_escape_chars = R"(\n\r\0 escape characters are added verbatim and not escaped)";

    bee::String json_str = R"({ "key": 23, "shader_raw": ''')";
    json_str += raw_shader;
    json_str += R"(''', "escape_raw": ''')";
    json_str += raw_escape_chars;
    json_str += "''' }";


    char json_with_comments[1024] = {0};
    strcpy(json_with_comments, json_str.c_str());

    bee::json::ParseOptions options{};
    bee::json::Document doc(options);
    auto success = doc.parse(json_with_comments);
    ASSERT_FALSE(success) << doc.get_error_string().c_str();

    strcpy(json_with_comments, json_str.c_str());
    options.allow_multiline_strings = true;
    bee::json::Document doc2(options);
    success = doc2.parse(json_with_comments);
    ASSERT_TRUE(success) << doc2.get_error_string().c_str();

    ASSERT_DOUBLE_EQ(doc2.get_member_data(doc2.root(), "key").as_number(), 23.0);
    ASSERT_STREQ(doc2.get_member_data(doc2.root(), "shader_raw").as_string(), raw_shader);
    ASSERT_STREQ(doc2.get_member_data(doc2.root(), "escape_raw").as_string(), raw_escape_chars);
}

TEST(JSONTests, all_relaxed_options_on)
{
    const char* raw_shader = R"(
    /*
     * input parameters
     */
    cbuffer Params : register(b0) {
        float4x4 mvp;
    };

    struct AppData {
        float4 position: POSITION;
        float4 color: COLOR;
    };

    struct FragIn {
        float4 position: SV_POSITION;
        float4 color: COLOR;
    };

    FragIn vert(AppData IN)
    {
        FragIn OUT;
        // transform to screen space
        OUT.position = mul(mvp, IN.position);
        OUT.color = IN.color;
        return OUT;
    }

    float4 frag(FragIn IN): SV_TARGET
    {
        return IN.color;
    }
)";

    bee::String json_str = R"(
# this is a shadecc test file
# it uses all relaxed options
info: {
    name: "test shader"
    vertex_function: "vert"
    fragment_function: "frag"
}

inputs: {
    # input MVP matrix
    mvp: "float4x4"
}

shader: ''')";
    json_str += raw_shader;
    json_str += "'''";

    char relaxed_json_str[1024] = {0};
    strcpy(relaxed_json_str, json_str.c_str());

    bee::json::ParseOptions options{};
    bee::json::Document doc(options);
    auto success = doc.parse(relaxed_json_str);
    ASSERT_FALSE(success) << doc.get_error_string().c_str();

    strcpy(relaxed_json_str, json_str.c_str());

    options.allow_multiline_strings = true;
    options.allow_comments = true;
    options.require_string_keys = false;
    options.require_root_element = false;
    options.require_commas = false;
    bee::json::Document relaxed_doc(options);
    success = relaxed_doc.parse(relaxed_json_str);
    ASSERT_TRUE(success) << relaxed_doc.get_error_string().c_str();

    const auto info = relaxed_doc.get_member(relaxed_doc.root(), "info");
    const auto inputs = relaxed_doc.get_member(relaxed_doc.root(), "inputs");
    const auto shader = relaxed_doc.get_member(relaxed_doc.root(), "shader");

    ASSERT_TRUE(info.is_valid());
    ASSERT_TRUE(inputs.is_valid());
    ASSERT_TRUE(shader.is_valid());

    ASSERT_STREQ(relaxed_doc.get_member_data(info, "name").as_string(), "test shader");
    ASSERT_STREQ(relaxed_doc.get_member_data(info, "vertex_function").as_string(), "vert");
    ASSERT_STREQ(relaxed_doc.get_member_data(info, "fragment_function").as_string(), "frag");

    ASSERT_STREQ(relaxed_doc.get_member_data(inputs, "mvp").as_string(), "float4x4");

    ASSERT_STREQ(relaxed_doc.get_data(shader).as_string(), raw_shader);
}

TEST(JSONTests, number_parsing)
{
    char json_str[] = "[1.238421230000, 2.2394509, 1.0E+2, 1e-2, 1E6, 5, 0.0909]";
    bee::json::Document doc(bee::json::ParseOptions{});
    const auto success = doc.parse(json_str);
    ASSERT_TRUE(success) << doc.get_error_string().c_str();

    ASSERT_TRUE(doc.get_data(doc.root()).has_children());

    double expected[] = { 1.23842123, 2.2394509, 1.0E+2, 1E-2, 1E6, 5.0, 0.0909 };
    int i = 0;
    for (auto handle : doc.get_elements_range(doc.root()))
    {
        const auto actual = doc.get_data(handle).as_number();
        ASSERT_DOUBLE_EQ(actual, expected[i]) << "Element: " << i;
        ++i;
    }
}
