bee_new_source_root()
bee_add_sources(
    Environment.hpp Environment.cpp
)
bee_library(Bee.Build STATIC LINK_LIBRARIES Bee.Core)

bee_new_source_root()
bee_add_sources(Main.cpp)
bee_exe(bb LINK_LIBRARIES Bee.Build)
bee_package_tool(bb)