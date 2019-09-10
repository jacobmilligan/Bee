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

#include <Bee/Core/Logger.hpp>

#include <gtest/gtest.h>


// Bee logging callback to ensure logging is detected by GTest Death tests etc.
void test_logger_callback(const bee::LogVerbosity verbosity, const char* fmt, va_list args)
{
    FILE* handle = verbosity == bee::LogVerbosity::error ? stderr : stdout;
    vfprintf(handle, fmt, args);
}


GTEST_API_ int main(int argc, char **argv)
{
    printf("Running Bee TestMain...\n");
    testing::InitGoogleTest(&argc, argv);
    bee::logger_init();
    bee::log_register_callback(test_logger_callback);
    return RUN_ALL_TESTS();
}
