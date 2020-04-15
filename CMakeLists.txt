cmake_minimum_required(VERSION 3.15)
project(Bee)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)

include(${CMAKE_MODULE_PATH}/project.cmake)

bee_begin()
    add_subdirectory(ThirdParty)
    add_subdirectory(Source/Bee)
    add_subdirectory(Tools)
    add_subdirectory(Sandbox)

    if (BUILD_TESTS)
        add_subdirectory(Tests)
    endif ()
bee_end()
