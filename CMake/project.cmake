################################################################################
#
# Setup global properties and variables for the build system to use
#
################################################################################
include(${CMAKE_CURRENT_LIST_DIR}/version.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/options.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/compile_flags.cmake)

if (BUILD_TESTS)
    include(GoogleTest)
endif ()

set(BEE_CMAKE_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BEE_PROJECT_ROOT ${PROJECT_SOURCE_DIR})
set(BEE_RUNTIME_ROOT ${BEE_PROJECT_ROOT}/Source/Runtime)
set(BEE_DEVELOP_ROOT ${BEE_PROJECT_ROOT}/Source/Develop)
set(BEE_THIRD_PARTY ${BEE_PROJECT_ROOT}/ThirdParty)
set(BEE_BUILD_ROOT ${BEE_PROJECT_ROOT}/Build)
set(BEE_DEBUG_BINARY_DIR ${BEE_BUILD_ROOT}/Debug)
set(BEE_RELEASE_BINARY_DIR ${BEE_BUILD_ROOT}/Release)

# Setup compiler variables
set(BEE_COMPILER_IS_MSVC FALSE)
set(BEE_COMPILER_IS_CLANG FALSE)
set(BEE_COMPILER_IS_GCC FALSE)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(BEE_COMPILER_IS_MSVC TRUE)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(BEE_COMPILER_IS_CLANG TRUE)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(BEE_COMPILER_IS_GCC TRUE)
endif ()

if (NOT BEE_PLATFORM)
    if (WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(BEE_PLATFORM Win64)
        else()
            set(BEE_PLATFORM Win32)
        endif()
    elseif(APPLE)
        set(BEE_PLATFORM macOS)
    else()
        set(BEE_PLATFORM Linux)
    endif ()
endif ()

# remove assertions in all release builds no matter what
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:RelWithDebInfo>:BEE_ENABLE_ASSERTIONS=0>
    $<$<CONFIG:Release>:BEE_ENABLE_ASSERTIONS=0>
    $<$<CONFIG:MinSizeRel>:BEE_ENABLE_ASSERTIONS=0>
)

############################
#
# Various logging helpers
#
############################
function(bee_log msg)
    MESSAGE(STATUS "Bee: ${msg}")
endfunction()

################################################################################
#
# Begins a new source root - this resets all previously added source files and
# will persist until the next_free `bee_new_source_root` or until a `bee_` target is
# added
#
################################################################################
function(bee_new_source_root)
    set(__bee_sources "" CACHE INTERNAL "")
    set(__bee_defines "" CACHE INTERNAL "")
endfunction()


################################################################################
#
# Initializes a bee project and all of its variables
#
################################################################################
function(bee_begin)
    set(__bee_include_dirs ${BEE_RUNTIME_ROOT} ${BEE_DEVELOP_ROOT} CACHE INTERNAL "")
    set(__bee_libraries "" CACHE INTERNAL "")
    bee_new_source_root()
endfunction()

function(bee_end)
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        add_custom_target(NatVis SOURCES ${BEE_CMAKE_ROOT}/Bee.natvis)
    endif ()
    bee_begin()
endfunction()

################################################################################
#
# Adds the source files given in ${ARGN} to the current bee source root. All
# filepaths should be relative to the current ${CMAKE_SOURCE_DIR} and will
# be turned into absolute paths by this function
#
################################################################################
function(bee_add_sources)
    set(src_list)
    foreach(src ${ARGN})
        if (NOT IS_ABSOLUTE "${src}")
            get_filename_component(src "${src}" ABSOLUTE)
        endif ()
        list(APPEND src_list "${src}")
    endforeach()
    list(REMOVE_DUPLICATES src_list)
    set(__bee_sources ${__bee_sources} "${src_list}" CACHE INTERNAL "")
endfunction()


################################################################################
#
# Adds PUBLIC compile definitions to the current target
#
################################################################################
function(bee_add_compile_definitions)
    foreach (def ${ARGN})
        set(__bee_defines ${__bee_defines} ${def} CACHE INTERNAL "")
    endforeach ()
endfunction()

################################################################################
#
# Adds the source files given in ${ARGN} to the current bee source root. All
# filepaths should be relative to the current ${CMAKE_SOURCE_DIR} and will
# be turned into absolute paths by this function
#
################################################################################
function(bee_add_include_dirs)
    set(dir_list)
    foreach(dir ${ARGN})
        if (NOT IS_ABSOLUTE "${dir}")
            get_filename_component(dir "${dir}" ABSOLUTE)
        endif ()
        list(APPEND dir_list "${dir}")
    endforeach()
    list(REMOVE_DUPLICATES dir_list)
    set(__bee_include_dirs ${__bee_include_dirs} "${dir_list}" CACHE INTERNAL "")
endfunction()


function(__bee_finalize_target name)
    target_include_directories(${name} PUBLIC ${__bee_include_dirs})
    __bee_set_compile_options(${name})
    set_target_properties(${name}
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BEE_DEBUG_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BEE_DEBUG_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BEE_DEBUG_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${BEE_RELEASE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BEE_RELEASE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BEE_RELEASE_BINARY_DIR}
    )

    if (${BEE_COMPILER_IS_MSVC})
        target_compile_options(${name} PUBLIC "/wd4251")
    endif ()

    foreach (dep ${__bee_libraries})
        if (NOT "${dep}" STREQUAL "${name}")
            __bee_get_api_macro(${dep} api_macro)
            target_compile_definitions(${name} PRIVATE ${api_macro}=BEE_IMPORT_SYMBOL)
        endif ()
    endforeach ()

    target_compile_definitions(${name} PUBLIC ${__bee_defines})
    bee_new_source_root()
endfunction()


function(__bee_get_api_macro target dst)
    # Setup a <module_name>_API macro as a compile define
    string(REPLACE "Bee" "BEE" api_macro "${target}")
    string(REPLACE "." "_" api_macro "${api_macro}")
    string(REPLACE "-" "_" api_macro "${api_macro}")
    string(REPLACE " " "_" api_macro "${api_macro}")
    string(TOUPPER "${api_macro}_API" api_macro)

    string(FIND "${target}" "Bee" bee_name_idx)
    if (NOT ${bee_name_idx} EQUAL 0)
        set(api_macro "BEE_THIRD_PARTY_${api_macro}")
    endif ()

    set(${dst} ${api_macro} PARENT_SCOPE)
endfunction()

################################################################################
#
# Adds a new executable target with the given name, linking against the libraries
# specified in `link_libraries`
#
# OPTIONAL ARGS:
#   * `GUI` - adds the executable as a GUI app (WIN32 or MACOSX_BUNDLE)
#   * `LINK_LIBRARIES` - a series of library names to link via
#     `target_link_libraries`
#
################################################################################
function(bee_exe name)
    cmake_parse_arguments(ARGS "GUI" "" "LINK_LIBRARIES" ${ARGN})
    if (ARGS_GUI)
        if (APPLE)
            add_executable(${name} MACOSX_BUNDLE ${__bee_sources})
        elseif(WIN32)
            add_executable(${name} WIN32 ${__bee_sources})
        else()
            add_executable(${name} ${__bee_sources})
        endif ()

        target_compile_definitions(${name} PUBLIC BEE_GUI_APP)
    else()
        add_executable(${name} ${__bee_sources})
    endif ()

    if (ARGS_LINK_LIBRARIES)
        target_link_libraries(${name} PUBLIC ${ARGS_LINK_LIBRARIES})
    endif ()

    __bee_finalize_target(${name})
endfunction()


################################################################################
#
# Adds a new library target with the given name, linking against the libraries
# specified in `link_libraries`
#
# OPTIONAL ARGS:
#   * `SHARED` - adds target as SHARED library (i.e. .dll on windows),
#     otherwise it will be STATIC
#   * `LINK_LIBRARIES` - a series of library names to link via
#     `target_link_libraries`
#
################################################################################
function(bee_library name)
    cmake_parse_arguments(ARGS "SHARED" "" "LINK_LIBRARIES" ${ARGN})

    __bee_get_api_macro(${name} api_macro)

    if (ARGS_SHARED AND NOT MONOLITHIC_BUILD)
        add_library(${name} SHARED ${__bee_sources})
        target_compile_definitions(${name} PRIVATE BEE_DLL)
    else()
        add_library(${name} STATIC ${__bee_sources})
    endif ()

    target_compile_definitions(${name} PRIVATE ${api_macro}=BEE_EXPORT_SYMBOL)

    if (ARGS_LINK_LIBRARIES)
        target_link_libraries(${name} PUBLIC ${ARGS_LINK_LIBRARIES})
    endif ()

    set(__bee_libraries ${__bee_libraries} ${name} CACHE INTERNAL "")

    __bee_finalize_target(${name})
endfunction()


################################################################################
#
# Adds the source files to a google test executable target
#
#   * `LINK_LIBRARIES` - a series of library names to link via
#     `target_link_libraries`
#
################################################################################
function(bee_test name)
    if (BUILD_TESTS)
        cmake_parse_arguments(ARGS "" "" "LINK_LIBRARIES;SOURCES" ${ARGN})

        set(cached_sources ${__bee_sources})

        bee_new_source_root()
        bee_add_sources(${BEE_RUNTIME_ROOT}/Bee/TestMain.cpp ${ARGS_SOURCES})
        bee_exe(${name} LINK_LIBRARIES ${ARGS_LINK_LIBRARIES} gtest)

        __bee_set_compile_options(${name})
        gtest_add_tests(TARGET ${name} ${ARGS_SOURCES})

        bee_new_source_root()
        bee_add_sources(${cached_sources})
    endif ()
endfunction()