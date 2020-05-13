bee_add_sources(
    GPU.hpp
    Mesh.hpp        Mesh.cpp
#    RenderGraph.hpp RenderGraph.cpp
)

if (VULKAN_BACKEND)
    bee_log("Vulkan GPU backend selected")
    add_subdirectory(Vulkan)
endif ()