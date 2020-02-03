bee_test(Bee.Tests.Runtime
    LINK_LIBRARIES

    Bee.Runtime

    SOURCES

    TestComponents.hpp
    EntityTests.cpp
)

if (BUILD_TESTS)
    bee_reflect(Bee.Tests.Runtime)
    bee_reflect_link(Bee.Tests.Runtime)
endif ()