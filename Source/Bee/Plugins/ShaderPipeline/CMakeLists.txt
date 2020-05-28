bee_new_source_root()
bee_add_sources(
    Lex.hpp     Lex.cpp
    Parse.hpp   Parse.cpp
    Compiler.hpp Compiler.cpp
    Loader.cpp
    Shader.hpp
    ShaderPipeline.cpp
    # BSCV4.hpp           BSCV4.cpp
#    Reflection.hpp      Reflection.cpp
)
bee_plugin(Bee.ShaderPipeline
    LINK_LIBRARIES Bee.Runtime SPIRV-Reflect SPIRV-Tools
    VERSION
        0.0.0
    DESCRIPTION
        "Bee ShaderPipeline plugin"
)
bee_reflect(Bee.ShaderPipeline INLINE)
