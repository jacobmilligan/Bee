bee_new_source_root()
bee_add_sources(Platform.hpp)

if (WIN32)
    add_subdirectory(Win32)
endif ()

bee_plugin(Bee.Platform LINK_LIBRARIES Bee.Core)
bee_reflect(Bee.Platform)