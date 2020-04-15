bee_test(Bee.Tests.Runtime
    LINK_LIBRARIES

    Bee.Runtime

    SOURCES

    #EntityTests.cpp
    #RenderGraphTests.cpp
    #TestComponents.hpp
)

#    bee_reflect(Bee.Tests.Runtime)
bee_reflect_link(Bee.Tests.Runtime)