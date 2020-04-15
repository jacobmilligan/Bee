bee_new_source_root()
bee_add_sources(AssetRegistry.hpp AssetRegistry.cpp)
bee_plugin(Bee.AssetRegistry LINK_LIBRARIES Bee.Core)