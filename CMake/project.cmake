include(${CMAKE_CURRENT_LIST_DIR}/compile_flags.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/version.cmake)

set(BEE_PROJECT_ROOT ${PROJECT_SOURCE_DIR})
set(BEE_SOURCE_ROOT ${BEE_PROJECT_ROOT}/Source)
set(BEE_THIRD_PARTY ${BEE_PROJECT_ROOT}/ThirdParty)

if (BUILD_TESTS)
    include(GoogleTest)
endif ()


################################################################################
#
# Begins a new source root - this resets all previously added source files and
# will persist until the next `bee_new_source_root` or until a `bee_` target is
# added
#
################################################################################
function(bee_new_source_root)
    set(__bee_sources "" CACHE INTERNAL "")
endfunction()


################################################################################
#
# Initializes a bee project and all of its variables
#
################################################################################
function(bee_begin)
    set(__bee_include_dirs ${BEE_SOURCE_ROOT} CACHE INTERNAL "")
    bee_new_source_root()
endfunction()

function(bee_end)
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

    target_include_directories(${name} PUBLIC ${__bee_include_dirs})
    __bee_set_compile_options(${name})
    bee_new_source_root()
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
    if (ARGS_DYNAMIC)
        add_library(${name} SHARED ${__bee_sources})
    else()
        add_library(${name} STATIC ${__bee_sources})
    endif ()

    if (ARGS_LINK_LIBRARIES)
        target_link_libraries(${name} PUBLIC ${ARGS_LINK_LIBRARIES})
    endif ()

    target_include_directories(${name} PUBLIC ${__bee_include_dirs})
    __bee_set_compile_options(${name})
    bee_new_source_root()
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
        message("${ARGS_SOURCES}")
        bee_add_sources(${ARGS_SOURCES})
        bee_exe(${name} LINK_LIBRARIES Bee.TestMain ${ARGS_LINK_LIBRARIES})

        __bee_set_compile_options(${name})

        gtest_add_tests(TARGET ${name})

        bee_new_source_root()
        bee_add_sources(${cached_sources})
    endif ()
endfunction()