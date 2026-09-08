if (NOT DEFINED SHADER_SOURCE_DIR OR NOT DEFINED SHADER_OUTPUT_DIR)
    message(FATAL_ERROR "Shader source and output directories are required.")
endif()

file(GLOB shader_sources "${SHADER_SOURCE_DIR}/*.slang")
file(GLOB shader_configs "${SHADER_SOURCE_DIR}/*.shadercfg")
if (NOT shader_sources OR NOT shader_configs)
    message(FATAL_ERROR "No Slang shaders or shader configs were found.")
endif()

foreach(source IN LISTS shader_sources)
    get_filename_component(name "${source}" NAME_WLE)
    if (NOT EXISTS "${SHADER_OUTPUT_DIR}/${name}.spv")
        message(FATAL_ERROR "Missing compiled shader: ${name}.spv")
    endif()
endforeach()

foreach(config IN LISTS shader_configs)
    get_filename_component(name "${config}" NAME)
    if (NOT EXISTS "${SHADER_OUTPUT_DIR}/${name}")
        message(FATAL_ERROR "Missing shader configuration: ${name}")
    endif()
endforeach()

foreach(required IN ITEMS
    Builtin.MaterialShader
    Builtin.UIShader
    Builtin.SkyboxShader)
    if (NOT EXISTS "${SHADER_OUTPUT_DIR}/${required}.vertex.spv" OR
        NOT EXISTS "${SHADER_OUTPUT_DIR}/${required}.fragment.spv" OR
        NOT EXISTS "${SHADER_OUTPUT_DIR}/${required}.shadercfg")
        message(FATAL_ERROR "Incomplete built-in shader '${required}'.")
    endif()
endforeach()

message(STATUS "Verified all Slang shader assets in ${SHADER_OUTPUT_DIR}")
