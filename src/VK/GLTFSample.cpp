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

#include "GLTFSample.h"

GLTFSample::GLTFSample(LPCSTR name) : FrameworkWindows(name)
{
    m_lastFrameTime = MillisecondsNow();
    m_time = 0;
    m_bPlay = true;

    m_pGltfLoader = NULL;
    m_currentDisplayMode = DISPLAYMODE_SDR;
}

//--------------------------------------------------------------------------------------
//
// OnParseCommandLine
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight, bool *pbFullScreen)
{
    // set some default values
    *pWidth = 1920;
    *pHeight = 1080;
    m_activeScene = 0; //load the first one by default
    *pbFullScreen = false;
    m_state.m_isBenchmarking = false;
    m_state.m_isValidationLayerEnabled = false;
    m_activeCamera = 0;

    //read globals
    auto process = [&](json jData)
    {
        *pWidth = jData.value("width", *pWidth);
        *pHeight = jData.value("height", *pHeight);
        *pbFullScreen = jData.value("fullScreen", *pbFullScreen);
        m_activeScene = jData.value("activeScene", m_activeScene);
        m_activeCamera = jData.value("activeCamera", m_activeCamera);
        m_isCpuValidationLayerEnabled = jData.value("CpuValidationLayerEnabled", m_isCpuValidationLayerEnabled);
        m_isGpuValidationLayerEnabled = jData.value("GpuValidationLayerEnabled", m_isGpuValidationLayerEnabled);
        m_state.m_isBenchmarking = jData.value("benchmark", m_state.m_isBenchmarking);
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
void GLTFSample::OnCreate(HWND hWnd)
{
    // Create Device
    //
    m_device.OnCreate("myapp", "myEngine", m_isCpuValidationLayerEnabled, m_isGpuValidationLayerEnabled, hWnd);
    m_device.CreatePipelineCache();

    //init the shader compiler
    InitDirectXCompiler();
    CreateShaderCache();

    // Create Swapchain
    //
    uint32_t dwNumberOfBackBuffers = 2;
    m_swapChain.OnCreate(&m_device, dwNumberOfBackBuffers, hWnd);

    // Create a instance of the renderer and initialize it, we need to do that for each GPU
    //
    m_Node = new SampleRenderer();
    m_Node->OnCreate(&m_device, &m_swapChain);

    // init GUI (non gfx stuff)
    //
    ImGUI_Init((void *)hWnd);

    // Init Camera, looking at the origin
    //
    m_roll = 0.0f;
    m_pitch = 0.0f;
    m_distance = 3.5f;

    // init GUI state
    m_state.toneMapper = 0;
    m_state.m_useTAA = true;
    m_state.skyDomeType = 0;
    m_state.exposure = 1.0f;
    m_state.iblFactor = 2.0f;
    m_state.emmisiveFactor = 1.0f;
    m_state.bDrawLightFrustum = false;
    m_state.bDrawBoundingBoxes = false;
    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0, 0, 0, 0));

    
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

    // Fullscreen state should always be false before exiting the app.
    m_swapChain.SetFullScreen(false);

    m_Node->UnloadScene();
    m_Node->OnDestroyWindowSizeDependentResources();
    m_Node->OnDestroy();

    delete m_Node;

    m_swapChain.OnDestroyWindowSizeDependentResources();
    m_swapChain.OnDestroy();

    //shut down the shader compiler 
    DestroyShaderCache(&m_device);

    if (m_pGltfLoader)
    {
        delete m_pGltfLoader;
        m_pGltfLoader = NULL;
    }

    m_device.DestroyPipelineCache();
    m_device.OnDestroy();
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
    return true;
}

//--------------------------------------------------------------------------------------
//
// SetFullScreen
//
//--------------------------------------------------------------------------------------
void GLTFSample::SetFullScreen(bool fullscreen)
{
    m_device.GPUFlush();

    // when going to windowed make sure we are always using SDR
    if ((fullscreen == false) && (m_currentDisplayMode != DISPLAYMODE_SDR))
        m_currentDisplayMode = DISPLAYMODE_SDR;

    m_swapChain.SetFullScreen(fullscreen);
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnResize(uint32_t width, uint32_t height, DisplayModes displayMode)
{
    if (m_Width != width || m_Height != height || m_currentDisplayMode != displayMode)
    {
        // Flush GPU
        //
        m_device.GPUFlush();

        // destroy resources (if we are not minimized)
        //
        if (m_Width > 0 && m_Height > 0)
        {
            if (m_Node != NULL)
            {
                m_Node->OnDestroyWindowSizeDependentResources();
            }
            m_swapChain.OnDestroyWindowSizeDependentResources();
        }

        m_Width = width;
        m_Height = height;
        m_currentDisplayMode = displayMode;

        // if resizing but not minimizing the recreate it with the new size
        //
        if (m_Width > 0 && m_Height > 0)
        {
            m_swapChain.OnCreateWindowSizeDependentResources(m_Width, m_Height, false, m_currentDisplayMode);
            if (m_Node != NULL)
            {
                m_Node->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
            }
        }
    }
    m_state.camera.SetFov(XM_PI / 4, m_Width, m_Height, 0.1f, 1000.0f);
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
        m_Node->UnloadScene();
        m_Node->OnDestroyWindowSizeDependentResources();
        m_Node->OnDestroy();
        m_pGltfLoader->Unload();
        m_Node->OnCreate(&m_device, &m_swapChain);
        m_Node->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
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
        LOAD(scene, "TAA", m_state.m_useTAA);
        LOAD(scene, "toneMapper", m_state.toneMapper);
        LOAD(scene, "skyDomeType", m_state.skyDomeType);
        LOAD(scene, "exposure", m_state.exposure);
        LOAD(scene, "iblFactor", m_state.iblFactor);
        LOAD(scene, "emmisiveFactor", m_state.emmisiveFactor);
        LOAD(scene, "skyDomeType", m_state.skyDomeType);

        // Add a default light in case there are none
        //
        if (m_pGltfLoader->m_lights.size() == 0)
        {
            tfNode n;
            n.m_tranform.LookAt(PolarToVector(XM_PI / 2.0f, 0.58f)*3.5f, XMVectorSet(0, 0, 0, 0));

            tfLight l;
            l.m_type = tfLight::LIGHT_SPOTLIGHT;
            l.m_intensity = scene.value("intensity", 1.0f);
            l.m_color = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
            l.m_range = 15;
            l.m_outerConeAngle = XM_PI / 4.0f;
            l.m_innerConeAngle = (XM_PI / 4.0f) * 0.9f;

            m_pGltfLoader->AddLight(n, l);
        }

        // set default camera
        //
        json camera = scene["camera"];
        m_activeCamera = scene.value("activeCamera", m_activeCamera);
        XMVECTOR from = GetVector(GetElementJsonArray(camera, "defaultFrom", { 0.0, 0.0, 10.0 }));
        XMVECTOR to = GetVector(GetElementJsonArray(camera, "defaultTo", { 0.0, 0.0, 0.0 }));
        m_state.camera.LookAt(from, to);
        m_roll = m_state.camera.GetYaw();
        m_pitch = m_state.camera.GetPitch();
        m_distance = m_state.camera.GetDistance();

        // set benchmarking state if enabled 
        //
        if (m_state.m_isBenchmarking)
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
// BuildUI, all UI code should be here
//
//--------------------------------------------------------------------------------------
void GLTFSample::BuildUI()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 700), ImGuiCond_FirstUseEver);

    bool opened = true;
    ImGui::Begin("Stats", &opened);

    if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Resolution       : %ix%i", m_Width, m_Height);
    }

    if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Play", &m_bPlay);
        ImGui::SliderFloat("Time", &m_time, 0, 30);
    }

    if (ImGui::CollapsingHeader("Model Selection", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto getterLambda = [](void* data, int idx, const char** out_str)->bool { *out_str = ((std::vector<std::string> *)data)->at(idx).c_str(); return true; };
        if (ImGui::Combo("model", &m_activeScene, getterLambda, &m_sceneNames, (int)m_sceneNames.size()) || (m_pGltfLoader == NULL))
        {
            LoadScene(m_activeScene);

            //bail out as we need to reload everything
            ImGui::End();
            ImGui::EndFrame();
            ImGui::NewFrame();
            return;
        }
    }

    ImGui::SliderFloat("exposure", &m_state.exposure, 0.0f, 4.0f);
    ImGui::SliderFloat("emissive", &m_state.emmisiveFactor, 1.0f, 1000.0f, NULL, 1.0f);
    ImGui::SliderFloat("iblFactor", &m_state.iblFactor, 0.0f, 3.0f);
    for (int i = 0; i < m_pGltfLoader->m_lights.size(); i++)
    {
        ImGui::SliderFloat(format("Light %i Intensity", i).c_str(), &m_pGltfLoader->m_lights[i].m_intensity, 0.0f, 50.0f);
    }

    const char * tonemappers[] = { "Timothy", "DX11DSK", "Reinhard", "Uncharted2Tonemap", "ACES", "No tonemapper" };
    ImGui::Combo("tone mapper", &m_state.toneMapper, tonemappers, _countof(tonemappers));

    const char * skyDomeType[] = { "Procedural Sky", "cubemap", "Simple clear" };
    ImGui::Combo("SkyDome", &m_state.skyDomeType, skyDomeType, _countof(skyDomeType));

    char *cameraControl[] = { "Orbit", "WASD", "cam #0", "cam #1", "cam #2", "cam #3" , "cam #4", "cam #5" };
    if (m_activeCamera >= m_pGltfLoader->m_cameras.size() + 2)
        m_activeCamera = 0;
    ImGui::Combo("Camera", &m_activeCamera, cameraControl, min((int)(m_pGltfLoader->m_cameras.size() + 2), _countof(cameraControl)));

    ImGui::Checkbox("TAA", &m_state.m_useTAA);
    ImGui::Checkbox("Show Bounding Boxes", &m_state.bDrawBoundingBoxes);
    ImGui::Checkbox("Show Light Frustum", &m_state.bDrawLightFrustum);

    if (ImGui::Button("Set spotlight 0"))
    {
        int idx = m_pGltfLoader->m_lightInstances[0].m_nodeIndex;
        m_pGltfLoader->m_nodes[idx].m_tranform.LookAt(m_state.camera.GetPosition(), m_state.camera.GetPosition() - m_state.camera.GetDirection());
        m_pGltfLoader->m_animatedMats[idx] = m_pGltfLoader->m_nodes[idx].m_tranform.GetWorldMat();
    }

    // FreeSync HDR display mode selector
    // 
    if (ImGui::Button("FreeSync HDR"))
    {
        ImGui::OpenPopup("FreeSync HDR");
        m_swapChain.EnumerateDisplayModes(&m_displayModesAvailable, &m_displayModesNamesAvailable);
    }

    if (ImGui::BeginPopupModal("FreeSync HDR", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (m_displayModesAvailable.size() == 1)
        {
            ImGui::Text("\nOpps! This window is not on a FreeSync HDR monitor so the only available mode is SDR.\n\n");
            ImGui::Text("If you have a FreeSync HDR monitor move this window to that monitor and try again\n\n");
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        else
        {
            if (m_swapChain.IsFullScreen() == false)
            {
                ImGui::Text("\nFreeSync HDR modes are only available in in fullscreen mode, please press ALT + ENTER for fun!\n\n");
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
            else
            {
                ImGui::Text("\nChoose video mode\n\n");

                static DisplayModes selectedDisplayModeIdx;
                for (int i = 0; i < m_displayModesAvailable.size(); i++)
                {
                    ImGui::RadioButton(m_displayModesNamesAvailable[i], (int*)&selectedDisplayModeIdx, m_displayModesAvailable[i]);
                }
                ImGui::Separator();

                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    OnResize(m_Width, m_Height, selectedDisplayModeIdx);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::SetItemDefaultFocus();
                ImGui::EndPopup();
            }
        }
    }

    if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::vector<TimeStamp> timeStamps = m_Node->GetTimingValues();
        if (timeStamps.size() > 0)
        {
            for (uint32_t i = 0; i < timeStamps.size(); i++)
            {
                ImGui::Text("%-22s: %7.1f", timeStamps[i].m_label.c_str(), timeStamps[i].m_microseconds);
            }

            //scrolling data and average computing
            static float values[128];
            values[127] = timeStamps.back().m_microseconds;
            for (uint32_t i = 0; i < 128 - 1; i++) { values[i] = values[i + 1]; }
            ImGui::PlotLines("", values, 128, 0, "GPU frame time (us)", 0.0f, 30000.0f, ImVec2(0, 80));
        }
    }

#ifdef USE_VMA
    if (ImGui::Button("Save VMA json"))
    {
        char *pJson;
        vmaBuildStatsString(m_device.GetAllocator(), &pJson, VK_TRUE);

        static char filename[256];
        time_t now = time(NULL);
        tm buf;
        localtime_s(&buf, &now);
        strftime(filename, sizeof(filename), "VMA_%Y%m%d_%H%M%S.json", &buf);
        std::ofstream ofs(filename, std::ofstream::out);
        ofs << pJson;
        ofs.close();
        vmaFreeStatsString(m_device.GetAllocator(), pJson);
    }
#endif        

    ImGui::End();

    // Sets Camera based on UI selection (WASD, Orbit or any of the GLTF cameras)
    //
    ImGuiIO& io = ImGui::GetIO();
    {
        //If the mouse was not used by the GUI then it's for the camera
        //
        if (io.WantCaptureMouse)
        {
            io.MouseDelta.x = 0;
            io.MouseDelta.y = 0;
            io.MouseWheel = 0;
        }
        else if ((io.KeyCtrl == false) && (io.MouseDown[0] == true))
        {
            m_roll -= io.MouseDelta.x / 100.f;
            m_pitch += io.MouseDelta.y / 100.f;
        }

        // Choose camera movement depending on setting
        //
        if (m_activeCamera == 0)
        {
            //  Orbiting                
            //
            m_distance -= (float)io.MouseWheel / 3.0f;
            m_distance = std::max<float>(m_distance, 0.1f);

            bool panning = (io.KeyCtrl == true) && (io.MouseDown[0] == true);

            m_state.camera.UpdateCameraPolar(m_roll, m_pitch, panning ? -io.MouseDelta.x / 100.0f : 0.0f, panning ? io.MouseDelta.y / 100.0f : 0.0f, m_distance);
        }
        else if (m_activeCamera == 1)
        {
            //  WASD
            //
            m_state.camera.UpdateCameraWASD(m_roll, m_pitch, io.KeysDown, io.DeltaTime);
        }
        else if (m_activeCamera > 1)
        {
            // Use a camera from the GLTF
            // 
            m_pGltfLoader->GetCamera(m_activeCamera - 2, &m_state.camera);
            m_roll = m_state.camera.GetYaw();
            m_pitch = m_state.camera.GetPitch();
        }
    }
}

//--------------------------------------------------------------------------------------
//
// OnRender, updates the state from the UI, animates, transforms and renders the scene
//
//--------------------------------------------------------------------------------------
void GLTFSample::OnRender()
{
    // Get timings
    //
    double timeNow = MillisecondsNow();
    float deltaTime = (float)(timeNow - m_lastFrameTime);
    m_lastFrameTime = timeNow;

    ImGUI_UpdateIO();
    ImGui::NewFrame();

    if (m_loadingScene)
    {
        // the scene loads in chuncks, that way we can show a progress bar
        static int loadingStage = 0;
        loadingStage = m_Node->LoadScene(m_pGltfLoader, loadingStage);
        if (loadingStage == 0)
        {
            m_time = 0;
            m_loadingScene = false;
        }
    }
    else if (m_pGltfLoader && m_state.m_isBenchmarking)
    {
        // benchmarking takes control of the time, and exits the app when the animation is done
        std::vector<TimeStamp> timeStamps = m_Node->GetTimingValues();

        const std::string *pFilename;
        m_time = BenchmarkLoop(timeStamps, &m_state.camera, &pFilename);
    }
    else
    {
        // Build the UI. Note that the rendering of the UI happens later.
        BuildUI();

        // Set animation time
        //
        if (m_bPlay)
        {
            m_time += (float)deltaTime / 1000.0f;
        }
    }


    // Animate and transform the scene
    //
    if (m_pGltfLoader)
    {
        m_pGltfLoader->SetAnimationTime(0, m_time);
        m_pGltfLoader->TransformScene(0, XMMatrixIdentity());
    }

    m_state.time = m_time;

    // Do Render frame using AFR 
    //
    m_Node->OnRender(&m_state, &m_swapChain);

    m_swapChain.Present();
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
    LPCSTR Name = "SampleVK v1.3";

    // create new Vulkan sample
    return RunFramework(hInstance, lpCmdLine, nCmdShow, new GLTFSample(Name));
}

