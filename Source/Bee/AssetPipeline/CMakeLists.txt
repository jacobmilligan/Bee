bee_new_source_root()
bee_add_sources(
        AssetImportPipeline.hpp AssetImportPipeline.cpp AssetImportPipeline.inl
)
bee_plugin(Bee.AssetPipeline LINK_LIBRARIES Bee.Core lmdb)
bee_reflect(Bee.AssetPipeline)
add_dependencies(Bee.AssetPipeline Bee.AssetDatabase)