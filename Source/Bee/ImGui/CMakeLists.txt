bee_new_source_root()
bee_add_sources(
    ImConfig.hpp
    ImGui.hpp ImGui.cpp
)
bee_library(Bee.ImGui LINK_LIBRARIES imgui Bee.Core)