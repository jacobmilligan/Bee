bee_add_sources(
    Platform.hpp    Platform.cpp
    Input.hpp       Input.cpp
    Application.hpp
)

if (WIN32)
    add_subdirectory(Win32)
endif ()