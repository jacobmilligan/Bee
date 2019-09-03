//
//  UnixProcess.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 2018-12-21
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Skyrocket/Platform/Process.hpp"

#include <unistd.h>
#include <sys/errno.h>

namespace bee {
namespace process {


bool exec(Handle* handle, const i32 argc, const char* const* argv, const Path& working_directory)
{
    const auto chdir_return = chdir(working_directory.c_str());
    if (BEE_FAIL_F(chdir_return == 0, "Process: error changing to working directory: %s", strerror(errno))) {
        return false;
    }

    handle->pid = fork();

    if (handle->pid != 0) {
        // calling process
        return true;
    }

    const auto execv_return = execv(argv[0], (char* const*)argv);
    return BEE_CHECK_F(execv_return != -1, "Process: exec_process error: %s", strerror(errno));
}



} // namespace process
} // namespace bee