project (GLTFSample_DX12)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common.cmake)

set(sources 
    GLTFSample.cpp
    GLTFSample.h
    SampleRenderer.cpp
    SampleRenderer.h
    stdafx.cpp
    stdafx.h)
    
source_group("Sources"            FILES ${sources})    
    
add_executable(${PROJECT_NAME} WIN32 ${sources}) 
target_link_libraries (${PROJECT_NAME} LINK_PUBLIC Framework_DX12 ImGUI amd_ags DXC d3dcompiler D3D12)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

addManifest(${PROJECT_NAME})

