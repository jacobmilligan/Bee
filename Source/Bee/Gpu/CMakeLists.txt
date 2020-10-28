bee_new_source_root()
bee_add_sources(
    Gpu.hpp Gpu.cpp
    ResourceTable.hpp
)
bee_plugin(Bee.Gpu LINK_LIBRARIES Bee.Core)
bee_reflect(Bee.Gpu)
