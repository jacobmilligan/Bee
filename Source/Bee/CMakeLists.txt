bee_new_source_root()

add_subdirectory(Core)

set(bee_link_libraries xxHash)

if (WIN32)
    set(bee_link_libraries ${bee_link_libraries} winmm.lib ws2_32)
endif ()

bee_library(Bee LINK_LIBRARIES ${bee_link_libraries})
target_compile_definitions(Bee PUBLIC BEE_BUILD_EXPORT_LIBRARY)


# Tests library
bee_new_source_root()
bee_add_sources(TestMain.cpp)
bee_library(Bee.TestMain LINK_LIBRARIES gtest)