bee_new_source_root()
add_subdirectory(Bee/ShaderCompiler)
bee_add_include_dirs(${CMAKE_CURRENT_SOURCE_DIR})
bee_library(Bee.ShaderCompiler LINK_LIBRARIES Bee.Core)
copy_dxc_binaries(Bee.ShaderCompiler)

bee_new_source_root()
bee_add_sources(ServerMain.cpp)
bee_exe(BSCServer LINK_LIBRARIES Bee.ShaderCompiler)

bee_new_source_root()
bee_add_sources(CLI.cpp)
bee_exe(BSC LINK_LIBRARIES Bee.ShaderCompiler)