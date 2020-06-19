# glTFSample 

A simple but cute demo to show off the capabilities of the [Cauldron framework](https://github.com/GPUOpen-LibrariesAndSDKs/Cauldron).

![Screenshot](screenshot.png)

# Build Instructions

### Prerequisites

To build glTFSample, you must first install the following tools:

- [CMake 3.16](https://cmake.org/download/)
- [Visual Studio 2017](https://visualstudio.microsoft.com/downloads/)
- [Windows 10 SDK 10.0.18362.0](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
- [Vulkan SDK 1.2.131.2](https://www.lunarg.com/vulkan-sdk/)

Then follow the next steps:

1) Clone the repo with its submodules:
    ```
    > git clone https://github.com/GPUOpen-LibrariesAndSDKs/glTFSample.git --recurse-submodules
    ```

2) Generate the solutions:
    ```
    > cd glTFSample\build
    > GenerateSolutions.bat
    ```

3) Open the solutions in the VK or DX12 directories, compile and run.

