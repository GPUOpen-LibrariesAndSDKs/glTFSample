project(${PROJECT_NAME})
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common.cmake)

add_compile_options(/MP)

set(sources
    GLTFSample.cpp
    GLTFSample.h
    SampleRenderer.cpp
    SampleRenderer.h
    stdafx.cpp
    stdafx.h) 

set(config
	${CMAKE_CURRENT_SOURCE_DIR}/../Common/GLTFSample.json
)


copyCommand("${config}" ${CMAKE_HOME_DIRECTORY}/bin)

source_group("Sources" FILES ${sources})
source_group("Config" FILES ${config})

add_executable(${PROJECT_NAME} WIN32 ${sources} ${config})
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC Cauldron_VK ImGUI Vulkan::Vulkan)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

addManifest(${PROJECT_NAME})
