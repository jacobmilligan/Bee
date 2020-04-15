bee_new_source_root()

bee_add_sources(
    Engine.hpp Engine.cpp
)

bee_library(Bee.Engine LINK_LIBRARIES Bee.Core Bee.Runtime)
bee_reflect(Bee.Engine)
bee_reflect_link(Bee.Engine)
target_include_directories(Bee.Engine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)