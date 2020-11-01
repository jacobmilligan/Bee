bee_new_source_root()

bee_add_sources(
        Atomic.hpp
        Bit.hpp
        CLI.hpp             CLI.cpp
        Concurrency.hpp     Concurrency.cpp
        Config.hpp
        declval.hpp
        DynamicLibrary.hpp
        Debug.hpp           Debug.cpp
        Enum.hpp
        Error.hpp           Error.cpp
        Filesystem.hpp      Filesystem.cpp
        Forward.hpp
        Functional.hpp
        GUID.hpp            GUID.cpp
        Handle.hpp
        Hash.hpp            Hash.cpp
        IO.hpp              IO.cpp      IO.inl
        Logger.hpp          Logger.cpp
        Main.hpp
        TypeTraits.hpp
        Plugin.hpp          Plugin.cpp
        New.hpp
        Noncopyable.hpp
        NumericTypes.hpp
        Path.hpp            Path.cpp
        Process.hpp
        Random.hpp          Random.cpp
        Reflection.hpp      Reflection.cpp
        Span.hpp
        Socket.hpp
        String.hpp          String.cpp
        Thread.hpp          Thread.cpp
        Time.hpp            Time.cpp
)

add_subdirectory(Containers)
add_subdirectory(Memory)
add_subdirectory(Math)
add_subdirectory(JSON)
add_subdirectory(Serialization)
add_subdirectory(Jobs)

if (WIN32)
    add_subdirectory(Win32)
endif ()

set(core_link_libraries xxHash)

if (WIN32)
    set(core_link_libraries ${core_link_libraries} winmm.lib ws2_32)
endif ()

bee_library(Bee.Core KEEP_SOURCE_ROOT LINK_LIBRARIES ${core_link_libraries})
bee_library(Bee.Core.StaticLib STATIC LINK_LIBRARIES ${core_link_libraries})

target_compile_definitions(Bee.Core PRIVATE BEE_BUILD_CORE_EXPORT_LIBRARY)
bee_reflect(
    Bee.Core

    EXCLUDE

    Serialization
    Win32
    macOS
    Memory
    Jobs
    Config.hpp
    AtomicCounter.hpp
    Thread.hpp
    Process.hpp
    IO.hpp
        TypeTraits.hpp
    DynamicLibrary.hpp
    declval.hpp
    Error.hpp
    Bitset.hpp
    Enum.hpp
    EnumStruct.hpp
)
