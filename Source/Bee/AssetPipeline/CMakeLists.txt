bee_new_source_root()
bee_add_sources(
    AssetPipeline.hpp AssetPipeline.cpp AssetPipeline.inl
)
bee_plugin(Bee.AssetPipeline LINK_LIBRARIES Bee.Core lmdb)
bee_reflect(Bee.AssetPipeline)
add_dependencies(Bee.AssetPipeline Bee.AssetDatabase)