bee_add_sources(
    Main.hpp
    Platform.hpp    Platform.cpp
    AppLoop.hpp     AppLoop.cpp
)

if (WIN32)
    add_subdirectory(Win32)
endif ()