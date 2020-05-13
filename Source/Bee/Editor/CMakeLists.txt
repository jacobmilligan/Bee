bee_new_source_root()

bee_add_sources(
    EditorApp.hpp EditorApp.cpp
)

bee_plugin(Bee.Editor LINK_LIBRARIES Bee.Runtime)
bee_reflect(Bee.Editor INLINE)
target_include_directories(Bee.Editor PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
