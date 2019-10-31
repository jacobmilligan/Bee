bee_new_source_root()
bee_add_sources(
    RecordFinder.hpp    RecordFinder.cpp
    CodeGen.hpp         CodeGen.cpp
    Frontend.hpp        Frontend.cpp
    FileTemplates.hpp   FileTemplates.cpp
    Main.cpp
)
bee_exe(bee-reflect LINK_LIBRARIES Bee.Core clangTooling)
target_compile_options(bee-reflect PRIVATE ${bee_clang_tool_compile_options})

add_subdirectory(Tests)
