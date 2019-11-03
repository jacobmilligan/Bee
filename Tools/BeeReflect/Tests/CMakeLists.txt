set(bee_reflect_program ${BEE_RELEASE_BINARY_DIR}/bee-reflect)
if (WIN32)
    set(bee_reflect_program ${bee_reflect_program}.exe)
endif ()

function(bee_reflect target)
    set(output_dir ${PROJECT_SOURCE_DIR}/Build/DevData/Generated)

    get_target_property(source_list ${target} SOURCES)
    list(FILTER source_list EXCLUDE REGEX ".*\\.(cpp|cxx|c)$")

    set(expected_output)
    foreach(src ${source_list})
        get_filename_component(filename ${src} NAME_WE)
        list(APPEND expected_output ${output_dir}/${target}/${filename}.generated.cpp)
    endforeach()

    set(include_dirs)
    foreach(dir ${__bee_include_dirs})
        list(APPEND include_dirs -I${dir})
    endforeach()

    get_target_property(compile_defines ${target} COMPILE_DEFINITIONS)
    set(defines)
    foreach(def ${compile_defines})
        list(APPEND defines -D${def})
    endforeach()

    add_custom_command(
            PRE_BUILD
            DEPENDS ${source_list}
            OUTPUT ${expected_output}
            COMMAND ${bee_reflect_program} generate ${source_list} --output ${PROJECT_SOURCE_DIR}/Build/DevData/Generated/${target} -- ${defines} ${include_dirs}
    )

    target_sources(${target} PUBLIC ${expected_output})
endfunction()

function(bee_reflect_link target)
    set(output_dir ${PROJECT_SOURCE_DIR}/Build/DevData/Generated)
    file(GLOB_RECURSE registration_files ${output_dir} *.registration)

    add_custom_command(
            PRE_BUILD
            DEPENDS ${registration_files}
            OUTPUT ${output_dir}/Reflection.init.cpp
            COMMAND ${bee_reflect_program} link ${output_dir} --output ${output_dir}/Reflection.init.cpp --
    )

    target_sources(${target} PUBLIC ${output_dir}/Reflection.init.cpp)
endfunction()

bee_new_source_root()
bee_add_sources(
    BasicStruct.hpp
    OtherHeader.hpp
    Main.cpp
)
bee_exe(BeeReflectTest LINK_LIBRARIES Bee.Core)
bee_reflect(BeeReflectTest)
bee_reflect_link(BeeReflectTest)
target_include_directories(BeeReflectTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})