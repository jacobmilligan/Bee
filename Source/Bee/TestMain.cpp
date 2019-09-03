//
//  TestMain.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 7/08/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include <Skyrocket/Core/Logger.hpp>

#include <gtest/gtest.h>


// Skyrocket logging callback to ensure logging is detected by GTest Death tests etc.
void test_logger_callback(const sky::LogVerbosity verbosity, const char* fmt, va_list args)
{
    FILE* handle = verbosity == sky::LogVerbosity::error ? stderr : stdout;
    vfprintf(handle, fmt, args);
}


GTEST_API_ int main(int argc, char **argv)
{
    printf("Running Skyrocket TestMain...\n");
    testing::InitGoogleTest(&argc, argv);
    sky::log_register_callback(test_logger_callback);
    return RUN_ALL_TESTS();
}
