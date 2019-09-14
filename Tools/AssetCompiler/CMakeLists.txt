bee_new_source_root()
add_subdirectory(Bee/AssetCompiler)
bee_add_include_dirs(${CMAKE_CURRENT_SOURCE_DIR})
bee_library(Bee.AssetCompiler LINK_LIBRARIES Bee.Core)

bee_new_source_root()
bee_add_sources(Main.cpp)
bee_exe(bac LINK_LIBRARIES Bee.AssetCompiler)