bee_new_source_root()
bee_add_sources(Main.cpp)
bee_exe(Sandbox.GPU LINK_LIBRARIES Bee.Runtime)
bee_reflect_link(Sandbox.GPU)