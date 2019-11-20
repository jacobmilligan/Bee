bee_test(Bee.Tests.Core
        LINK_LIBRARIES

        Bee.Core

        SOURCES

        ArrayTests.cpp
        MemoryTests.cpp
        RingBufferTests.cpp
        AllocatorTests.cpp
        FilesystemTests.cpp
        HashMapTests.cpp
        ResourcePoolTests.cpp
        JSONTests.cpp
        StringTests.cpp
        FunctionalTests.cpp
        HashTests.cpp
        GUIDTests.cpp
        SerializationTests.cpp
        SerializationTestsTypes.hpp
        SerializationTestsV2.cpp
        ReflectionTests.cpp
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

if (BUILD_TESTS)
    bee_reflect(Bee.Tests.Core)
    bee_reflect_link(Bee.Tests.Core)
endif ()
