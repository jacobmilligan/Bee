add_subdirectory(BeeBuild)

if (BUILD_CLANG_TOOLS)
    bee_log("Building clang-based tools")

    find_package(Clang REQUIRED CONFIG)

    # Require that clang tooling is based off LLVM version 10
    if (NOT ${LLVM_VERSION_MAJOR} EQUAL 10)
        message(FATAL_ERROR "LLVM version ${LLVM_PACKAGE_VERSION} is not compatible with Bee")
    endif ()

    bee_log("Using LLVM installation located at ${LLVM_BINARY_DIR}")

    include_directories(${LLVM_INCLUDE_DIRS})

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(bee_clang_tool_compile_options /wd4146 /wd4244 /wd4458 /wd4245 /wd4624 /wd4324 /wd4389 /wd4291 /wd4459)
    endif()

    add_subdirectory(BeeReflect)
endif ()

if (WIN32)
    add_custom_target(Bee.Tools.PackageBinaries ALL)
    add_custom_command(
        TARGET Bee.Tools.PackageBinaries
        COMMAND cmd.exe /c .\\package.bat
        WORKING_DIRECTORY ${BEE_PROJECT_ROOT}/Tools
        COMMENT "Bee: packaging tool binaries..."
        USES_TERMINAL
        VERBATIM
    )
else()
    bee_fail("Tool binary package target not implemented on ${BEE_PLATFORM}")
endif ()
