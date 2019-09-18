# Create a DLL that contains just Core for external tools to use
add_subdirectory(Core)

# Make the Bee library target and link with Bee.Core
bee_new_source_root()
add_subdirectory(Application)
add_subdirectory(Graphics)

bee_library(Bee.Runtime LINK_LIBRARIES Bee.Core)
target_compile_definitions(Bee.Runtime PUBLIC BEE_BUILD_EXPORT_LIBRARY)

# Make a new target for tools
bee_new_source_root()
add_subdirectory(AssetPipeline)
bee_library(Bee.Develop LINK_LIBRARIES Bee.Core)