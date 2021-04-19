set(sources
    GLTFSample.cpp
    GLTFSample.h
    Renderer.cpp
    Renderer.h
	UI.cpp
    UI.h
    stdafx.cpp
    stdafx.h
    dpiawarescaling.manifest)

source_group("Sources" FILES ${sources})
source_group("Icon"    FILES ${icon_src}) # defined in top-level CMakeLists.txt

add_executable(GLTFSample_VK WIN32 ${sources} ${icon_src})

target_link_libraries(GLTFSample_VK LINK_PUBLIC GLTFSample_Common Cauldron_VK ImGUI Vulkan::Vulkan)

set_target_properties(GLTFSample_VK PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin" DEBUG_POSTFIX "d")
