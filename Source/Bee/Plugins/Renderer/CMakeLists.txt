bee_new_source_root()
bee_add_sources(Renderer.hpp Renderer.cpp)
bee_plugin(Bee.Renderer LINK_LIBRARIES Bee.Runtime
    VERSION
        0.0.0
    DESCRIPTION
        "Bee renderer plugin"
)