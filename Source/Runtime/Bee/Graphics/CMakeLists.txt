bee_add_sources(
    GPU.hpp
    GPULimits.hpp
    BSC.hpp         BSC.cpp
    Mesh.hpp        Mesh.cpp
)

if (VULKAN_BACKEND)
    bee_log("Vulkan GPU backend selected")
    add_subdirectory(Vulkan)
endif ()