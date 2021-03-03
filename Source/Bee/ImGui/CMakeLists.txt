bee_new_source_root()
bee_add_sources(
    Api.hpp     Api.cpp
    ImGui.hpp   ImGui.cpp
    Target.cpp
)
bee_add_compile_definitions(IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)
bee_add_compile_definitions(IMGUI_USER_CONFIG="ImConfig.hpp")
bee_plugin(Bee.ImGui LINK_LIBRARIES Bee.Core)
target_include_directories(Bee.ImGui PRIVATE ${CMAKE_CURRENT_LIST_DIR})
add_dependencies(Bee.ImGui Bee.ShaderPipeline)

if (BEE_COMPILER_IS_MSVC)
    target_compile_options(Bee.ImGui PRIVATE /wd4127)
endif ()

