bee_new_source_root()
bee_add_sources(AssetRegistry.hpp AssetRegistry.cpp)
bee_plugin(Bee.AssetRegistry LINK_LIBRARIES Bee.Core
    VERSION
        0.0.0
    DESCRIPTION
        "Bee asset registry plugin"
)
bee_reflect(Bee.AssetRegistry)