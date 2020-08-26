// AMD SampleDX12 sample code
// 
// Copyright(c) 2018 Advanced Micro Devices, Inc.All rights reserved.
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

#include "SampleRenderer.h"
#include "base\\SaveTexture.h"


//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnCreate(Device* pDevice, SwapChain *pSwapChain)
{
    m_pDevice = pDevice;

    // Initialize helpers

    // Create all the heaps for the resources views
    const uint32_t cbvDescriptorCount = 4000;
    const uint32_t srvDescriptorCount = 8000;
    const uint32_t uavDescriptorCount = 10;
    const uint32_t dsvDescriptorCount = 10;
    const uint32_t rtvDescriptorCount = 60;
    const uint32_t samplerDescriptorCount = 20;
    m_resourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, dsvDescriptorCount, rtvDescriptorCount, samplerDescriptorCount);

    // Create a commandlist ring for the Direct queue
    uint32_t commandListsPerBackBuffer = 8;
    m_CommandListRing.OnCreate(pDevice, backBufferCount, commandListsPerBackBuffer, pDevice->GetGraphicsQueue()->GetDesc());

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 200 * 1024 * 1024;
    m_ConstantBufferRing.OnCreate(pDevice, backBufferCount, constantBuffersMemSize, &m_resourceViewHeaps);

    // Create a 'static' pool for vertices, indices and constant buffers
    const uint32_t staticGeometryMemSize = (5 * 128) * 1024 * 1024;
    m_VidMemBufferPool.OnCreate(pDevice, staticGeometryMemSize, USE_VID_MEM, "StaticGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, backBufferCount);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    // for 4K textures we'll need 100Megs
    const uint32_t uploadHeapMemSize = 1000 * 1024 * 1024;
    m_UploadHeap.OnCreate(pDevice, uploadHeapMemSize);    // initialize an upload heap (uses suballocation for faster results)

    // Create the depth buffer views
    m_resourceViewHeaps.AllocDSVDescriptor(1, &m_depthBufferDSV);

#if USE_SHADOWMASK    
    m_shadowResolve.OnCreate(m_pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);

    // Create the shadow mask descriptors
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_ShadowMaskUAV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_ShadowMaskSRV);
#endif

    // Create a Shadowmap atlas to hold 4 cascades/spotlights
    m_shadowMap.InitDepthStencil(pDevice, "m_pShadowMap", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, 2 * 1024, 2 * 1024, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
    m_resourceViewHeaps.AllocDSVDescriptor(1, &m_ShadowMapDSV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_ShadowMapSRV);
    m_shadowMap.CreateDSV(0, &m_ShadowMapDSV);
    m_shadowMap.CreateSRV(0, &m_ShadowMapSRV);

    m_skyDome.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, "..\\media\\cauldron-media\\envmaps\\papermill\\diffuse.dds", "..\\media\\cauldron-media\\envmaps\\papermill\\specular.dds", DXGI_FORMAT_R16G16B16A16_FLOAT, 4);
    m_skyDomeProc.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT, 4);
    m_wireframe.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT, 4);
    m_wireframeBox.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool);
    m_downSample.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_bloom.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_sharpen.OnCreate(pDevice, &m_resourceViewHeaps, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_taa.OnCreate(pDevice, &m_resourceViewHeaps);

    // Create tonemapping pass
    m_toneMappingPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());
    m_toneMappingCS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);
    m_colorConversionPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, pSwapChain->GetFormat());

    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_HDRRTV);
    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_HDRRTVMSAA);

    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_HDRSRV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_HDRUAV);

    // motion vectors views
    m_resourceViewHeaps.AllocDSVDescriptor(1, &m_MotionVectorsDepthMapDSV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_MotionVectorsDepthMapSRV);

    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_MotionVectorsRTV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_MotionVectorsSRV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(2, &m_MotionVectorsInputsSRV);
    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_NormalBufferRTV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_NormalBufferSRV);

    // TAA views
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_TAABufferSRV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_TAABufferUAV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(4, &m_TAAInputsSRV);

    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_HistoryBufferRTV);

    // Make sure upload heap has finished uploading before continuing
#if (USE_VID_MEM==true)
    m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    m_UploadHeap.FlushAndFinish();
#endif
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnDestroy()
{
    m_ImGUI.OnDestroy();
    m_colorConversionPS.OnDestroy();
    m_toneMappingCS.OnDestroy();
    m_toneMappingPS.OnDestroy();
    m_taa.OnDestroy();
    m_sharpen.OnDestroy();
    m_bloom.OnDestroy();
    m_downSample.OnDestroy();
    m_wireframeBox.OnDestroy();
    m_wireframe.OnDestroy();
    m_skyDomeProc.OnDestroy();
    m_skyDome.OnDestroy();
    m_shadowMap.OnDestroy();
#if USE_SHADOWMASK
    m_shadowResolve.OnDestroy();
#endif

    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
    m_ConstantBufferRing.OnDestroy();
    m_resourceViewHeaps.OnDestroy();
    m_CommandListRing.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnCreateWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height)
{
    m_Width = Width;
    m_Height = Height;

    // Set the viewport
    //
    m_viewport = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };

    // Create scissor rectangle
    //
    m_rectScissor = { 0, 0, (LONG)Width, (LONG)Height };

    // Create depth buffer
    //
    m_depthBuffer.InitDepthStencil(m_pDevice, "depthbuffer", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, Width, Height, 1, 1, 4, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));
    m_depthBuffer.CreateDSV(0, &m_depthBufferDSV);

#if USE_SHADOWMASK
    // Create shadow mask
    //
    m_ShadowMask.Init(m_pDevice, "shadowbuffer", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL);
    m_ShadowMask.CreateUAV(0, &m_ShadowMaskUAV);
    m_ShadowMask.CreateSRV(0, &m_ShadowMaskSRV);
#endif

    // Create Texture + RTV with x4 MSAA
    //
    CD3DX12_RESOURCE_DESC RDescMSAA = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, Width, Height, 1, 1, 4, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    m_HDRMSAA.InitRenderTarget(m_pDevice, "HDRMSAA", &RDescMSAA, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_HDRMSAA.CreateRTV(0, &m_HDRRTVMSAA);

    // Create Texture + RTV, to hold the resolved scene 
    //
    CD3DX12_RESOURCE_DESC RDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_HDR.InitRenderTarget(m_pDevice, "HDR", &RDesc, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_HDR.CreateSRV(0, &m_HDRSRV);
    m_HDR.CreateUAV(0, &m_HDRUAV);  
    m_HDR.CreateRTV(0, &m_HDRRTV);

    // Create MotionVectorPass's GBuffer 
    //
    m_MotionVectorsDepthMap.InitDepthStencil(m_pDevice, "m_MotionVectorDepthMap", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
    m_MotionVectorsDepthMap.CreateDSV(0, &m_MotionVectorsDepthMapDSV);
    m_MotionVectorsDepthMap.CreateSRV(0, &m_MotionVectorsDepthMapSRV);

    m_NormalBuffer.InitRenderTarget(m_pDevice, "m_NormalBuffer", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));
    m_NormalBuffer.CreateRTV(0, &m_NormalBufferRTV);
    m_NormalBuffer.CreateSRV(0, &m_NormalBufferSRV);

    m_MotionVectors.InitRenderTarget(m_pDevice, "m_MotionVector", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16_FLOAT, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));
    m_MotionVectors.CreateRTV(0, &m_MotionVectorsRTV);
    m_MotionVectors.CreateSRV(0, &m_MotionVectorsSRV);

    // TAA buffers
    //
    CD3DX12_RESOURCE_DESC TAADesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_TAABuffer.Init(m_pDevice, "m_TAABuffer", &TAADesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL);
    m_TAABuffer.CreateSRV(0, &m_TAABufferSRV);
    m_TAABuffer.CreateUAV(0, &m_TAABufferUAV);

    CD3DX12_RESOURCE_DESC HistoryDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    m_HistoryBuffer.InitRenderTarget(m_pDevice, "m_HistoryBuffer", &HistoryDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_HistoryBuffer.CreateRTV(0, &m_HistoryBufferRTV);

    m_HDR.CreateSRV(0, &m_TAAInputsSRV);
    m_MotionVectorsDepthMap.CreateSRV(1, &m_TAAInputsSRV);
    m_HistoryBuffer.CreateSRV(2, &m_TAAInputsSRV);
    m_MotionVectors.CreateSRV(3, &m_TAAInputsSRV);

    // update bloom and downscaling effect
    //
    m_downSample.OnCreateWindowSizeDependentResources(m_Width, m_Height, &m_HDR, 5); //downsample the HDR texture 5 times
    m_bloom.OnCreateWindowSizeDependentResources(m_Width / 2, m_Height / 2, m_downSample.GetTexture(), 5, &m_HDR);

    // Update pipelines in case the format of the RTs changed (this happens when going HDR)
    //
    m_sharpen.UpdatePipelines(DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_colorConversionPS.UpdatePipelines(pSwapChain->GetFormat(), pSwapChain->GetDisplayMode());
    m_toneMappingPS.UpdatePipelines(pSwapChain->GetFormat());
    m_ImGUI.UpdatePipeline((pSwapChain->GetDisplayMode() == DISPLAYMODE_SDR) ? pSwapChain->GetFormat() : m_HDR.GetFormat());
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnDestroyWindowSizeDependentResources()
{
    m_bloom.OnDestroyWindowSizeDependentResources();
    m_downSample.OnDestroyWindowSizeDependentResources();

    // MotionVectorPass's GBuffer 
    //
    m_MotionVectors.OnDestroy();
    m_NormalBuffer.OnDestroy();
    m_MotionVectorsDepthMap.OnDestroy();

    m_HDR.OnDestroy();
    m_HDRMSAA.OnDestroy();

    // TAA buffers
    //
    m_HistoryBuffer.OnDestroy();
    m_TAABuffer.OnDestroy();

#if USE_SHADOWMASK
    m_ShadowMask.OnDestroy();
#endif
    m_depthBuffer.OnDestroy();
}


//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
int SampleRenderer::LoadScene(GLTFCommon *pGLTFCommon, int stage)
{
    // show loading progress
    //
    ImGui::OpenPopup("Loading");
    if (ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        float progress = (float)stage / 13.0f;
        ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), NULL);
        ImGui::EndPopup();
    }

    // use multithreading
    AsyncPool *pAsyncPool = &m_asyncPool;

    // Loading stages
    //
    if (stage == 0)
    {
    }
    else if (stage == 5)
    {
        Profile p("m_pGltfLoader->Load");

        m_pGLTFTexturesAndBuffers = new GLTFTexturesAndBuffers();
        m_pGLTFTexturesAndBuffers->OnCreate(m_pDevice, pGLTFCommon, &m_UploadHeap, &m_VidMemBufferPool, &m_ConstantBufferRing);
    }
    else if (stage == 6)
    {
        Profile p("LoadTextures");

        // here we are loading onto the GPU all the textures and the inverse matrices
        // this data will be used to create the PBR and Depth passes       
        m_pGLTFTexturesAndBuffers->LoadTextures(pAsyncPool);
    }
    else if (stage == 7)
    {
        Profile p("m_gltfDepth->OnCreate");

        //create the glTF's textures, VBs, IBs, shaders and descriptors for this particular pass
        m_gltfDepth = new GltfDepthPass();
        m_gltfDepth->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            pAsyncPool
        );
    }
    else if (stage == 8)
    {
        Profile p("m_gltfMotionVectors->OnCreate");

        m_gltfMotionVectors = new GltfMotionVectorsPass();
        m_gltfMotionVectors->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            m_MotionVectors.GetFormat(),
            m_NormalBuffer.GetFormat(),
            pAsyncPool
        );
    }
    else if (stage == 9)
    {
        Profile p("m_gltfPBR->OnCreate");

        // same thing as above but for the PBR pass
        m_gltfPBR = new GltfPbrPass();
        m_gltfPBR->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_skyDome,
            false,                  // use a SSAO mask
            USE_SHADOWMASK,
            m_HDRMSAA.GetFormat(), // forward pass channel
            DXGI_FORMAT_UNKNOWN,   // specular-roughness channel
            DXGI_FORMAT_UNKNOWN,   // diffuse channel
            DXGI_FORMAT_UNKNOWN,   // normal channel
            4,
            pAsyncPool
        );

    }
    else if (stage == 10)
    {
        Profile p("m_gltfBBox->OnCreate");

        // just a bounding box pass that will draw boundingboxes instead of the geometry itself
        m_gltfBBox = new GltfBBoxPass();
        m_gltfBBox->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_wireframe
        );
#if (USE_VID_MEM==true)
        // we are borrowing the upload heap command list for uploading to the GPU the IBs and VBs
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
#endif    
    }
    else if (stage == 11)
    {
        Profile p("Flush");

        m_UploadHeap.FlushAndFinish();

#if (USE_VID_MEM==true)
        //once everything is uploaded we dont need he upload heaps anymore
        m_VidMemBufferPool.FreeUploadHeap();
#endif

        // tell caller that we are done loading the map
        return 0;
    }

    stage++;
    return stage;
}

//--------------------------------------------------------------------------------------
//
// UnloadScene
//
//--------------------------------------------------------------------------------------
void SampleRenderer::UnloadScene()
{
    m_pDevice->GPUFlush();

    if (m_gltfPBR)
    {
        m_gltfPBR->OnDestroy();
        delete m_gltfPBR;
        m_gltfPBR = NULL;
    }

    if (m_gltfMotionVectors)
    {
        m_gltfMotionVectors->OnDestroy();
        delete m_gltfMotionVectors;
        m_gltfMotionVectors = NULL;
    }

    if (m_gltfDepth)
    {
        m_gltfDepth->OnDestroy();
        delete m_gltfDepth;
        m_gltfDepth = NULL;
    }

    if (m_gltfBBox)
    {
        m_gltfBBox->OnDestroy();
        delete m_gltfBBox;
        m_gltfBBox = NULL;
    }

    if (m_pGLTFTexturesAndBuffers)
    {
        m_pGLTFTexturesAndBuffers->OnDestroy();
        delete m_pGLTFTexturesAndBuffers;
        m_pGLTFTexturesAndBuffers = NULL;
    }

}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnRender(State *pState, SwapChain *pSwapChain)
{
    // Timing values
    //
    UINT64 gpuTicksPerSecond;
    m_pDevice->GetGraphicsQueue()->GetTimestampFrequency(&gpuTicksPerSecond);

    // Let our resource managers do some house keeping
    //
    m_CommandListRing.OnBeginFrame();
    m_ConstantBufferRing.OnBeginFrame();
    m_GPUTimer.OnBeginFrame(gpuTicksPerSecond, &m_TimeStamps);

    m_GPUTimer.GetTimeStampUser({ "time (s)", pState->time });

    if (pState->m_useTAA)
    {
        static uint32_t Seed;
        pState->camera.SetProjectionJitter(m_Width, m_Height, Seed);
    }

    // Sets the perFrame data 
    //
    per_frame *pPerFrame = NULL;
    if (m_pGLTFTexturesAndBuffers)
    {
        // fill as much as possible using the GLTF (camera, lights, ...)
        pPerFrame = m_pGLTFTexturesAndBuffers->m_pGLTFCommon->SetPerFrameData(pState->camera);

        // Set some lighting factors
        pPerFrame->iblFactor = pState->iblFactor;
        pPerFrame->emmisiveFactor = pState->emmisiveFactor;
        pPerFrame->invScreenResolution[0] = 1.0f / ((float)m_Width);
        pPerFrame->invScreenResolution[1] = 1.0f / ((float)m_Height);

        // Set shadowmaps bias and an index that indicates the rectangle of the atlas in which depth will be rendered
        uint32_t shadowMapIndex = 0;
        for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
        {
            if ((shadowMapIndex < 4) && (pPerFrame->lights[i].type == LightType_Spot))
            {
                pPerFrame->lights[i].shadowMapIndex = shadowMapIndex++; // set the shadowmap index
                pPerFrame->lights[i].depthBias = 70.0f / 100000.0f;
            }
            else if ((shadowMapIndex < 4) && (pPerFrame->lights[i].type == LightType_Directional))
            {
                pPerFrame->lights[i].shadowMapIndex = shadowMapIndex++; // set the shadowmap index
                pPerFrame->lights[i].depthBias = 1000.0f / 100000.0f;
            }
            else
            {
                pPerFrame->lights[i].shadowMapIndex = -1;   // no shadow for this light
            }
        }

        m_pGLTFTexturesAndBuffers->SetPerFrameConstants();
        m_pGLTFTexturesAndBuffers->SetSkinningMatricesForSkeletons();
    }

    // command buffer calls
    //
    ID3D12GraphicsCommandList* pCmdLst1 = m_CommandListRing.GetNewCommandList();

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Begin Frame");

    // Clear GBuffer and depth stencil
    //
    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clears -----------------------------------------------------------------------
    //
    pCmdLst1->ClearDepthStencilView(m_ShadowMapDSV.GetCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear shadow map");

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCmdLst1->ClearRenderTargetView(m_HDRRTVMSAA.GetCPU(), clearColor, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear HDR");

    pCmdLst1->ClearDepthStencilView(m_depthBufferDSV.GetCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear depth");

    // Render to shadow map atlas for spot lights ------------------------------------------
    //
    if (m_gltfDepth && pPerFrame != NULL)
    {
        uint32_t shadowMapIndex = 0;
        for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
        {
            if (!(pPerFrame->lights[i].type == LightType_Spot || pPerFrame->lights[i].type == LightType_Directional))
                continue;

            // Set the RT's quadrant where to render the shadomap (these viewport offsets need to match the ones in shadowFiltering.h)
            uint32_t viewportOffsetsX[4] = { 0, 1, 0, 1 };
            uint32_t viewportOffsetsY[4] = { 0, 0, 1, 1 };
            uint32_t viewportWidth = m_shadowMap.GetWidth() / 2;
            uint32_t viewportHeight = m_shadowMap.GetHeight() / 2;
            SetViewportAndScissor(pCmdLst1, viewportOffsetsX[i] * viewportWidth, viewportOffsetsY[i] * viewportHeight, viewportWidth, viewportHeight);
            pCmdLst1->OMSetRenderTargets(0, NULL, false, &m_ShadowMapDSV.GetCPU());

            GltfDepthPass::per_frame *cbDepthPerFrame = m_gltfDepth->SetPerFrameConstants();
            cbDepthPerFrame->mViewProj = pPerFrame->lights[i].mLightViewProj;

            m_gltfDepth->Draw(pCmdLst1);

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Shadow map");
            shadowMapIndex++;
        }
    }

    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    // Motion vectors ---------------------------------------------------------------------------
    //
    if (pState->m_useTAA)
    {
        if (pPerFrame != NULL && m_gltfMotionVectors != NULL)
        {
            // Compute motion vectors
            pCmdLst1->RSSetViewports(1, &m_viewport);
            pCmdLst1->RSSetScissorRects(1, &m_rectScissor);
            D3D12_CPU_DESCRIPTOR_HANDLE rts[] = { m_MotionVectorsRTV.GetCPU(), m_NormalBufferRTV.GetCPU() };
            pCmdLst1->OMSetRenderTargets(ARRAYSIZE(rts), rts, false, &m_MotionVectorsDepthMapDSV.GetCPU());

            float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            pCmdLst1->ClearRenderTargetView(m_MotionVectorsRTV.GetCPU(), clearColor, 0, nullptr);
            pCmdLst1->ClearRenderTargetView(m_NormalBufferRTV.GetCPU(), clearColor, 0, nullptr);
            pCmdLst1->ClearDepthStencilView(m_MotionVectorsDepthMapDSV.GetCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            GltfMotionVectorsPass::per_frame *cbDepthPerFrame = m_gltfMotionVectors->SetPerFrameConstants();
            cbDepthPerFrame->mCurrViewProj = pPerFrame->mCameraViewProj;
            cbDepthPerFrame->mPrevViewProj = pState->camera.GetPrevView() * pState->camera.GetProjection();

            m_gltfMotionVectors->Draw(pCmdLst1);
        }

        m_GPUTimer.GetTimeStamp(pCmdLst1, "Motion vectors");
    }

    // Shadow resolve ---------------------------------------------------------------------------
    //
#if USE_SHADOWMASK
    if (pPerFrame != NULL)
    {
        const D3D12_RESOURCE_BARRIER preShadowResolve[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_ShadowMask.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_MotionVectorsDepthMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        pCmdLst1->ResourceBarrier(ARRAYSIZE(preShadowResolve), preShadowResolve);

        ShadowResolveFrame shadowResolveFrame;
        shadowResolveFrame.m_Width = m_Width;
        shadowResolveFrame.m_Height = m_Height;
        shadowResolveFrame.m_ShadowMapSRV = m_ShadowMapSRV;
        shadowResolveFrame.m_DepthBufferSRV = m_MotionVectorsDepthMapSRV;
        shadowResolveFrame.m_ShadowBufferUAV = m_ShadowMaskUAV;

        m_shadowResolve.Draw(pCmdLst1, m_pGLTFTexturesAndBuffers, &shadowResolveFrame);

        const D3D12_RESOURCE_BARRIER postShadowResolve[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_ShadowMask.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_MotionVectorsDepthMap.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
        };
        pCmdLst1->ResourceBarrier(ARRAYSIZE(postShadowResolve), postShadowResolve);
    }
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Shadow resolve");
#endif

    // Render Scene to the MSAA HDR RT ------------------------------------------------
    //
    pCmdLst1->RSSetViewports(1, &m_viewport);
    pCmdLst1->RSSetScissorRects(1, &m_rectScissor);
    pCmdLst1->OMSetRenderTargets(1, &m_HDRRTVMSAA.GetCPU(), false, &m_depthBufferDSV.GetCPU());

    if (pPerFrame != NULL)
    {
        // Render skydome
        //
        if (pState->skyDomeType == 1)
        {
            XMMATRIX clipToView = XMMatrixInverse(NULL, pPerFrame->mCameraViewProj);
            m_skyDome.Draw(pCmdLst1, clipToView);
            m_GPUTimer.GetTimeStamp(pCmdLst1, "Skydome cube");
        }
        else if (pState->skyDomeType == 0)
        {
            SkyDomeProc::Constants skyDomeConstants;
            skyDomeConstants.invViewProj = XMMatrixInverse(NULL, pPerFrame->mCameraViewProj);
            skyDomeConstants.vSunDirection = XMVectorSet(1.0f, 0.05f, 0.0f, 0.0f);
            skyDomeConstants.turbidity = 10.0f;
            skyDomeConstants.rayleigh = 2.0f;
            skyDomeConstants.mieCoefficient = 0.005f;
            skyDomeConstants.mieDirectionalG = 0.8f;
            skyDomeConstants.luminance = 1.0f;
            skyDomeConstants.sun = false;
            m_skyDomeProc.Draw(pCmdLst1, skyDomeConstants);

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Skydome proc");
        }

        // Render scene to color buffer
        //
        if (m_gltfPBR && pPerFrame != NULL)
        {
            //set per frame constant buffer values
#if USE_SHADOWMASK
            m_gltfPBR->Draw(pCmdLst1, &m_ShadowMaskSRV);
#else
            m_gltfPBR->Draw(pCmdLst1, &m_ShadowMapSRV);
#endif
            m_GPUTimer.GetTimeStamp(pCmdLst1, "PBR Forward");
        }

        // draw object's bounding boxes
        //
        if (m_gltfBBox && pPerFrame != NULL)
        {
            if (pState->bDrawBoundingBoxes)
            {
                m_gltfBBox->Draw(pCmdLst1, pPerFrame->mCameraViewProj);

                m_GPUTimer.GetTimeStamp(pCmdLst1, "Bounding Box");
            }
        }

        // draw light's frustums
        //
        if (pState->bDrawLightFrustum && pPerFrame != NULL)
        {
            UserMarker marker(pCmdLst1, "light frustrums");

            XMVECTOR vCenter = XMVectorSet(0.0f, 0.0f, 0.5f, 0.0f);
            XMVECTOR vRadius = XMVectorSet(1.0f, 1.0f, 0.5f, 0.0f);
            XMVECTOR vColor = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
            for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
            {
                XMMATRIX spotlightMatrix = XMMatrixInverse(NULL, pPerFrame->lights[i].mLightViewProj);
                XMMATRIX worldMatrix = spotlightMatrix * pPerFrame->mCameraViewProj;
                m_wireframeBox.Draw(pCmdLst1, &m_wireframe, worldMatrix, vCenter, vRadius, vColor);
            }

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Light's frustum");
        }
    }
    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Resolve MSAA ------------------------------------------------------------------------
    //
    {
        UserMarker marker(pCmdLst1, "Resolving MSAA");

        D3D12_RESOURCE_BARRIER preResolve[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDRMSAA.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
        };
        pCmdLst1->ResourceBarrier(2, preResolve);

        pCmdLst1->ResolveSubresource(m_HDR.GetResource(), 0, m_HDRMSAA.GetResource(), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);

        D3D12_RESOURCE_BARRIER postResolve[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDRMSAA.GetResource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        pCmdLst1->ResourceBarrier(2, postResolve);

        m_GPUTimer.GetTimeStamp(pCmdLst1, "Resolve MSAA");
    }

    // Post proc---------------------------------------------------------------------------
    //

    // Bloom, takes HDR as input and applies bloom to it.
    //
    {
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = { m_HDRRTV.GetCPU() };
        pCmdLst1->OMSetRenderTargets(ARRAYSIZE(renderTargets), renderTargets, false, NULL);

        m_downSample.Draw(pCmdLst1);
        //m_downSample.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Downsample");

        m_bloom.Draw(pCmdLst1, &m_HDR);
        //m_bloom.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Bloom");
    }

    // TAA + Sharpen
    //
    if (pState->m_useTAA)
    {
        D3D12_RESOURCE_BARRIER preTAA[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_TAABuffer.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };
        pCmdLst1->ResourceBarrier(ARRAYSIZE(preTAA), preTAA);

        // break down of the descriptor tables:
        //  m_TAABufferUAV = { m_TAABuffer }
        //  m_TAAInputsSRV = { m_HDR, m_MotionVectorsDepthMap, m_HistoryBuffer, m_MotionVectors }
        m_taa.Draw(pCmdLst1, &m_TAABufferUAV, &m_TAAInputsSRV, m_Width, m_Height);
        m_GPUTimer.GetTimeStamp(pCmdLst1, "TAA");

        D3D12_RESOURCE_BARRIER postTAA[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(m_TAABuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HistoryBuffer.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        pCmdLst1->ResourceBarrier(ARRAYSIZE(postTAA), postTAA);

        D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = {
            m_HDRRTV.GetCPU(),
            m_HistoryBufferRTV.GetCPU()
        };
        pCmdLst1->RSSetViewports(1, &m_viewport);
        pCmdLst1->RSSetScissorRects(1, &m_rectScissor);
        pCmdLst1->OMSetRenderTargets(ARRAYSIZE(renderTargets), renderTargets, false, NULL);

        // m_TAABufferSRV = { m_TAABuffer }
        m_sharpen.Draw(pCmdLst1, &m_TAABufferSRV);
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Sharpen");

        D3D12_RESOURCE_BARRIER postSharpen[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HistoryBuffer.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        pCmdLst1->ResourceBarrier(ARRAYSIZE(postSharpen), postSharpen);
    }

    // If using FreeSync HDR we need to to the tonemapping in-place and then apply the GUI, later we'll apply the color conversion into the swapchain
    //
    if (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR)
    {
        // In place Tonemapping ------------------------------------------------------------------------
        //
        {
            D3D12_RESOURCE_BARRIER hdrToUAV = CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            pCmdLst1->ResourceBarrier(1, &hdrToUAV);

            m_toneMappingCS.Draw(pCmdLst1, &m_HDRUAV, pState->exposure, pState->toneMapper, m_Width, m_Height);

            D3D12_RESOURCE_BARRIER hdrToRTV = CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
            pCmdLst1->ResourceBarrier(1, &hdrToRTV);
        }

        // Render HUD  ------------------------------------------------------------------------
        //
        {
            pCmdLst1->RSSetViewports(1, &m_viewport);
            pCmdLst1->RSSetScissorRects(1, &m_rectScissor);
            pCmdLst1->OMSetRenderTargets(1, &m_HDRRTV.GetCPU(), true, NULL);

            m_ImGUI.Draw(pCmdLst1);

            D3D12_RESOURCE_BARRIER hdrToSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            pCmdLst1->ResourceBarrier(1, &hdrToSRV);

            m_GPUTimer.GetTimeStamp(pCmdLst1, "ImGUI Rendering");
        }
    }

    // submit command buffer #1
    ThrowIfFailed(pCmdLst1->Close());
    ID3D12CommandList* CmdListList1[] = { pCmdLst1 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList1);

    // Wait for swapchain (we are going to render to it) -----------------------------------
    //
    pSwapChain->WaitForSwapChain();

    ID3D12GraphicsCommandList* pCmdLst2 = m_CommandListRing.GetNewCommandList();

    if (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR)
    {
        // FS HDR mode! Apply color conversion now.
        //
        pCmdLst2->RSSetViewports(1, &m_viewport);
        pCmdLst2->RSSetScissorRects(1, &m_rectScissor);
        pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

        m_colorConversionPS.Draw(pCmdLst2, &m_HDRSRV);
        m_GPUTimer.GetTimeStamp(pCmdLst2, "Color conversion");

        pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
    }
    else
    {
        // non FS HDR mode, that is SDR, here we apply the tonemapping from the HDR into the swapchain and then we render the GUI
        //

        // Tonemapping ------------------------------------------------------------------------
        //
        {
            pCmdLst2->RSSetViewports(1, &m_viewport);
            pCmdLst2->RSSetScissorRects(1, &m_rectScissor);
            pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

            m_toneMappingPS.Draw(pCmdLst2, &m_HDRSRV, pState->exposure, pState->toneMapper);
            m_GPUTimer.GetTimeStamp(pCmdLst2, "Tone mapping");

            pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        }

        // Render HUD  ------------------------------------------------------------------------
        //
        {
            pCmdLst2->RSSetViewports(1, &m_viewport);
            pCmdLst2->RSSetScissorRects(1, &m_rectScissor);
            pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

            m_ImGUI.Draw(pCmdLst2);

            m_GPUTimer.GetTimeStamp(pCmdLst2, "ImGUI Rendering");
        }
    }

    if (pState->m_pScreenShotName != NULL)
    {
        m_saveTexture.CopyRenderTargetIntoStagingTexture(m_pDevice->GetDevice(), pCmdLst2, pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    // Transition swapchain into present mode

    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    m_GPUTimer.OnEndFrame();

    m_GPUTimer.CollectTimings(pCmdLst2);

    // Close & Submit the command list #2 -------------------------------------------------
    //
    ThrowIfFailed(pCmdLst2->Close());

    ID3D12CommandList* CmdListList2[] = { pCmdLst2 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList2);

    if (pState->m_pScreenShotName != NULL)
    {
        m_saveTexture.SaveStagingTextureAsJpeg(m_pDevice->GetDevice(), m_pDevice->GetGraphicsQueue(), pState->m_pScreenShotName->c_str());
        pState->m_pScreenShotName = NULL;
    }

    // Update previous camera matrices
    //
    pState->camera.UpdatePreviousMatrices();
}
