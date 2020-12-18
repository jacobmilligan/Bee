bee_new_source_root()
bee_add_sources(Sandbox.hpp Sandbox.cpp)
bee_plugin(Bee.Sandbox LINK_LIBRARIES Bee.Core)
bee_reflect(Bee.Sandbox)

bee_new_source_root()
bee_add_sources(Main.cpp)
bee_exe(Bee.Sandbox.Host LINK_LIBRARIES Bee.Core)
add_dependencies(Bee.Sandbox.Host ${__bee_plugins})