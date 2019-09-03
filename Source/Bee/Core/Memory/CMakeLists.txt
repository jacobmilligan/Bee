bee_add_sources(
        Memory.hpp
        StackAllocator.hpp
        VMAllocator.hpp

        Allocator.hpp
        MemoryTracker.hpp               MemoryTracker.cpp
        MallocAllocator.hpp             MallocAllocator.cpp
        PoolAllocator.hpp               PoolAllocator.cpp
        VariableSizedPoolAllocator.hpp  VariableSizedPoolAllocator.cpp

        GlobalMemory.cpp
)

source_group("tlsf sources" FILES ${BEE_THIRD_PARTY}/tlsf/tlsf.h  ${bee_third_party}/tlsf/tlsf.c)

if (APPLE)
    add_subdirectory(Apple)
elseif(WIN32)
    add_subdirectory(Win32)
endif ()