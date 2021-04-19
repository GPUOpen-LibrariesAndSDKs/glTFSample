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

add_executable(GLTFSample_DX12 WIN32 ${sources} ${common} ${icon_src})
target_link_libraries(GLTFSample_DX12 LINK_PUBLIC GLTFSample_Common Cauldron_DX12 ImGUI amd_ags d3dcompiler D3D12)

set_target_properties(GLTFSample_DX12 PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin" DEBUG_POSTFIX "d")
