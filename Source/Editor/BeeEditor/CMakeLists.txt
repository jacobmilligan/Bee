bee_new_source_root()

bee_add_sources(
    Main.cpp
    EditorApp.hpp EditorApp.cpp
)

bee_exe(Bee.Editor LINK_LIBRARIES Bee.Core Bee.Runtime Bee.Develop)
bee_reflect(Bee.Editor)
bee_reflect_link(Bee.Editor)
set_target_properties(${Bee.Editor} PROPERTIES OUTPUT_NAME "Bee")
target_include_directories(Bee.Editor PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
