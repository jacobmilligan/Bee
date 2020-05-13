add_subdirectory(Core)

# Bee.Runtime
bee_new_source_root()
add_subdirectory(Application)
add_subdirectory(Graphics)
#add_subdirectory(Bee/Entity)

bee_library(Bee.Runtime LINK_LIBRARIES Bee.Core)
bee_reflect(Bee.Runtime)

# Non-framework subdirs must come after Core, Runtime & Develop
add_subdirectory(Engine)
add_subdirectory(Plugins)
add_subdirectory(Editor)

bee_new_source_root()
bee_add_sources(Bee.cpp)
bee_exe(Bee LINK_LIBRARIES Bee.Core Bee.Runtime)
add_dependencies(Bee ${__bee_plugins})