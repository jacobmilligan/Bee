bee_new_source_root()
bee_add_sources(
    MainV2.cpp
    TestAssetTypes.hpp
)
bee_exe(Sandbox.Cubes GUI LINK_LIBRARIES Bee.Runtime Bee.Develop)
bee_reflect(Sandbox.Cubes)
bee_reflect_link(Sandbox.Cubes)
target_include_directories(Sandbox.Cubes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
