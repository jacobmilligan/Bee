bee_new_source_root()
bee_add_sources(
    ImGui.hpp ImGui.cpp
)
bee_add_compile_definitions(IMGUI_USER_CONFIG="ImConfig.hpp")
bee_plugin(Bee.ImGui LINK_LIBRARIES cimgui Bee.Core)

