// AMD SampleVK sample code
// 
// Copyright(c) 2020 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stdafx.h"
#include <intrin.h>
#include "GLTFSample.h"

GLTFSample::GLTFSample(LPCSTR name) : FrameworkWindows(name)
{
    m_time = 0;
    m_bPlay = true;

    m_pGltfLoader = NULL;
}

//--------------------------------------------------------------------------------------
//
// OnParseCommandLine
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight)
{
    // set some default values
    *pWidth = 1920;
    *pHeight = 1080;
    m_activeScene = 0;          //load the first one by default
    m_bIsBenchmarking = false;
    m_VsyncEnabled = false;
    m_fontSize = 13.f;
    m_activeCamera = 0;

    // read globals
    auto process = [&](json jData)
    {
        *pWidth = jData.value("width", *pWidth);
        *pHeight = jData.value("height", *pHeight);
        m_fullscreenMode = jData.value("presentationMode", m_fullscreenMode);
        m_activeScene = jData.value("activeScene", m_activeScene);
        m_activeCamera = jData.value("activeCamera", m_activeCamera);
        m_isCpuValidationLayerEnabled = jData.value("CpuValidationLayerEnabled", m_isCpuValidationLayerEnabled);
        m_isGpuValidationLayerEnabled = jData.value("GpuValidationLayerEnabled", m_isGpuValidationLayerEnabled);
        m_VsyncEnabled = jData.value("vsync", m_VsyncEnabled);
        m_FreesyncHDROptionEnabled = jData.value("FreesyncHDROptionEnabled", m_FreesyncHDROptionEnabled);
        m_bIsBenchmarking = jData.value("benchmark", m_bIsBenchmarking);
        m_fontSize = jData.value("fontsize", m_fontSize);
    };

    //read json globals from commandline
    //
    try
    {
        if (strlen(lpCmdLine) > 0)
        {
            auto j3 = json::parse(lpCmdLine);
            process(j3);
        }
    }
    catch (json::parse_error)
    {
        Trace("Error parsing commandline\n");
        exit(0);
    }

    // read config file (and override values from commandline if so)
    //
    {
        std::ifstream f("GLTFSample.json");
        if (!f)
        {
            MessageBox(NULL, "Config file not found!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }

        try
        {
            f >> m_jsonConfigFile;
        }
        catch (json::parse_error)
        {
            MessageBox(NULL, "Error parsing GLTFSample.json!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }
    }

    json globals = m_jsonConfigFile["globals"];
    process(globals);

    // get the list of scenes
    for (const auto & scene : m_jsonConfigFile["scenes"])
        m_sceneNames.push_back(scene["name"]);
}

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnCreate()
{
    // Init the shader compiler
    InitDirectXCompiler();
    CreateShaderCache();

    // Create a instance of the renderer and initialize it, we need to do that for each GPU
    m_pRenderer = new Renderer();
    m_pRenderer->OnCreate(&m_device, &m_swapChain, m_fontSize);

    // init GUI (non gfx stuff)
    ImGUI_Init((void *)m_windowHwnd);
    m_UIState.Initialize();

    OnResize(true);
    OnUpdateDisplay();

    // Init Camera, looking at the origin
    m_camera.LookAt(math::Vector4(0, 0, 5, 0), math::Vector4(0, 0, 0, 0));
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnDestroy()
{
    ImGUI_Shutdown();

    m_device.GPUFlush();

    m_pRenderer->UnloadScene();
    m_pRenderer->OnDestroyWindowSizeDependentResources();
    m_pRenderer->OnDestroy();

    delete m_pRenderer;

    // shut down the shader compiler 
    DestroyShaderCache(&m_device);

    if (m_pGltfLoader)
    {
        delete m_pGltfLoader;
        m_pGltfLoader = NULL;
    }
}

//--------------------------------------------------------------------------------------
//
// OnEvent, win32 sends us events and we forward them to ImGUI
//
//--------------------------------------------------------------------------------------
bool GLTFSample::OnEvent(MSG msg)
{
    if (ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        return true;

    // handle function keys (F1, F2...) here, rest of the input is handled
    // by imGUI later in HandleInput() function
    const WPARAM& KeyPressed = msg.wParam;
    switch (msg.message)
    {
    case WM_KEYUP:
    case WM_SYSKEYUP:
        /* WINDOW TOGGLES */
        if (KeyPressed == VK_F1) m_UIState.bShowControlsWindow ^= 1;
        if (KeyPressed == VK_F2) m_UIState.bShowProfilerWindow ^= 1;
        break;
    }

    return true;
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnResize(bool resizeRender)
{
    // destroy resources (if we are not minimized)
    if (resizeRender && m_Width && m_Height && m_pRenderer)
    {
        m_pRenderer->OnDestroyWindowSizeDependentResources();
        m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
    }

    m_camera.SetFov(AMD_PI_OVER_4, m_Width, m_Height, 0.1f, 1000.0f);
}

//--------------------------------------------------------------------------------------
//
// UpdateDisplay
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnUpdateDisplay()
{
    // Destroy resources (if we are not minimized)
    if (m_pRenderer)
    {
        m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain, m_UIState.bUseMagnifier);
    }
}

//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
void GLTFSample::LoadScene(int sceneIndex)
{
    json scene = m_jsonConfigFile["scenes"][sceneIndex];

    // release everything and load the GLTF, just the light json data, the rest (textures and geometry) will be done in the main loop
    if (m_pGltfLoader != NULL)
    {
        m_pRenderer->UnloadScene();
        m_pRenderer->OnDestroyWindowSizeDependentResources();
        m_pRenderer->OnDestroy();
        m_pGltfLoader->Unload();
        m_pRenderer->OnCreate(&m_device, &m_swapChain, m_fontSize);
        m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
    }

    delete(m_pGltfLoader);
    m_pGltfLoader = new GLTFCommon();
    if (m_pGltfLoader->Load(scene["directory"], scene["filename"]) == false)
    {
        MessageBox(NULL, "The selected model couldn't be found, please check the documentation", "Cauldron Panic!", MB_ICONERROR);
        exit(0);
    }


    // Load the UI settings, and also some defaults cameras and lights, in case the GLTF has none
    {
#define LOAD(j, key, val) val = j.value(key, val)

        // global settings
        LOAD(scene, "TAA", m_UIState.bUseTAA);
        LOAD(scene, "toneMapper", m_UIState.SelectedTonemapperIndex);
        LOAD(scene, "skyDomeType", m_UIState.SelectedSkydomeTypeIndex);
        LOAD(scene, "exposure", m_UIState.Exposure);
        LOAD(scene, "iblFactor", m_UIState.IBLFactor);
        LOAD(scene, "emmisiveFactor", m_UIState.EmissiveFactor);
        LOAD(scene, "skyDomeType", m_UIState.SelectedSkydomeTypeIndex);

        // Add a default light in case there are none
        if (m_pGltfLoader->m_lights.size() == 0)
        {
            tfNode n;
            n.m_tranform.LookAt(PolarToVector(AMD_PI_OVER_2, 0.58f) * 3.5f, math::Vector4(0, 0, 0, 0));

            tfLight l;
            l.m_type = tfLight::LIGHT_SPOTLIGHT;
            l.m_intensity = scene.value("intensity", 1.0f);
            l.m_color = math::Vector4(1.0f, 1.0f, 1.0f, 0.0f);
            l.m_range = 15;
            l.m_outerConeAngle = AMD_PI_OVER_4;
            l.m_innerConeAngle = AMD_PI_OVER_4 * 0.9f;
            l.m_shadowResolution = 1024;

            m_pGltfLoader->AddLight(n, l);
        }

        // Allocate shadow information (if any)
        m_pRenderer->AllocateShadowMaps(m_pGltfLoader);

        // set default camera
        json camera = scene["camera"];
        m_activeCamera = scene.value("activeCamera", m_activeCamera);
        math::Vector4 from = GetVector(GetElementJsonArray(camera, "defaultFrom", { 0.0, 0.0, 10.0 }));
        math::Vector4 to = GetVector(GetElementJsonArray(camera, "defaultTo", { 0.0, 0.0, 0.0 }));
        m_camera.LookAt(from, to);

        // set benchmarking state if enabled 
        if (m_bIsBenchmarking)
        {
            std::string deviceName;
            std::string driverVersion;
            m_device.GetDeviceInfo(&deviceName, &driverVersion);
            BenchmarkConfig(scene["BenchmarkSettings"], m_activeCamera, m_pGltfLoader, deviceName, driverVersion);
        }

        // indicate the mainloop we started loading a GLTF and it needs to load the rest (textures and geometry)
        m_loadingScene = true;
    }
}


//--------------------------------------------------------------------------------------
//
// OnUpdate
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnUpdate()
{
    ImGuiIO& io = ImGui::GetIO();

    //If the mouse was not used by the GUI then it's for the camera
    //
    if (io.WantCaptureMouse)
    {
        io.MouseDelta.x = 0;
        io.MouseDelta.y = 0;
        io.MouseWheel = 0;
    }
    
    // Update Camera
    UpdateCamera(m_camera, io);
    if (m_UIState.bUseTAA)
    {
        static uint32_t Seed;
        m_camera.SetProjectionJitter(m_Width, m_Height, Seed);
    }
    else
        m_camera.SetProjectionJitter(0.f, 0.f);

    // Keyboard & Mouse
    HandleInput(io);

    // Animation Update
    if (m_bPlay)
        m_time += (float)m_deltaTime / 1000.0f; // animation time in seconds

    if (m_pGltfLoader)
    {
        m_pGltfLoader->SetAnimationTime(0, m_time);
        m_pGltfLoader->TransformScene(0, math::Matrix4::identity());
    }
}

void GLTFSample::HandleInput(const ImGuiIO& io)
{
    auto fnIsKeyTriggered = [&io](char key) { return io.KeysDown[key] && io.KeysDownDuration[key] == 0.0f; };

    // Handle Keyboard/Mouse input here

    /* MAGNIFIER CONTROLS */
    if (fnIsKeyTriggered('L'))                       m_UIState.ToggleMagnifierLock();
    if (fnIsKeyTriggered('M') || io.MouseClicked[2]) // middle mouse / M key toggles magnifier
    {
        m_UIState.bUseMagnifier ^= 1;
        // We need to update IMGUI's renderpass to draw to magnfier's renderpass when in hdr
        // Hence, flush GPU and update it through OnUpdateDisplay
        // Which needs to do the same thing when display mode is changed.
        m_device.GPUFlush();
        OnUpdateDisplay();
    }

    if (io.MouseClicked[1] && m_UIState.bUseMagnifier) // right mouse click
        m_UIState.ToggleMagnifierLock();
}

void GLTFSample::UpdateCamera(Camera& cam, const ImGuiIO& io)
{
    float yaw = cam.GetYaw();
    float pitch = cam.GetPitch();
    float distance = cam.GetDistance();

    cam.UpdatePreviousMatrices(); // set previous view matrix

    // Sets Camera based on UI selection (WASD, Orbit or any of the GLTF cameras)
    if ((io.KeyCtrl == false) && (io.MouseDown[0] == true))
    {
        yaw -= io.MouseDelta.x / 100.f;
        pitch += io.MouseDelta.y / 100.f;
    }

    // Choose camera movement depending on setting
    if (m_activeCamera == 0)
    {
        // If nothing has changed, don't calculate an update (we are getting micro changes in view causing bugs)
        if (!io.MouseWheel && (!io.MouseDown[0] || (!io.MouseDelta.x && !io.MouseDelta.y)))
            return;

        //  Orbiting
        distance -= (float)io.MouseWheel / 3.0f;
        distance = std::max<float>(distance, 0.1f);

        bool panning = (io.KeyCtrl == true) && (io.MouseDown[0] == true);

        cam.UpdateCameraPolar(yaw, pitch,
            panning ? -io.MouseDelta.x / 100.0f : 0.0f,
            panning ? io.MouseDelta.y / 100.0f : 0.0f,
            distance);
    }
    else if (m_activeCamera == 1)
    {
        //  WASD
        cam.UpdateCameraWASD(yaw, pitch, io.KeysDown, io.DeltaTime);
    }
    else if (m_activeCamera > 1)
    {
        // Use a camera from the GLTF
        m_pGltfLoader->GetCamera(m_activeCamera - 2, &cam);
    }
}

//--------------------------------------------------------------------------------------
//
// OnRender, updates the state from the UI, animates, transforms and renders the scene
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnRender()
{
    // Do any start of frame necessities
	BeginFrame();

    ImGUI_UpdateIO();
    ImGui::NewFrame();

    if (m_loadingScene)
    {
        // the scene loads in chuncks, that way we can show a progress bar
        static int loadingStage = 0;
        loadingStage = m_pRenderer->LoadScene(m_pGltfLoader, loadingStage);
        if (loadingStage == 0)
        {
            m_time = 0;
            m_loadingScene = false;
        }
    }
    else if (m_pGltfLoader && m_bIsBenchmarking)
    {
        // Benchmarking takes control of the time, and exits the app when the animation is done
        std::vector<TimeStamp> timeStamps = m_pRenderer->GetTimingValues();
        std::string Filename;
        m_time = BenchmarkLoop(timeStamps, &m_camera, Filename);
    }
    else
    {
        BuildUI();  // UI logic. Note that the rendering of the UI happens later.
        OnUpdate(); // Update camera, handle keyboard/mouse input
    }

    // Do Render frame using AFR
    m_pRenderer->OnRender(&m_UIState, m_camera, &m_swapChain);

	// Framework will handle Present and some other end of frame logic
	EndFrame();
}


//--------------------------------------------------------------------------------------
//
// WinMain
//
//--------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    LPCSTR Name = "SampleVK v1.4.1";

    // create new Vulkan sample
    return RunFramework(hInstance, lpCmdLine, nCmdShow, new GLTFSample(Name));
}
