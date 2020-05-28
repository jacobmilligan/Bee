bee_new_source_root()

bee_add_sources(
    EditorApp.hpp EditorApp.cpp
)

bee_plugin(Bee.Editor LINK_LIBRARIES Bee.Runtime
    VERSION
        0.0.0
    DESCRIPTION
        "Bee editor plugin"
    DEPENDENCIES
        Bee.AssetPipeline 0.0.0
        Bee.ImGui 0.0.0
        Bee.ShaderPipeline 0.0.0
)

bee_reflect(Bee.Editor INLINE)
target_include_directories(Bee.Editor PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
set(output_dir $<TARGET_FILE_DIR:Bee.Editor>)
add_custom_command(
    TARGET Bee.Editor
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/bee.boot ${output_dir}
    COMMENT "Copying bee.boot for Bee.Editor"
)
