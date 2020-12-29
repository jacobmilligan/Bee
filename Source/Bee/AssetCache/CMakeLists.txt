bee_new_source_root()
bee_add_sources(
        AssetCache.hpp AssetCache.cpp
)
bee_plugin(Bee.AssetCache LINK_LIBRARIES Bee.Core)