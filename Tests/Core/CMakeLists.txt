bee_test(Bee.Tests.Core
        LINK_LIBRARIES

        Bee.Core

        SOURCES

        ArrayTests.cpp
        Base64Tests.cpp
        ConcurrencyTests.cpp
        MemoryTests.cpp
        AllocatorTests.cpp
        FilesystemTests.cpp
        HashMapTests.cpp
        ResourcePoolTests.hpp ResourcePoolTests.cpp
        JSONTests.cpp
        StringTests.cpp
        FunctionalTests.cpp
        HashTests.cpp
        GUIDTests.cpp
#        SerializationTestsTypes.hpp
#        SerializationTests.cpp
#        ReflectionTests.cpp
        PathTests.cpp
        HandleTableTests.cpp
        SoATests.cpp
        IOTests.cpp
        JobsTests.cpp

        # Math tests from subdirectory
        Math/float2.cpp
        Math/float3.cpp
        Math/float4.cpp
        Math/float4x4.cpp
        Math/half.cpp
        Math/quaternion.cpp
)

bee_test(Bee.Tests.Core.ResourcePoolStressTests
        LINK_LIBRARIES Bee.Core
        SOURCES ResourcePoolStressTests.cpp
)

bee_relacy_test(Bee.Tests.WorkStealingQueue
        LINK_LIBRARIES Bee.Core
        SOURCES WorkStealingQueueTests.cpp
)

target_include_directories(Bee.Tests.Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

bee_reflect(Bee.Tests.Core)

bee_new_source_root()
bee_add_sources(ChunkAllocatorTest.cpp)
bee_exe(Bee.Tests.Core.ChunkAllocator
        LINK_LIBRARIES Bee.Core)
