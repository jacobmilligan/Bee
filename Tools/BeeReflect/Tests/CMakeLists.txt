bee_new_source_root()
bee_add_sources(
    BasicStruct.hpp BasicStruct.generated.cpp
    OtherHeader.hpp OtherHeader.generated.cpp
    Main.cpp
)
bee_exe(BeeReflectTest LINK_LIBRARIES Bee.Core)