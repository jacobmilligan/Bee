bee_new_source_root()
bee_add_sources(
    Project.hpp Project.cpp Project.inl
)
bee_plugin(Bee.Project LINK_LIBRARIES Bee.Core)
bee_reflect(Bee.Project)