bee_new_source_root()

set(imgui_root ${BEE_THIRD_PARTY}/imgui)

bee_add_sources(
    ImGui.hpp ImGui_Runtime.cpp
    # Dear Imgui sources
    ${imgui_root}/imgui.h ${imgui_root}/imgui.cpp
    ${imgui_root}/imgui_internal.h
    ${imgui_root}/imgui_draw.cpp
    ${imgui_root}/imgui_widgets.cpp
)

bee_add_include_dirs(${imgui_root})
bee_add_compile_definitions(IMGUI_USER_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/ImConfig.hpp")
bee_plugin(Bee.ImGui LINK_LIBRARIES Bee.Runtime)

bee_new_source_root()
bee_add_sources(ImGui.hpp ImGui_AssetPipeline.cpp)
bee_add_compile_definitions(BEE_IMGUI_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/Assets")
bee_plugin(Bee.ImGui.AssetPipeline LINK_LIBRARIES Bee.Develop)

