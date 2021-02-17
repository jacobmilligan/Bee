bee_new_source_root()
bee_add_sources(
    Main.cpp
)
bee_exe(Bee.Tests.Render LINK_LIBRARIES Bee.Core)
add_dependencies(Bee.Tests.Render Bee.Gpu Bee.VulkanBackend Bee.ShaderPipeline Bee.Platform)