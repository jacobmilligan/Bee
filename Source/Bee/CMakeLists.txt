add_subdirectory(Core)

# Plugins
add_subdirectory(Platform)
add_subdirectory(Input)
add_subdirectory(Gpu)
add_subdirectory(VulkanBackend)
add_subdirectory(DataConnection)
add_subdirectory(AssetPipeline)
add_subdirectory(ShaderPipeline)
add_subdirectory(RenderGraph)
add_subdirectory(ImGui)
add_subdirectory(Project)
add_subdirectory(Editor)

add_dependencies(Bee ${__bee_plugins})
