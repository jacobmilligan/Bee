# Make a new target for tools
bee_new_source_root()
#add_subdirectory(ShaderCompiler)
add_subdirectory(AssetPipeline)
bee_library(Bee.Develop LINK_LIBRARIES Bee.Core lmdb SPIRV-Reflect)
copy_dxc_binaries(Bee.Develop)
bee_reflect(Bee.Develop)