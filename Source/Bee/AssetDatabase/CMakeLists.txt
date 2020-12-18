bee_new_source_root()
bee_add_sources(
    AssetDatabase.hpp AssetDatabase.cpp AssetDatabase.inl
)
bee_plugin(Bee.AssetDatabase LINK_LIBRARIES Bee.Core lmdb)
bee_reflect(Bee.AssetDatabase)