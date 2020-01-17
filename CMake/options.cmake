function(bee_option_with_definition name help value)
    option(${name} ${help} ${value})
    if (${name} MATCHES ON)
        add_compile_definitions(BEE_CONFIG_${name}=1)
    else()
        add_compile_definitions(BEE_CONFIG_${name}=0)
    endif ()
endfunction()

################################################
#
# Options the user can set when configuring
# the CMake build system for Bee
#
################################################

option(BUILD_TESTS "Builds unit and performance tests" ON)
option(BUILD_CLANG_TOOLS "Builds LLVM/clang-based tools such as bee-reflect. " OFF)
option(USE_ASAN "Compiles with Address Sanitizer active" OFF)
option(USE_TSAN "Compiles with Thread Sanitizer active" OFF)
bee_option_with_definition(VULKAN_BACKEND "Uses the Vulkan API as the GPU Backend" ON) # on by default until other platforms are supported
bee_option_with_definition(ENABLE_MEMORY_TRACKING "Allows dynamically switching memory tracking on or off" ON)
bee_option_with_definition(FORCE_MEMORY_TRACKING "Forces memory tracking on even in release builds" OFF)
bee_option_with_definition(FORCE_ASSERTIONS_ENABLED "Enable assertions messages - crash and logging behaviour is build-type dependent" OFF)
bee_option_with_definition(DISABLE_REFLECTION "Disables generating reflection data as a build step" OFF)

# Validate LLVM install directory
if (${BUILD_CLANG_TOOLS} MATCHES ON)
    if (NOT EXISTS ${LLVM_INSTALL_DIR})
        message(FATAL_ERROR "\n Invalid LLVM_INSTALL_DIR \"${LLVM_INSTALL_DIR}\" given. Cannot build clang tools without a valid LLVM installation\n")
    endif ()

    set(llvm_cmake_dir ${LLVM_INSTALL_DIR}/lib/cmake/llvm)
    set(clang_cmake_dir ${LLVM_INSTALL_DIR}/lib/cmake/clang)

    # Fatal error if either LLVM and Clang weren't found - turn off llvm option in this case
    if (NOT EXISTS ${llvm_cmake_dir} OR NOT EXISTS ${clang_cmake_dir})
        set(BUILD_CLANG_TOOLS OFF)
        message(FATAL_ERROR "\nUnable to find LLVM. Cannot build clang tools without a valid LLVM installation\n")
    endif ()

    # success - can build LLVM-based tools. Add the config paths to
    list(APPEND CMAKE_PREFIX_PATH "${llvm_cmake_dir}")
    list(APPEND CMAKE_PREFIX_PATH "${clang_cmake_dir}")
endif ()
