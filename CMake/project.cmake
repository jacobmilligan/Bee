include(TestBigEndian)
TEST_BIG_ENDIAN(BEE_BIG_ENDIAN)
if (${BEE_BIG_ENDIAN})
    add_definitions(-DBEE_BIG_ENDIAN)
else()
    add_definitions(-DBEE_LITTLE_ENDIAN)
endif ()

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
    enable_testing()
endif ()

set(BEE_CMAKE_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BEE_PROJECT_ROOT ${PROJECT_SOURCE_DIR})
set(BEE_SOURCES_ROOT ${BEE_PROJECT_ROOT}/Source)
set(BEE_TESTS_ROOT ${BEE_PROJECT_ROOT}/Tests)
set(BEE_THIRD_PARTY ${BEE_PROJECT_ROOT}/ThirdParty)
set(BEE_BUILD_ROOT ${BEE_PROJECT_ROOT}/Build)
set(BEE_DEBUG_BINARY_DIR ${BEE_BUILD_ROOT}/Debug)
set(BEE_RELEASE_BINARY_DIR ${BEE_BUILD_ROOT}/Release)
set(BEE_GENERATED_ROOT ${BEE_BUILD_ROOT}/Generated)

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

# Setup tool invocations
set(BB_COMMAND "${BEE_RELEASE_BINARY_DIR}/bb.exe")

# remove assertions in all release builds no matter what
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:RelWithDebInfo>:BEE_ENABLE_ASSERTIONS=0>
    $<$<CONFIG:Release>:BEE_ENABLE_ASSERTIONS=0>
    $<$<CONFIG:MinSizeRel>:BEE_ENABLE_ASSERTIONS=0>
)

############################
#
# Various helpers
#
############################
function(bee_log msg)
    MESSAGE(STATUS "Bee: ${msg}")
endfunction()

function(bee_fail msg)
    MESSAGE(FATAL_ERROR "Bee: ${msg}")
endfunction()

macro(bee_parse_version major minor patch version_string)
    if (NOT ${version_string} MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        bee_fail("Invalid semantic version string")
    endif ()

    string(REPLACE "." ";" __components__ ${version_string})
    list(GET __components__ 0 major)
    list(GET __components__ 1 minor)
    list(GET __components__ 2 patch)
endmacro()

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
    set(__bee_current_source_root ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")
endfunction()


################################################################################
#
# Initializes a bee project and all of its variables
#
################################################################################
function(bee_begin)
    set(__bee_include_dirs ${BEE_SOURCES_ROOT} ${BEE_GENERATED_ROOT} CACHE INTERNAL "")
    set(__bee_libraries "" CACHE INTERNAL "")
    set(__bee_plugins "" CACHE INTERNAL "")
    set(__bee_current_source_root "" CACHE INTERNAL "")
    bee_new_source_root()
endfunction()

function(bee_end)
    add_custom_target(All_Plugins
        ALL
        DEPENDS ${__bee_plugins}
    )

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


function(__bee_finalize_target name output_directory)
    target_include_directories(${name} PUBLIC ${__bee_include_dirs})
    __bee_set_compile_options(${name})
    set_target_properties(${name}
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${BEE_DEBUG_BINARY_DIR}/${output_directory}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${BEE_DEBUG_BINARY_DIR}/${output_directory}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BEE_DEBUG_BINARY_DIR}/${output_directory}"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${BEE_RELEASE_BINARY_DIR}/${output_directory}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${BEE_RELEASE_BINARY_DIR}/${output_directory}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BEE_RELEASE_BINARY_DIR}/${output_directory}"
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

    foreach (dep ${__bee_plugins})
        if (NOT "${dep}" STREQUAL "${name}")
            __bee_get_api_macro(${dep} api_macro)
            target_compile_definitions(${name} PRIVATE ${api_macro}=BEE_IMPORT_SYMBOL)
        endif()
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

    __bee_finalize_target(${name} "")
endfunction()


################################################################################
#
# Adds a new library target with the given name, linking against the libraries
# specified in `link_libraries`
#
# OPTIONAL ARGS:
#   * `STATIC` - adds target as STATIC library otherwise it will be SHARED
#     (i.e. .dll on windows)
#   * `LINK_LIBRARIES` - a series of library names to link via
#     `target_link_libraries`
#
################################################################################
function(bee_library name)
    cmake_parse_arguments(ARGS "STATIC" "" "LINK_LIBRARIES" ${ARGN})

    __bee_get_api_macro(${name} api_macro)

    if (ARGS_STATIC OR MONOLITHIC_BUILD)
        add_library(${name} STATIC ${__bee_sources})
    else()
        add_library(${name} SHARED ${__bee_sources})
        target_compile_definitions(${name} PRIVATE BEE_DLL)
    endif ()

    target_compile_definitions(${name} PRIVATE ${api_macro}=BEE_EXPORT_SYMBOL)

    if (ARGS_LINK_LIBRARIES)
        target_link_libraries(${name} PUBLIC ${ARGS_LINK_LIBRARIES})
    endif ()

    set(__bee_libraries ${__bee_libraries} ${name} CACHE INTERNAL "")

    __bee_finalize_target(${name} "")
endfunction()

################################################################################
#
# Adds a new plugin target with the given name, linking against the libraries
# specified in `link_libraries`
#
# OPTIONAL ARGS:
#   * `LINK_LIBRARIES` - a series of library names to link via
#     `target_link_libraries`
#
################################################################################
function(bee_plugin name)
    cmake_parse_arguments(ARGS "" "" "LINK_LIBRARIES;VERSION;DESCRIPTION;DEPENDENCIES" ${ARGN})

    # Ensure required args are set
    if (NOT ARGS_VERSION)
        bee_fail("bee_plugin requires VERSION to be set to a semantic version string, i.e. 0.1.0")
    endif ()

    if (NOT ARGS_DESCRIPTION)
        bee_fail("bee_plugin requires DESCRIPTION to be set")
    endif ()

    __bee_get_api_macro(${name} api_macro)

    if (MONOLITHIC_BUILD)
        add_library(${name} STATIC ${__bee_sources})
    else()
        add_library(${name} SHARED ${__bee_sources})
        target_compile_definitions(${name} PRIVATE BEE_DLL)
    endif ()

    target_compile_definitions(${name} PRIVATE ${api_macro}=BEE_EXPORT_SYMBOL)

    if (ARGS_LINK_LIBRARIES)
        target_link_libraries(${name} PUBLIC ${ARGS_LINK_LIBRARIES})
    endif ()

    set(__bee_plugins ${__bee_plugins} ${name} CACHE INTERNAL "")

    if (WIN32)
        set(lib_path "${BEE_DEBUG_BINARY_DIR}/${output_directory}/${name}")
        add_custom_command(
                TARGET ${name} PRE_BUILD
                COMMAND ${BB_COMMAND} prepare-plugin ${lib_path}
                COMMENT "Preparing plugin ${lib_path} to enable hot reloading on Windows"
                VERBATIM
        )
    endif ()

    if (ARGS_DEPENDENCIES)
        target_compile_definitions(${name} PRIVATE BEE_TARGET_PLUGIN_HAS_DEPENDENCIES)
        set(TARGET_DEPENDENCIES)
        list(LENGTH ARGS_DEPENDENCIES count)
        MATH(EXPR end "${count} - 1")
        foreach(index RANGE 0 ${end} 2)
            MATH(EXPR version_index "${index} + 1")
            list(GET ARGS_DEPENDENCIES ${index} dep_name)
            list(GET ARGS_DEPENDENCIES ${version_index} version)
            bee_parse_version(major minor patch ${version})
            set(TARGET_DEPENDENCIES "${TARGET_DEPENDENCIES}        { \"${dep_name}\", { ${major}, ${minor}, ${patch} } },\n")

            if (TARGET ${dep_name})
                add_dependencies(${name} ${dep_name})
            endif ()
        endforeach()
    endif ()

    bee_parse_version(major minor patch ${ARGS_VERSION})
    file(RELATIVE_PATH target_relpath ${BEE_PROJECT_ROOT} ${CMAKE_CURRENT_LIST_DIR})

    set(TARGET_VERSION_MAJOR ${major})
    set(TARGET_VERSION_MINOR ${minor})
    set(TARGET_VERSION_PATCH ${patch})
    set(TARGET_NAME ${name})
    set(TARGET_DESCRIPTION ${ARGS_DESCRIPTION})
    set(TARGET_RELATIVE_ROOT ${target_relpath})

    configure_file(
        ${CMAKE_MODULE_PATH}/PluginDescriptor.hpp.in
        ${BEE_GENERATED_ROOT}/${name}/${name}.Descriptor.hpp
    )

    target_include_directories(${name} PRIVATE ${BEE_GENERATED_ROOT}/${name})
    __bee_finalize_target(${name} "Plugins")
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
        bee_add_sources(${BEE_TESTS_ROOT}/TestMain.cpp ${ARGS_SOURCES})
        bee_exe(${name} LINK_LIBRARIES ${ARGS_LINK_LIBRARIES} gtest)
        target_compile_definitions(${name} PRIVATE GTEST_BREAK_ON_FAILURE)

        __bee_set_compile_options(${name})

        gtest_add_tests(TARGET ${name} ${ARGS_SOURCES})

        bee_new_source_root()
        bee_add_sources(${cached_sources})
    endif ()
endfunction()

function(bee_relacy_test name)
    if (BUILD_TESTS)
        cmake_parse_arguments(ARGS "" "" "LINK_LIBRARIES;SOURCES" ${ARGN})

        set(cached_sources ${__bee_sources})

        bee_new_source_root()
        bee_add_sources(${ARGS_SOURCES})
        bee_exe(${name} LINK_LIBRARIES ${ARGS_LINK_LIBRARIES})
        if (${BEE_COMPILER_IS_MSVC})
            target_compile_options(
                    ${name}
                    PUBLIC
                    /wd4456
                    /wd4595
                    /wd4311
                    /wd4302
                    /wd4312
                    /wd4003
            )
        endif ()
#        target_compile_definitions(${name} PRIVATE)

        __bee_set_compile_options(${name})

        add_test(${name} ${name})

        bee_new_source_root()
        bee_add_sources(${cached_sources})
    endif ()
endfunction()

################################################################################
#
# Bee Reflect
#
################################################################################
set(bee_reflect_program ${BEE_RELEASE_BINARY_DIR}/bee-reflect)
if (WIN32)
    set(bee_reflect_program ${bee_reflect_program}.exe)
endif ()

function(bee_reflect target)
    if (DISABLE_REFLECTION)
        return()
    endif ()

    cmake_parse_arguments(ARGS "INCLUDE_NON_HEADERS;INLINE" "" "EXCLUDE" ${ARGN})

    set(output_dir ${PROJECT_SOURCE_DIR}/Build/Generated)

    get_target_property(source_list ${target} SOURCES)

    if (NOT ARGS_INCLUDE_NON_HEADERS)
        list(FILTER source_list EXCLUDE REGEX ".*\\.(cpp|cxx|c|inl)$")
    endif ()

    set(generated_extension cpp)
    if (ARGS_INLINE)
        set(generated_extension inl)
    endif ()

    set(excluded_files)
    # Excluded argument contains both files and directories - we only want files, so glob all the headers
    # in any subdirectories of any directories given to EXCLUDED
    foreach(excluded ${ARGS_EXCLUDE})
        # Glob subdirs
        if (IS_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/${excluded})
            file(GLOB_RECURSE headers "${CMAKE_CURRENT_LIST_DIR}/${excluded}/*.h" "${CMAKE_CURRENT_LIST_DIR}/${excluded}/*.hpp")
            foreach(hdr ${headers})
                # Get the relative source path to the current dir so we can parse it correctly in the next step
                file(RELATIVE_PATH relative_src ${CMAKE_CURRENT_LIST_DIR} ${hdr})
                list(APPEND excluded_files ${relative_src})
            endforeach()
        else()
            list(APPEND excluded_files ${excluded})
        endif ()
    endforeach()

    list(LENGTH source_list source_list_length)

    if (${source_list_length} LESS_EQUAL 0)
        return()
    endif()

    set(reflected_sources)
    set(expected_output)
    foreach(src ${source_list})
        # Check if the relative path from the current dir to the file is excluded
        file(RELATIVE_PATH relative_src ${CMAKE_CURRENT_LIST_DIR} ${src})
        if (NOT "${relative_src}" IN_LIST excluded_files)
            # Then, as a real quick way of seeing if the file will output reflection data, check if there's
            # any presences of the BEE_REFLECT macro - this will slurp up #defines etc. as well which is
            # okay because bee-reflect will just output stubs for any files that don't generate reflection
            file(READ ${src} contents)
            string(FIND "${contents}" "BEE_REFLECT" position)

            if (position GREATER_EQUAL 0)
                # All good - we can include this file in the reflection generation
                get_filename_component(filename ${src} NAME_WE)
                list(APPEND expected_output "${output_dir}/${target}/${filename}.generated.${generated_extension}")
                list(APPEND reflected_sources "${src}")
            endif ()
        endif ()
    endforeach()

    list(LENGTH reflected_sources reflected_sources_length)
    if (${reflected_sources_length} LESS_EQUAL 0)
        return()
    endif ()

    # Now we build the bee-reflect command.
    # Add all the system and regular include dirs
    set(include_dirs)
    set(system_include_dirs)
    foreach(dir ${__bee_include_dirs})
        if (dir MATCHES "${BEE_THIRD_PARTY}.*")
            list(APPEND system_include_dirs -isystem"${dir}")
        else()
            list(APPEND include_dirs -I"${dir}")
        endif ()
    endforeach()

    list(APPEND include_dirs -I"${__bee_current_source_root}")

    # bee-reflect needs to explicitly tell clang where the compilers implicit include directories are located
    # because we need to use the same standard types as is being compiled by the project, not just rely on llvm's
    # builtin headers
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES MSVC)
        foreach(compiler_path $ENV{INCLUDE})
            list(APPEND system_include_dirs -isystem"${compiler_path}")
        endforeach()
    endif ()

    # Get all the compiler flags
    get_target_property(compile_defines ${target} COMPILE_DEFINITIONS)
    get_directory_property(global_defines COMPILE_DEFINITIONS)
    list(APPEND compile_defines ${global_defines})

    set(defines)
    foreach(def ${compile_defines})
        # Ignore generator expressions
        if (NOT def MATCHES "\\$<.*")
            list(APPEND defines -D${def})
        endif ()
    endforeach()

    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:Debug>:BEE_DEBUG=1>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:Release>:BEE_RELEASE=1>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:RelWithDebInfo>:BEE_ENABLE_ASSERTIONS=0>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:Release>:BEE_ENABLE_ASSERTIONS=0>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:MinSizeRel>:BEE_ENABLE_ASSERTIONS=0>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:Debug>:BEE_DEBUG=1>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:Release>:BEE_RELEASE=1>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:RelWithDebInfo>:BEE_ENABLE_ASSERTIONS=0>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:Release>:BEE_ENABLE_ASSERTIONS=0>
    # -- Bee: bee-reflect: Ignoring $<$<CONFIG:MinSizeRel>:BEE_ENABLE_ASSERTIONS=0>
    set(build_type_define
        $<$<CONFIG:Debug>:-DBEE_DEBUG=1>
        $<$<CONFIG:Release>:-DBEE_RELEASE=1>
    )
    list(APPEND defines ${build_type_define})

    set(inline_opt)
    if (ARGS_INLINE)
        set(inline_opt --inline)
    endif ()

    set(bee_reflect_command ${bee_reflect_program} ${inline_opt} ${reflected_sources} --output ${output_dir}/${target} -- ${defines} ${include_dirs} ${system_include_dirs})

    add_custom_command(
            DEPENDS ${reflected_sources} ${bee_reflect_program}
            OUTPUT ${expected_output}
            COMMAND ${bee_reflect_command}
            USES_TERMINAL
            COMMENT "Running bee-reflect on header files: generating ${expected_output}"
    )

    set(generated_sources ${expected_output} "${output_dir}/${target}/TypeList.init.cpp")
    set_source_files_properties(${generated_sources} PROPERTIES GENERATED 1)
    target_sources(${target} PRIVATE ${generated_sources})
    target_include_directories(${target} PUBLIC ${output_dir}/${target})
endfunction()