bee_add_sources(
        Memory.hpp

        Allocator.hpp
        MallocAllocator.hpp             MallocAllocator.cpp
        LinearAllocator.hpp             LinearAllocator.cpp
        PoolAllocator.hpp               PoolAllocator.cpp
        ThreadSafeLinearAllocator.hpp   ThreadSafeLinearAllocator.cpp
        ChunkAllocator.hpp              ChunkAllocator.cpp
        GlobalMemory.cpp
        MemoryTracker.hpp               MemoryTracker.cpp
)

source_group("tlsf sources" FILES ${BEE_THIRD_PARTY}/tlsf/tlsf.h  ${bee_third_party}/tlsf/tlsf.c)

if (APPLE)
    add_subdirectory(Apple)
elseif(WIN32)
    add_subdirectory(Win32)
endif ()