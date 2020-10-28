bee_new_source_root()
bee_add_sources(
    ASTMatcher.hpp      ASTMatcher.cpp
    CodeGen.hpp         CodeGen.cpp
    Frontend.hpp        Frontend.cpp
    Storage.hpp         Storage.cpp
    Main.cpp
)
bee_exe(bee-reflect LINK_LIBRARIES Bee.Core.StaticLib clangTooling)

target_compile_options(bee-reflect PRIVATE ${bee_clang_tool_compile_options})

add_subdirectory(Tests)

add_custom_command(
        TARGET bee-reflect
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bee-reflect> ${BEE_TOOLS}/Binaries
        COMMENT "Bee: Copying bee-reflect.exe to Tools/Binaries"
)

