bee_new_source_root()
bee_add_sources(
    Lex.hpp     Lex.cpp
    Parse.hpp   Parse.cpp
    Compiler.hpp Compiler.cpp
    Loader.cpp
    Material.hpp Material.cpp
    Shader.hpp
    ShaderPipeline.cpp
)
bee_plugin(Bee.ShaderPipeline
    LINK_LIBRARIES Bee.Core Bee.Runtime SPIRV-Reflect SPIRV-Tools
    VERSION
        0.0.0
    DESCRIPTION
        "Bee ShaderPipeline plugin"
)
bee_reflect(Bee.ShaderPipeline INLINE)
