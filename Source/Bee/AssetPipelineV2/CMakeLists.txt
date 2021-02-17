bee_new_source_root()
bee_add_sources(
    AssetDatabase.hpp AssetDatabase.cpp AssetDatabase.inl
    AssetPipeline.hpp AssetPipeline.cpp AssetPipeline.inl
    AssetImportPipeline.cpp
    AssetLoadPipeline.cpp
)
bee_plugin(Bee.AssetPipeline LINK_LIBRARIES Bee.Core lmdb)
bee_reflect(Bee.AssetPipeline)