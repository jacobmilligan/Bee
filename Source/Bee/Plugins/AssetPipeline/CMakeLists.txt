bee_new_source_root()
bee_add_sources(
    AssetDatabase.cpp
    AssetPipeline.hpp AssetPipeline.inl AssetPipeline.cpp
)
bee_plugin(Bee.AssetPipeline LINK_LIBRARIES Bee.Core lmdb
    VERSION
        0.0.0
    DESCRIPTION
        "Bee asset pipeline plugin"
    DEPENDENCIES
        Bee.AssetRegistry 0.0.0
)
bee_reflect(Bee.AssetPipeline INLINE)