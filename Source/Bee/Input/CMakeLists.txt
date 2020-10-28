bee_new_source_root()
bee_add_sources(
    Input.hpp   Input.cpp
    Keyboard.hpp
)

bee_plugin(Bee.Input LINK_LIBRARIES Bee.Core)