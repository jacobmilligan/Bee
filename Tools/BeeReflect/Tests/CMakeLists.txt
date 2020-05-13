bee_new_source_root()
bee_add_sources(
    BasicStruct.hpp
    OtherHeader.hpp
    Main.cpp
)
bee_exe(BeeReflectTest LINK_LIBRARIES Bee.Core)
bee_reflect(BeeReflectTest)
target_include_directories(BeeReflectTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})