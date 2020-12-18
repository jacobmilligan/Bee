bee_new_source_root()
bee_add_sources(
    AssetPipeline.hpp   AssetPipeline.cpp AssetPipeline.inl
    Database.cpp
)
bee_plugin(Bee.AssetPipeline LINK_LIBRARIES Bee.Core lmdb)