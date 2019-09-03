//
//  MacPath.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 2/06/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Path.hpp"

#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

namespace bee {


const char Path::slash = '/';

Path Path::executable_path()
{
    char buf[limits::u16_max];
    uint32_t size = sizeof(buf);
    auto success = _NSGetExecutablePath(buf, &size);

    if ( success != 0 ) {
        BEE_ERROR("Path", "Couldn't get the path to the apps executable directory");
        return Path();
    }

    return Path(buf);
}

Path Path::current_working_directory()
{
    char buf[limits::u16_max];
    const auto path_name = getcwd(buf, sizeof(char) * limits::u16_max);
    if (!BEE_CHECK(path_name != nullptr)) {
        return Path("");
    }

    return Path(buf);
}

void Path::make_real()
{
    if (!exists()) {
        return;
    }
    auto ptr = realpath(path_.c_str(), nullptr);
    if ( ptr == nullptr ) {
        return;
    }
    path_ = ptr;
    free(ptr);
}

bool Path::exists() const
{
    struct stat st{};
    return stat(path_.c_str(), &st) == 0;
}


} // namespace bee