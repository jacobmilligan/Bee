/*
 *  Entry.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/String.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Application/AppLoop.hpp"

#include <stdlib.h>


/**
 * This is the main entry point for all Bee GUI-based applications. Bee will implement a platform-specific
 * GUI main (i.e. WinMain) and then call into `bee_main`. To use, statically link the exe target with
 * `Bee.Application` and then add `int bee_main(int argc, char** argv)` to any .cpp file
 */
extern int bee_main(int argc, char** argv);


namespace bee {


// Called by all main functions before platform-specific implementations to setup core globals
void preinit_main()
{
    current_thread::set_as_main();

    /*
     * Register logger before handlers so we can print something if anything goes wrong with their initialization.
     * If the logger fails to init and causes an exception well that's just too bad
     */
    logger_init();
    init_signal_handler();
    enable_exception_handling();
}


} // namespace bee


#if BEE_GUI_APP == 1 && BEE_OS_WINDOWS == 1

#include "Bee/Core/Win32/MinWindows.h"
#include "shellapi.h"

int WinMain(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    bee::preinit_main();

    int argc = 0;
    auto command_line = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (BEE_FAIL_F(command_line != nullptr, "Failed to parse command line"))
    {
        return EXIT_FAILURE;
    }

    auto utf8_args = bee::FixedArray<bee::String>::with_size(argc);
    auto argv = bee::FixedArray<char*>::with_size(argc);

    for (int arg_idx = 0; arg_idx < argc; ++arg_idx)
    {
        utf8_args[arg_idx] = bee::str::from_wchar(command_line[arg_idx]);
        argv[arg_idx] = utf8_args[arg_idx].data();
    }

    auto return_code = bee_main(argc, argv.data());

    LocalFree(command_line);

    return return_code;
}

#else


// TODO(Jacob): get game/app name from project settings
int main(int argc, char** argv)
{
    bee::preinit_main();
    return bee_main(argc, argv);
}


#endif // BEE_GUI_APP == 1

