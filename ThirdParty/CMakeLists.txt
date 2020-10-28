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
bee_library(xxHash STATIC)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(xxHash PRIVATE /wd4204 /wd4146)
endif ()


##########################################
#
# cpu_info - rapidjson - volk
#
##########################################
bee_add_include_dirs(cpu_info/include)
bee_add_include_dirs(rapidjson/include)
bee_add_include_dirs(volk)
bee_add_include_dirs(VulkanMemoryAllocator/src)


#################
#
# SPIRV-Reflect
#
#################
bee_new_source_root()
bee_add_sources(SPIRV-Reflect/spirv_reflect.h SPIRV-Reflect/spirv_reflect.c)
bee_add_include_dirs(SPIRV-Reflect)
bee_library(SPIRV-Reflect STATIC)

##################################
#
# SPIRV-Tools & SPIRV-headers
#
##################################
set(SPIRV_SKIP_TESTS ON)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SPIRV-Headers_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers)

add_subdirectory(SPIRV-Tools EXCLUDE_FROM_ALL)

set_target_properties(SPIRV-Tools
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BEE_DEBUG_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BEE_DEBUG_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BEE_DEBUG_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${BEE_RELEASE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BEE_RELEASE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BEE_RELEASE_BINARY_DIR}
)


##############
#
# Tests
#
##############
if (BUILD_TESTS)
    ##############
    #
    # gtest
    #
    ##############

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # disable pthread on win32 for gtest because why would it even look for them?
    if (WIN32)
        set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
    endif ()

    add_subdirectory(
            ${BEE_THIRD_PARTY}/googletest
            ${CMAKE_CURRENT_BINARY_DIR}/googletest
            EXCLUDE_FROM_ALL
    )
    bee_add_include_dirs("${gtest_SOURCE_DIR}/include")

    ############################
    #
    # Relacy Race Detector
    #
    ############################
    bee_add_include_dirs(${BEE_THIRD_PARTY}/relacy)
endif ()

############################
#
# DirectX Shader Compiler
#
############################
bee_add_include_dirs(${BEE_THIRD_PARTY}/Binaries/DirectXShaderCompiler/include)

function(copy_third_party_binaries third_party_dir target_name)
    set(binary_dir ${BEE_THIRD_PARTY}/Binaries/${third_party_dir})
    set(output_dir $<TARGET_FILE_DIR:${target_name}>)
    file(GLOB binaries ${binary_dir}/*.dll ${binary_dir}/*.pdb ${binary_dir}/*.exe ${binary_dir}/*.lib)

    set(command_message "${target_name}: copied third party binaries to output directory:")
    foreach(bin ${binaries})
        file(RELATIVE_PATH rel_path ${BEE_THIRD_PARTY}/Binaries ${bin})
        set(command_message "${command_message}\n  - ${rel_path}")
    endforeach()

    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${binaries} ${output_dir}
        COMMENT "${command_message}"
    )
endfunction()

###########################
#
# LMDB
#
##########################
function(lmdb_configure target)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /wd4267 /wd4146 /wd4244 /wd4996 /wd4333 /wd4172 /wd4389 /wd4456 /wd4127 /wd4457 /wd4706 /wd4701)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        FIND_PACKAGE(Threads REQUIRED)
        target_link_libraries(${target} Threads::Threads)
    endif ()
endfunction()

set(lmdb_root "${BEE_THIRD_PARTY}/lmdb/libraries/liblmdb")

bee_new_source_root()
bee_add_sources(
    ${lmdb_root}/mdb.c
    ${lmdb_root}/lmdb.h
    ${lmdb_root}/midl.h
    ${lmdb_root}/midl.c
)
bee_library(lmdb STATIC)
bee_add_include_dirs(${lmdb_root})
lmdb_configure(lmdb)


###########################
#
# Clang tooling
#
##########################
