//
//  TestMain.cpp
//  Bee
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 7/08/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include <Bee/Core/Bee.hpp>
#include <gtest/gtest.h>

// Bee logging callback to ensure logging is detected by GTest Death tests etc.
void test_logger_callback(const bee::LogVerbosity verbosity, const char* fmt, va_list args)
{
    FILE* handle = verbosity == bee::LogVerbosity::error ? stderr : stdout;
    vfprintf(handle, fmt, args);
}


GTEST_API_ int bee_main(int argc, char **argv)
{
    printf("Running TestMain...\n");
    bee::log_register_callback(test_logger_callback);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
