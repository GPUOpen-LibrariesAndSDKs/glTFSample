include(${CMAKE_CURRENT_SOURCE_DIR}/../../common.cmake)

add_library(GLTFSample_Common INTERFACE)

set(config
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/GLTFSample.json
)

copyTargetCommand("${config}" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} copied_common_config)
add_dependencies(GLTFSample_Common copied_common_config)
