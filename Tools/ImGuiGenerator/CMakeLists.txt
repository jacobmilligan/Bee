bee_new_source_root()
bee_add_sources(
    Generator.inl
    Main.cpp
)
bee_exe(bee-imgui-generator LINK_LIBRARIES Bee.Build)
bee_reflect(bee-imgui-generator)
bee_package_tool(bee-imgui-generator)