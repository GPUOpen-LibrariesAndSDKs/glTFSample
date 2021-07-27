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

#include "Renderer.h"
#include "UI.h"

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void Renderer::OnCreate(Device *pDevice, SwapChain *pSwapChain, float FontSize)
{
    m_pDevice = pDevice;

    // Initialize helpers

    // Create all the heaps for the resources views
    const uint32_t cbvDescriptorCount = 2000;
    const uint32_t srvDescriptorCount = 8000;
    const uint32_t uavDescriptorCount = 10;
    const uint32_t samplerDescriptorCount = 20;
    m_ResourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, samplerDescriptorCount);

    // Create a commandlist ring for the Direct queue
    uint32_t commandListsPerBackBuffer = 8;
    m_CommandListRing.OnCreate(pDevice, backBufferCount, commandListsPerBackBuffer);

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 200 * 1024 * 1024;
    m_ConstantBufferRing.OnCreate(pDevice, backBufferCount, constantBuffersMemSize, "Uniforms");

    // Create a 'static' pool for vertices and indices 
    const uint32_t staticGeometryMemSize = (1 * 128) * 1024 * 1024;
    m_VidMemBufferPool.OnCreate(pDevice, staticGeometryMemSize, true, "StaticGeom");

    // Create a 'static' pool for vertices and indices in system memory
    const uint32_t systemGeometryMemSize = 32 * 1024;
    m_SysMemBufferPool.OnCreate(pDevice, systemGeometryMemSize, false, "PostProcGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, backBufferCount);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    const uint32_t uploadHeapMemSize = 1000 * 1024 * 1024;
    m_UploadHeap.OnCreate(pDevice, uploadHeapMemSize);    // initialize an upload heap (uses suballocation for faster results)

    // Create GBuffer and render passes
    //
    {
        m_GBuffer.OnCreate(
            pDevice, 
            &m_ResourceViewHeaps, 
            {
                { GBUFFER_DEPTH, VK_FORMAT_D32_SFLOAT},
                { GBUFFER_FORWARD, VK_FORMAT_R16G16B16A16_SFLOAT},
                { GBUFFER_MOTION_VECTORS, VK_FORMAT_R16G16_SFLOAT},
            },
            1
        );

        GBufferFlags fullGBuffer = GBUFFER_DEPTH | GBUFFER_FORWARD | GBUFFER_MOTION_VECTORS;
        bool bClear = true;
        m_RenderPassFullGBufferWithClear.OnCreate(&m_GBuffer, fullGBuffer, bClear,"m_RenderPassFullGBufferWithClear");
        m_RenderPassFullGBuffer.OnCreate(&m_GBuffer, fullGBuffer, !bClear, "m_RenderPassFullGBuffer");
        m_RenderPassJustDepthAndHdr.OnCreate(&m_GBuffer, GBUFFER_DEPTH | GBUFFER_FORWARD, !bClear, "m_RenderPassJustDepthAndHdr");
    }

    // Create render pass shadow, will clear contents
    {
        VkAttachmentDescription depthAttachments;
        AttachClearBeforeUse(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &depthAttachments);
        m_Render_pass_shadow = CreateRenderPassOptimal(m_pDevice->GetDevice(), 0, NULL, &depthAttachments);
    }

    m_SkyDome.OnCreate(pDevice, m_RenderPassJustDepthAndHdr.GetRenderPass(), &m_UploadHeap, VK_FORMAT_R16G16B16A16_SFLOAT, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, "..\\media\\cauldron-media\\envmaps\\papermill\\diffuse.dds", "..\\media\\cauldron-media\\envmaps\\papermill\\specular.dds", VK_SAMPLE_COUNT_1_BIT);
    m_SkyDomeProc.OnCreate(pDevice, m_RenderPassJustDepthAndHdr.GetRenderPass(), &m_UploadHeap, VK_FORMAT_R16G16B16A16_SFLOAT, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_SAMPLE_COUNT_1_BIT);
    m_Wireframe.OnCreate(pDevice, m_RenderPassJustDepthAndHdr.GetRenderPass(), &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_SAMPLE_COUNT_1_BIT);
    m_WireframeBox.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool);
    m_DownSample.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_Bloom.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_TAA.OnCreate(pDevice, &m_ResourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);
    m_MagnifierPS.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);

    // Create tonemapping pass
    m_ToneMappingCS.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing);
    m_ToneMappingPS.OnCreate(m_pDevice, pSwapChain->GetRenderPass(), &m_ResourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);
    m_ColorConversionPS.OnCreate(pDevice, pSwapChain->GetRenderPass(), &m_ResourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(m_pDevice, pSwapChain->GetRenderPass(), &m_UploadHeap, &m_ConstantBufferRing, FontSize);

    // Make sure upload heap has finished uploading before continuing
    m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    m_UploadHeap.FlushAndFinish();
}

//--------------------------------------------------------------------------------------
//
// OnDestroy 
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroy()
{
    m_AsyncPool.Flush();

    m_ImGUI.OnDestroy();
    m_ColorConversionPS.OnDestroy();
    m_ToneMappingPS.OnDestroy();
    m_ToneMappingCS.OnDestroy();
    m_TAA.OnDestroy();
    m_Bloom.OnDestroy();
    m_DownSample.OnDestroy();
    m_MagnifierPS.OnDestroy();
    m_WireframeBox.OnDestroy();
    m_Wireframe.OnDestroy();
    m_SkyDomeProc.OnDestroy();
    m_SkyDome.OnDestroy();

    m_RenderPassFullGBufferWithClear.OnDestroy();
    m_RenderPassJustDepthAndHdr.OnDestroy();
    m_RenderPassFullGBuffer.OnDestroy();
    m_GBuffer.OnDestroy();

    vkDestroyRenderPass(m_pDevice->GetDevice(), m_Render_pass_shadow, nullptr);
       
    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
    m_SysMemBufferPool.OnDestroy();
    m_ConstantBufferRing.OnDestroy();
    m_ResourceViewHeaps.OnDestroy();
    m_CommandListRing.OnDestroy();    
}

//--------------------------------------------------------------------------------------
//
// OnCreateWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height)
{
    m_Width = Width;
    m_Height = Height;
    
    // Set the viewport
    //
    m_Viewport.x = 0;
    m_Viewport.y = (float)Height;
    m_Viewport.width = (float)Width;
    m_Viewport.height = -(float)(Height);
    m_Viewport.minDepth = (float)0.0f;
    m_Viewport.maxDepth = (float)1.0f;

    // Create scissor rectangle
    //
    m_RectScissor.extent.width = Width;
    m_RectScissor.extent.height = Height;
    m_RectScissor.offset.x = 0;
    m_RectScissor.offset.y = 0;
   
    // Create GBuffer
    //
    m_GBuffer.OnCreateWindowSizeDependentResources(pSwapChain, Width, Height);

    // Create frame buffers for the GBuffer render passes
    //
    m_RenderPassFullGBufferWithClear.OnCreateWindowSizeDependentResources(Width, Height);
    m_RenderPassJustDepthAndHdr.OnCreateWindowSizeDependentResources(Width, Height);
    m_RenderPassFullGBuffer.OnCreateWindowSizeDependentResources(Width, Height);

    // Update PostProcessing passes
    //
    m_DownSample.OnCreateWindowSizeDependentResources(Width, Height, &m_GBuffer.m_HDR, 6); //downsample the HDR texture 6 times
    m_Bloom.OnCreateWindowSizeDependentResources(Width / 2, Height / 2, m_DownSample.GetTexture(), 6, &m_GBuffer.m_HDR);
    m_TAA.OnCreateWindowSizeDependentResources(Width, Height, &m_GBuffer);
    m_MagnifierPS.OnCreateWindowSizeDependentResources(&m_GBuffer.m_HDR);
    m_bMagResourceReInit = true;
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroyWindowSizeDependentResources()
{
    m_Bloom.OnDestroyWindowSizeDependentResources();
    m_DownSample.OnDestroyWindowSizeDependentResources();
    m_TAA.OnDestroyWindowSizeDependentResources();
    m_MagnifierPS.OnDestroyWindowSizeDependentResources();

    m_RenderPassFullGBufferWithClear.OnDestroyWindowSizeDependentResources();
    m_RenderPassJustDepthAndHdr.OnDestroyWindowSizeDependentResources();
    m_RenderPassFullGBuffer.OnDestroyWindowSizeDependentResources();
    m_GBuffer.OnDestroyWindowSizeDependentResources();
}

void Renderer::OnUpdateDisplayDependentResources(SwapChain *pSwapChain, bool bUseMagnifier)
{
    // Update the pipelines if the swapchain render pass has changed (for example when the format of the swapchain changes)
    //
    m_ColorConversionPS.UpdatePipelines(pSwapChain->GetRenderPass(), pSwapChain->GetDisplayMode());
    m_ToneMappingPS.UpdatePipelines(pSwapChain->GetRenderPass());

    m_ImGUI.UpdatePipeline((pSwapChain->GetDisplayMode() == DISPLAYMODE_SDR) ? pSwapChain->GetRenderPass() : bUseMagnifier ? m_MagnifierPS.GetPassRenderPass() : m_RenderPassJustDepthAndHdr.GetRenderPass());
}

//--------------------------------------------------------------------------------------
//
// OnUpdateLocalDimmingChangedResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnUpdateLocalDimmingChangedResources(SwapChain *pSwapChain)
{
    m_ColorConversionPS.UpdatePipelines(pSwapChain->GetRenderPass(), pSwapChain->GetDisplayMode());
}

//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
int Renderer::LoadScene(GLTFCommon *pGLTFCommon, int Stage)
{
    // show loading progress
    //
    ImGui::OpenPopup("Loading");
    if (ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        float progress = (float)Stage / 12.0f;
        ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), NULL);
        ImGui::EndPopup();
    } 

    // use multi threading
    AsyncPool *pAsyncPool = &m_AsyncPool;

    // Loading stages
    //
    if (Stage == 0)
    {
    }
    else if (Stage == 5)
    {   
        Profile p("m_pGltfLoader->Load");
        
        m_pGLTFTexturesAndBuffers = new GLTFTexturesAndBuffers();
        m_pGLTFTexturesAndBuffers->OnCreate(m_pDevice, pGLTFCommon, &m_UploadHeap, &m_VidMemBufferPool, &m_ConstantBufferRing);
    }
    else if (Stage == 6)
    {
        Profile p("LoadTextures");

        // here we are loading onto the GPU all the textures and the inverse matrices
        // this data will be used to create the PBR and Depth passes       
        m_pGLTFTexturesAndBuffers->LoadTextures(pAsyncPool);
    }
    else if (Stage == 7)
    {
        Profile p("m_GLTFDepth->OnCreate");

        //create the glTF's textures, VBs, IBs, shaders and descriptors for this particular pass    
        m_GLTFDepth = new GltfDepthPass();
        m_GLTFDepth->OnCreate(
            m_pDevice,
            m_Render_pass_shadow,
            &m_UploadHeap,
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            pAsyncPool
        );

        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
        m_UploadHeap.FlushAndFinish();
    }
    else if (Stage == 8)
    {
        Profile p("m_GLTFPBR->OnCreate");

        // same thing as above but for the PBR pass
        m_GLTFPBR = new GltfPbrPass();
        m_GLTFPBR->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_SkyDome,
            false, // use SSAO mask
            m_ShadowSRVPool,
            &m_RenderPassFullGBufferWithClear,
            pAsyncPool
        );

        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
        m_UploadHeap.FlushAndFinish();
    }
    else if (Stage == 9)
    {
        Profile p("m_GLTFBBox->OnCreate");

        // just a bounding box pass that will draw boundingboxes instead of the geometry itself
        m_GLTFBBox = new GltfBBoxPass();
            m_GLTFBBox->OnCreate(
            m_pDevice,
            m_RenderPassJustDepthAndHdr.GetRenderPass(),
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_Wireframe
        );

        // we are borrowing the upload heap command list for uploading to the GPU the IBs and VBs
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());

    }
    else if (Stage == 10)
    {
        Profile p("Flush");

        m_UploadHeap.FlushAndFinish();

        //once everything is uploaded we dont need the upload heaps anymore
        m_VidMemBufferPool.FreeUploadHeap();

        // tell caller that we are done loading the map
        return 0;
    }

    Stage++;
    return Stage;
}

//--------------------------------------------------------------------------------------
//
// UnloadScene
//
//--------------------------------------------------------------------------------------
void Renderer::UnloadScene()
{
    // wait for all the async loading operations to finish
    m_AsyncPool.Flush();

    m_pDevice->GPUFlush();

    if (m_GLTFPBR)
    {
        m_GLTFPBR->OnDestroy();
        delete m_GLTFPBR;
        m_GLTFPBR = NULL;
    }

    if (m_GLTFDepth)
    {
        m_GLTFDepth->OnDestroy();
        delete m_GLTFDepth;
        m_GLTFDepth = NULL;
    }

    if (m_GLTFBBox)
    {
        m_GLTFBBox->OnDestroy();
        delete m_GLTFBBox;
        m_GLTFBBox = NULL;
    }

    if (m_pGLTFTexturesAndBuffers)
    {
        m_pGLTFTexturesAndBuffers->OnDestroy();
        delete m_pGLTFTexturesAndBuffers;
        m_pGLTFTexturesAndBuffers = NULL;
    }

    assert(m_shadowMapPool.size() == m_ShadowSRVPool.size());
    while (!m_shadowMapPool.empty())
    {
        m_shadowMapPool.back().ShadowMap.OnDestroy();
        vkDestroyFramebuffer(m_pDevice->GetDevice(), m_shadowMapPool.back().ShadowFrameBuffer, nullptr);
        vkDestroyImageView(m_pDevice->GetDevice(), m_ShadowSRVPool.back(), nullptr);
        vkDestroyImageView(m_pDevice->GetDevice(), m_shadowMapPool.back().ShadowDSV, nullptr);
        m_ShadowSRVPool.pop_back();
        m_shadowMapPool.pop_back();
    }
}

void Renderer::AllocateShadowMaps(GLTFCommon* pGLTFCommon)
{
    // Go through the lights and allocate shadow information
    uint32_t NumShadows = 0;
    for (int i = 0; i < pGLTFCommon->m_lightInstances.size(); ++i)
    {
        const tfLight& lightData = pGLTFCommon->m_lights[pGLTFCommon->m_lightInstances[i].m_lightId];
        if (lightData.m_shadowResolution)
        {
            SceneShadowInfo ShadowInfo;
            ShadowInfo.ShadowResolution = lightData.m_shadowResolution;
            ShadowInfo.ShadowIndex = NumShadows++;
            ShadowInfo.LightIndex = i;
            m_shadowMapPool.push_back(ShadowInfo);
        }
    }

    if (NumShadows > MaxShadowInstances)
    {
        Trace("Number of shadows has exceeded maximum supported. Please grow value in gltfCommon.h/perFrameStruct.h");
        throw;
    }

    // If we had shadow information, allocate all required maps and bindings
    if (!m_shadowMapPool.empty())
    {
        std::vector<SceneShadowInfo>::iterator CurrentShadow = m_shadowMapPool.begin();
        for (uint32_t i = 0; CurrentShadow < m_shadowMapPool.end(); ++i, ++CurrentShadow)
        {
            CurrentShadow->ShadowMap.InitDepthStencil(m_pDevice, CurrentShadow->ShadowResolution, CurrentShadow->ShadowResolution, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, "ShadowMap");
            CurrentShadow->ShadowMap.CreateDSV(&CurrentShadow->ShadowDSV);

            // Create render pass shadow, will clear contents
            {
                VkAttachmentDescription depthAttachments;
                AttachClearBeforeUse(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &depthAttachments);

                // Create frame buffer
                VkImageView attachmentViews[1] = { CurrentShadow->ShadowDSV };
                VkFramebufferCreateInfo fb_info = {};
                fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fb_info.pNext = NULL;
                fb_info.renderPass = m_Render_pass_shadow;
                fb_info.attachmentCount = 1;
                fb_info.pAttachments = attachmentViews;
                fb_info.width = CurrentShadow->ShadowResolution;
                fb_info.height = CurrentShadow->ShadowResolution;
                fb_info.layers = 1;
                VkResult res = vkCreateFramebuffer(m_pDevice->GetDevice(), &fb_info, NULL, &CurrentShadow->ShadowFrameBuffer);
                assert(res == VK_SUCCESS);
            }

            VkImageView ShadowSRV;
            CurrentShadow->ShadowMap.CreateSRV(&ShadowSRV);
            m_ShadowSRVPool.push_back(ShadowSRV);
        }
    }
}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void Renderer::OnRender(const UIState* pState, const Camera& Cam, SwapChain* pSwapChain)
{
    // Let our resource managers do some house keeping 
    m_ConstantBufferRing.OnBeginFrame();

    // command buffer calls
    VkCommandBuffer cmdBuf1 = m_CommandListRing.GetNewCommandList();

    {
        VkCommandBufferBeginInfo cmd_buf_info;
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmd_buf_info.pInheritanceInfo = NULL;
        VkResult res = vkBeginCommandBuffer(cmdBuf1, &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }

    m_GPUTimer.OnBeginFrame(cmdBuf1, &m_TimeStamps);

    // Sets the perFrame data 
    per_frame *pPerFrame = NULL;
    if (m_pGLTFTexturesAndBuffers)
    {
        // fill as much as possible using the GLTF (camera, lights, ...)
        pPerFrame = m_pGLTFTexturesAndBuffers->m_pGLTFCommon->SetPerFrameData(Cam);

        // Set some lighting factors
        pPerFrame->iblFactor = pState->IBLFactor;
        pPerFrame->emmisiveFactor = pState->EmissiveFactor;
        pPerFrame->invScreenResolution[0] = 1.0f / ((float)m_Width);
        pPerFrame->invScreenResolution[1] = 1.0f / ((float)m_Height);

        pPerFrame->wireframeOptions.setX(pState->WireframeColor[0]);
        pPerFrame->wireframeOptions.setY(pState->WireframeColor[1]);
        pPerFrame->wireframeOptions.setZ(pState->WireframeColor[2]);
        pPerFrame->wireframeOptions.setW(pState->WireframeMode == UIState::WireframeMode::WIREFRAME_MODE_SOLID_COLOR ? 1.0f : 0.0f);
        pPerFrame->lodBias = 0.0f;
        m_pGLTFTexturesAndBuffers->SetPerFrameConstants();
        m_pGLTFTexturesAndBuffers->SetSkinningMatricesForSkeletons();
    }

    // Render all shadow maps
    if (m_GLTFDepth && pPerFrame != NULL)
    {
        SetPerfMarkerBegin(cmdBuf1, "ShadowPass");

        VkClearValue depth_clear_values[1];
        depth_clear_values[0].depthStencil.depth = 1.0f;
        depth_clear_values[0].depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin;
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.pNext = NULL;
        rp_begin.renderPass = m_Render_pass_shadow;
        rp_begin.renderArea.offset.x = 0;
        rp_begin.renderArea.offset.y = 0;
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = depth_clear_values;

        std::vector<SceneShadowInfo>::iterator ShadowMap = m_shadowMapPool.begin();
        while (ShadowMap < m_shadowMapPool.end())
        {
            // Clear shadow map
            rp_begin.framebuffer = ShadowMap->ShadowFrameBuffer;
            rp_begin.renderArea.extent.width = ShadowMap->ShadowResolution;
            rp_begin.renderArea.extent.height = ShadowMap->ShadowResolution;
            vkCmdBeginRenderPass(cmdBuf1, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

            // Render to shadow map
            SetViewportAndScissor(cmdBuf1, 0, 0, ShadowMap->ShadowResolution, ShadowMap->ShadowResolution);

            // Set per frame constant buffer values
            GltfDepthPass::per_frame* cbPerFrame = m_GLTFDepth->SetPerFrameConstants();
            cbPerFrame->mViewProj = pPerFrame->lights[ShadowMap->LightIndex].mLightViewProj;

            m_GLTFDepth->Draw(cmdBuf1);

            m_GPUTimer.GetTimeStamp(cmdBuf1, "Shadow Map Render");

            vkCmdEndRenderPass(cmdBuf1);
            ++ShadowMap;
        }
        
        SetPerfMarkerEnd(cmdBuf1);
    }

    // Render Scene to the GBuffer ------------------------------------------------
    SetPerfMarkerBegin(cmdBuf1, "Color pass");

    VkRect2D renderArea = { 0, 0, m_Width, m_Height };

    if (pPerFrame != NULL && m_GLTFPBR)
    {
        const bool bWireframe = pState->WireframeMode != UIState::WireframeMode::WIREFRAME_MODE_OFF;

        std::vector<GltfPbrPass::BatchList> opaque, transparent;
        m_GLTFPBR->BuildBatchLists(&opaque, &transparent, bWireframe);

        // Render opaque 
        {
            m_RenderPassFullGBufferWithClear.BeginPass(cmdBuf1, renderArea);

            m_GLTFPBR->DrawBatchList(cmdBuf1, &opaque, bWireframe);
            m_GPUTimer.GetTimeStamp(cmdBuf1, "PBR Opaque");

            m_RenderPassFullGBufferWithClear.EndPass(cmdBuf1);
        }

        // Render skydome
        {
            m_RenderPassJustDepthAndHdr.BeginPass(cmdBuf1, renderArea);

            if (pState->SelectedSkydomeTypeIndex == 1)
            {
                math::Matrix4 clipToView = math::inverse(pPerFrame->mCameraCurrViewProj);
                m_SkyDome.Draw(cmdBuf1, clipToView);

                m_GPUTimer.GetTimeStamp(cmdBuf1, "Skydome cube");
            }
            else if (pState->SelectedSkydomeTypeIndex == 0)
            {
                SkyDomeProc::Constants skyDomeConstants;
				skyDomeConstants.invViewProj = math::inverse(pPerFrame->mCameraCurrViewProj);
				skyDomeConstants.vSunDirection = math::Vector4(1.0f, 0.05f, 0.0f, 0.0f);
                skyDomeConstants.turbidity = 10.0f;
                skyDomeConstants.rayleigh = 2.0f;
                skyDomeConstants.mieCoefficient = 0.005f;
                skyDomeConstants.mieDirectionalG = 0.8f;
                skyDomeConstants.luminance = 1.0f;
                m_SkyDomeProc.Draw(cmdBuf1, skyDomeConstants);

                m_GPUTimer.GetTimeStamp(cmdBuf1, "Skydome Proc");
            }

            m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);
        }

        // draw transparent geometry
        {
            m_RenderPassFullGBuffer.BeginPass(cmdBuf1, renderArea);

            std::sort(transparent.begin(), transparent.end());
            m_GLTFPBR->DrawBatchList(cmdBuf1, &transparent, bWireframe);
            m_GPUTimer.GetTimeStamp(cmdBuf1, "PBR Transparent");

            m_RenderPassFullGBuffer.EndPass(cmdBuf1);
        }

        // draw object's bounding boxes
        {
            m_RenderPassJustDepthAndHdr.BeginPass(cmdBuf1, renderArea);

            if (m_GLTFBBox)
            {
                if (pState->bDrawBoundingBoxes)
                {
                    m_GLTFBBox->Draw(cmdBuf1, pPerFrame->mCameraCurrViewProj);

                    m_GPUTimer.GetTimeStamp(cmdBuf1, "Bounding Box");
                }
            }

            // draw light's frustums
            if (pState->bDrawLightFrustum && pPerFrame != NULL)
            {
                SetPerfMarkerBegin(cmdBuf1, "light frustums");

                math::Vector4 vCenter = math::Vector4(0.0f, 0.0f, 0.5f, 0.0f);
                math::Vector4 vRadius = math::Vector4(1.0f, 1.0f, 0.5f, 0.0f);
                math::Vector4 vColor = math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
                {
                    math::Matrix4 spotlightMatrix = math::inverse(pPerFrame->lights[i].mLightViewProj);
                    math::Matrix4 worldMatrix = pPerFrame->mCameraCurrViewProj * spotlightMatrix;
                    m_WireframeBox.Draw(cmdBuf1, &m_Wireframe, worldMatrix, vCenter, vRadius, vColor);
                }

                m_GPUTimer.GetTimeStamp(cmdBuf1, "Light's frustum");

                SetPerfMarkerEnd(cmdBuf1);
            }

            m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);
        }
    }
    else
    {
        m_RenderPassFullGBufferWithClear.BeginPass(cmdBuf1, renderArea);
        m_RenderPassFullGBufferWithClear.EndPass(cmdBuf1);
        m_RenderPassJustDepthAndHdr.BeginPass(cmdBuf1, renderArea);
        m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);
    }

    VkImageMemoryBarrier barrier[1] = {};
    barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier[0].pNext = NULL;
    barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier[0].subresourceRange.baseMipLevel = 0;
    barrier[0].subresourceRange.levelCount = 1;
    barrier[0].subresourceRange.baseArrayLayer = 0;
    barrier[0].subresourceRange.layerCount = 1;
    barrier[0].image = m_GBuffer.m_HDR.Resource();
    vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, barrier);

    SetPerfMarkerEnd(cmdBuf1);

    // Post proc---------------------------------------------------------------------------

    // Bloom, takes HDR as input and applies bloom to it.
    {
        SetPerfMarkerBegin(cmdBuf1, "PostProcess");
        
        // Downsample pass
        m_DownSample.Draw(cmdBuf1);
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Downsample");

        // Bloom pass (needs the downsampled data)
        m_Bloom.Draw(cmdBuf1);
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Bloom");

        SetPerfMarkerEnd(cmdBuf1);
    }

    // Apply TAA & Sharpen to m_HDR
    if (pState->bUseTAA)
    {
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            
            VkImageMemoryBarrier barriers[3];
            barriers[0] = barrier;
            barriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barriers[0].image = m_GBuffer.m_DepthBuffer.Resource();

            barriers[1] = barrier;
            barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barriers[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[1].image = m_GBuffer.m_MotionVectors.Resource();

            // no layout transition but we still need to wait
            barriers[2] = barrier;
            barriers[2].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[2].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[2].image = m_GBuffer.m_HDR.Resource();

            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 3, barriers);
        }

        m_TAA.Draw(cmdBuf1);
        m_GPUTimer.GetTimeStamp(cmdBuf1, "TAA");
    }


    // Magnifier Pass: m_HDR as input, pass' own output
    if (pState->bUseMagnifier)
    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = NULL;
        barrier.srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.image = m_MagnifierPS.GetPassOutputResource();

        if (m_bMagResourceReInit)
        {
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            m_bMagResourceReInit = false;
        }
        else
        {
            barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        // Note: assumes the input texture (specified in OnCreateWindowSizeDependentResources()) is in read state
        m_MagnifierPS.Draw(cmdBuf1, pState->MagnifierParams);
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Magnifier");
    }


    // Start tracking input/output resources at this point to handle HDR and SDR render paths 
    VkImage      ImgCurrentInput  = pState->bUseMagnifier ? m_MagnifierPS.GetPassOutputResource() : m_GBuffer.m_HDR.Resource();
    VkImageView  SRVCurrentInput  = pState->bUseMagnifier ? m_MagnifierPS.GetPassOutputSRV()      : m_GBuffer.m_HDRSRV;

    // If using FreeSync HDR, we need to do these in order: Tonemapping -> GUI -> Color Conversion
    const bool bHDR = pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR;
    if (bHDR)
    {
        // In place Tonemapping ------------------------------------------------------------------------
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = ImgCurrentInput;
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            
            m_ToneMappingCS.Draw(cmdBuf1, SRVCurrentInput, pState->Exposure, pState->SelectedTonemapperIndex, m_Width, m_Height);

            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.image = ImgCurrentInput;
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            
        }

        // Render HUD  ------------------------------------------------------------------------
        {

            if (pState->bUseMagnifier)
            {
                m_MagnifierPS.BeginPass(cmdBuf1, renderArea);
            }
            else
            {
                m_RenderPassJustDepthAndHdr.BeginPass(cmdBuf1, renderArea);
            }

            vkCmdSetScissor(cmdBuf1, 0, 1, &m_RectScissor);
            vkCmdSetViewport(cmdBuf1, 0, 1, &m_Viewport);

            m_ImGUI.Draw(cmdBuf1);

            if (pState->bUseMagnifier)
            {
                m_MagnifierPS.EndPass(cmdBuf1);
            }
            else
            {
                m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);
            }

            if (bHDR && !pState->bUseMagnifier)
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = NULL;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.image = ImgCurrentInput;
                vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            }

            m_GPUTimer.GetTimeStamp(cmdBuf1, "ImGUI Rendering");
        }
    }

    // submit command buffer
    {
        VkResult res = vkEndCommandBuffer(cmdBuf1);
        assert(res == VK_SUCCESS);

        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = NULL;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = NULL;
        submit_info.pWaitDstStageMask = NULL;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmdBuf1;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = NULL;
        res = vkQueueSubmit(m_pDevice->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
        assert(res == VK_SUCCESS);
    }

    // Wait for swapchain (we are going to render to it) -----------------------------------
    int imageIndex = pSwapChain->WaitForSwapChain();

    // Keep tracking input/output resource views 
    ImgCurrentInput = pState->bUseMagnifier ? m_MagnifierPS.GetPassOutputResource() : m_GBuffer.m_HDR.Resource(); // these haven't changed, re-assign as sanity check
    SRVCurrentInput = pState->bUseMagnifier ? m_MagnifierPS.GetPassOutputSRV()      : m_GBuffer.m_HDRSRV;         // these haven't changed, re-assign as sanity check

    m_CommandListRing.OnBeginFrame();

    VkCommandBuffer cmdBuf2 = m_CommandListRing.GetNewCommandList();

    {
        VkCommandBufferBeginInfo cmd_buf_info;
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmd_buf_info.pInheritanceInfo = NULL;
        VkResult res = vkBeginCommandBuffer(cmdBuf2, &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }

    SetPerfMarkerBegin(cmdBuf2, "Swapchain RenderPass");

    // prepare render pass
    {
        VkRenderPassBeginInfo rp_begin = {};
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.pNext = NULL;
        rp_begin.renderPass = pSwapChain->GetRenderPass();
        rp_begin.framebuffer = pSwapChain->GetFramebuffer(imageIndex);
        rp_begin.renderArea.offset.x = 0;
        rp_begin.renderArea.offset.y = 0;
        rp_begin.renderArea.extent.width = m_Width;
        rp_begin.renderArea.extent.height = m_Height;
        rp_begin.clearValueCount = 0;
        rp_begin.pClearValues = NULL;
        vkCmdBeginRenderPass(cmdBuf2, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    }

    vkCmdSetScissor(cmdBuf2, 0, 1, &m_RectScissor);
    vkCmdSetViewport(cmdBuf2, 0, 1, &m_Viewport);

    if (bHDR)
    {
        m_ColorConversionPS.Draw(cmdBuf2, SRVCurrentInput);
        m_GPUTimer.GetTimeStamp(cmdBuf2, "Color Conversion");
    }
    
    // For SDR pipeline, we apply the tonemapping and then draw the GUI and skip the color conversion
    else
    {
        // Tonemapping ------------------------------------------------------------------------
        {
            m_ToneMappingPS.Draw(cmdBuf2, SRVCurrentInput, pState->Exposure, pState->SelectedTonemapperIndex);
            m_GPUTimer.GetTimeStamp(cmdBuf2, "Tonemapping");
        }

        // Render HUD  -------------------------------------------------------------------------
        {
            m_ImGUI.Draw(cmdBuf2);
            m_GPUTimer.GetTimeStamp(cmdBuf2, "ImGUI Rendering");
        }
    }

    SetPerfMarkerEnd(cmdBuf2);

    m_GPUTimer.OnEndFrame();

    vkCmdEndRenderPass(cmdBuf2);
   
    // Close & Submit the command list ----------------------------------------------------
    {
        VkResult res = vkEndCommandBuffer(cmdBuf2);
        assert(res == VK_SUCCESS);

        VkSemaphore ImageAvailableSemaphore;
        VkSemaphore RenderFinishedSemaphores;
        VkFence CmdBufExecutedFences;
        pSwapChain->GetSemaphores(&ImageAvailableSemaphore, &RenderFinishedSemaphores, &CmdBufExecutedFences);

        VkPipelineStageFlags submitWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info2;
        submit_info2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info2.pNext = NULL;
        submit_info2.waitSemaphoreCount = 1;
        submit_info2.pWaitSemaphores = &ImageAvailableSemaphore;
        submit_info2.pWaitDstStageMask = &submitWaitStage;
        submit_info2.commandBufferCount = 1;
        submit_info2.pCommandBuffers = &cmdBuf2;
        submit_info2.signalSemaphoreCount = 1;
        submit_info2.pSignalSemaphores = &RenderFinishedSemaphores;

        res = vkQueueSubmit(m_pDevice->GetGraphicsQueue(), 1, &submit_info2, CmdBufExecutedFences);
        assert(res == VK_SUCCESS);
    }
}
