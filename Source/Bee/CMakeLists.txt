# Create a DLL that contains just Core for external tools to use
add_subdirectory(Core)

# Make the Bee library target and link with Bee.Core
bee_new_source_root()
add_subdirectory(Application)
add_subdirectory(Graphics)

bee_library(Bee LINK_LIBRARIES Bee.Core)
target_compile_definitions(Bee PUBLIC BEE_BUILD_EXPORT_LIBRARY)