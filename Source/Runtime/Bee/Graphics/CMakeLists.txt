bee_add_sources(
    Command.hpp     Command.cpp
    GPU.hpp
    GPULimits.hpp
#    BSC.hpp         BSC.cpp
    Mesh.hpp        Mesh.cpp
    RenderGraph.hpp RenderGraph.cpp
#    Shader.hpp
)

if (VULKAN_BACKEND)
    bee_log("Vulkan GPU backend selected")
    add_subdirectory(Vulkan)
endif ()