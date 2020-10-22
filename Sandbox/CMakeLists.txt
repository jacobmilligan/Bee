bee_new_source_root()
bee_add_sources(Sandbox.cpp)
bee_exe(Bee.Sandbox LINK_LIBRARIES Bee.Core)
add_dependencies(Bee.Sandbox ${__bee_plugins})