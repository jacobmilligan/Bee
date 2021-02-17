bee_new_source_root()
bee_add_sources(
    App.hpp App.cpp
)
bee_plugin(Bee.Editor LINK_LIBRARIES Bee.Core)

bee_new_source_root()
bee_add_sources(Main.cpp)
bee_exe(Bee LINK_LIBRARIES Bee.Core)
