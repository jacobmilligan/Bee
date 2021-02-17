bee_new_source_root()
bee_add_sources(
    Api.hpp     Api.cpp
    ImGui.hpp   ImGui.cpp
    ${BEE_THIRD_PARTY}/cimgui/cimgui.cpp
    ${BEE_THIRD_PARTY}/cimgui/imgui/imgui.cpp
    ${BEE_THIRD_PARTY}/cimgui/imgui/imgui_draw.cpp
    ${BEE_THIRD_PARTY}/cimgui/imgui/imgui_demo.cpp
    ${BEE_THIRD_PARTY}/cimgui/imgui/imgui_widgets.cpp
    ${BEE_THIRD_PARTY}/cimgui/imgui/imgui_tables.cpp
)
bee_add_compile_definitions(IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)
bee_add_compile_definitions(IMGUI_USER_CONFIG="ImConfig.hpp")
bee_plugin(Bee.ImGui LINK_LIBRARIES Bee.Core)
target_include_directories(Bee.ImGui PRIVATE ${CMAKE_CURRENT_LIST_DIR})

if (BEE_COMPILER_IS_MSVC)
    target_compile_options(Bee.ImGui PRIVATE /wd4127)
endif ()

