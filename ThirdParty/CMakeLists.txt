##############
#
# xxHash
#
##############
bee_new_source_root()
bee_add_sources(
    xxHash/xxHash.c
    xxHash/xxhash.h
    xxHash/xxh3.h
)
bee_add_include_dirs(xxHash)
bee_library(xxHash)
target_compile_options(xxHash PRIVATE /wd4204 /wd4146)


##########################################
#
# cpu_info - rapidjson - volk
#
##########################################
bee_add_include_dirs(cpu_info/include)
bee_add_include_dirs(rapidjson/include)
bee_add_include_dirs(volk)
bee_add_include_dirs(VulkanMemoryAllocator/src)


##############
#
# gtest
#
##############

if (BUILD_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # disable pthread on win32 for gtest because why would it even look for them?
    if (WIN32)
        set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
    endif ()

    add_subdirectory(
            ${CMAKE_CURRENT_SOURCE_DIR}/googletest
            ${CMAKE_CURRENT_BINARY_DIR}/googletest
            EXCLUDE_FROM_ALL
    )
endif ()