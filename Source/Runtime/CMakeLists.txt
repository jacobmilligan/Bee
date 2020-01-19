bee_new_source_root()
#add_subdirectory(Asset)
add_subdirectory(Bee/AssetV2)
add_subdirectory(Bee/Application)
add_subdirectory(Bee/Graphics)
add_subdirectory(Bee/Entity)

bee_library(Bee.Runtime LINK_LIBRARIES Bee.Core)
target_compile_definitions(Bee.Runtime PUBLIC BEE_BUILD_EXPORT_LIBRARY)

add_subdirectory(Tests)