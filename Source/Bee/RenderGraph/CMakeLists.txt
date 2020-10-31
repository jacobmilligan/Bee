bee_new_source_root()
bee_add_sources(
    RenderGraph.hpp RenderGraph.cpp
)
bee_plugin(Bee.RenderGraph LINK_LIBRARIES Bee.Core)