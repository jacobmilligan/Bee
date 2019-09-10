/*
 *  Commands.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderCompiler/Compiler.hpp"
#include "Bee/Core/Filesystem.hpp"

#include <dxc/Support/WinIncludes.h>
#include <dxc/dxcapi.h>


namespace bee {


DxcCreateInstanceProc fp_DxcCreateInstance { nullptr };


BSCCompiler::BSCCompiler()
{
    auto dxc_path = fs::get_appdata().binaries_root.join("dxcompiler");
#if BEE_OS_WINDOWS == 1
    dxc_path.set_extension(".dll");
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1

    dxc_dll_ = load_library(dxc_path.c_str());
    if (fp_DxcCreateInstance == nullptr)
    {
        fp_DxcCreateInstance = (DxcCreateInstanceProc)get_library_symbol(dxc_dll_, "DxcCreateInstance");
        BEE_ASSERT(fp_DxcCreateInstance != nullptr);
    }

    fp_DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&dxc_compiler_);
    fp_DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&dxc_library_);

    BEE_ASSERT(dxc_compiler_ != nullptr && dxc_library_ != nullptr);
}

BSCCompiler::~BSCCompiler()
{
    if (dxc_compiler_ != nullptr)
    {
        dxc_compiler_->Release();
        dxc_compiler_ = nullptr;
    }

    if (dxc_library_ != nullptr)
    {
        dxc_library_->Release();
        dxc_library_ = nullptr;
    }
}

bool BSCCompiler::process_command(const BSCShutdownCmd& cmd)
{
    log_info("BSC: Shutting down server");
    shutdown_immediate_ = cmd.immediate;
    shutdown_deferred_ = !cmd.immediate;
    return true;
}


} // namespace bee