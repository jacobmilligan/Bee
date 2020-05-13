bee_new_source_root()
bee_add_sources(
    AssetDatabase.cpp
    AssetPipeline.hpp AssetPipeline.inl AssetPipeline.cpp
)
bee_plugin(Bee.AssetPipeline LINK_LIBRARIES Bee.Core lmdb)
bee_reflect(Bee.AssetPipeline INLINE)