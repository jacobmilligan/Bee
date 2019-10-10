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
option(USE_ASAN "Compiles with Google Address Sanitizer active" OFF)
option(USE_TSAN "Compiles with Google Thread Sanitizer active" OFF)

bee_option_with_definition(VULKAN_BACKEND "Uses the Vulkan API as the GPU Backend" ON) # on by default until other platforms are supported
bee_option_with_definition(ENABLE_MEMORY_TRACKING "Allows dynamically switching memory tracking on or off" ON)
bee_option_with_definition(FORCE_MEMORY_TRACKING "Forces memory tracking on even in release builds" OFF)
bee_option_with_definition(FORCE_ASSERTIONS_ENABLED "Enable assertions messages - crash and logging behaviour is build-type dependent" OFF)
