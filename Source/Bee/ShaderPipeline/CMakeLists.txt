bee_new_source_root()
bee_add_sources(
        Compiler.hpp    Compiler.cpp
        Resource.inl
        Cache.hpp       Cache.cpp
        ShaderPipeline.cpp
)
add_subdirectory(Parser)
bee_plugin(Bee.ShaderPipeline LINK_LIBRARIES Bee.Core SPIRV-Reflect SPIRV-Tools)
bee_reflect(Bee.ShaderPipeline DUMP_COMMAND)
copy_third_party_binaries(DirectXShaderCompiler Bee.ShaderPipeline)
