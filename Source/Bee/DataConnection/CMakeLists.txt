bee_new_source_root()
bee_add_sources(
        DataConnection.hpp DataConnection.cpp
)
bee_plugin(Bee.DataConnection LINK_LIBRARIES Bee.Core)