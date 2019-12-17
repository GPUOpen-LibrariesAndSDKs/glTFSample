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
#pragma once

// We are queuing (2 backbuffers + 0.5) frames, so we need to triple buffer the command lists
static const int backBufferCount = 3;

#define USE_VID_MEM true

using namespace CAULDRON_DX12;

//
// This class deals with the GPU side of the sample.
//
class SampleRenderer
{
public:
    struct Spotlight
    {
        Camera light;
        XMVECTOR color;
        float intensity;
    };

    struct State
    {
        float time;
        Camera camera;
        
        float exposure;
        float iblFactor;
        float emmisiveFactor;
        
        int   toneMapper;
        int   skyDomeType;
        bool  bDrawBoundingBoxes;

        uint32_t  spotlightCount;
        Spotlight spotlight[4];
        bool  bDrawLightFrustum;
    };

    void OnCreate(Device* pDevice, SwapChain *pSwapChain);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height);
    void OnDestroyWindowSizeDependentResources();

    int LoadScene(GLTFCommon *pGLTFCommon, int stage = 0);
    void UnloadScene();

    const std::vector<TimeStamp> &GetTimingValues() { return m_TimeStamps; }

    void OnRender(State *pState, SwapChain *pSwapChain);

private:
    Device                         *m_pDevice;

    uint32_t                        m_Width;
    uint32_t                        m_Height;

    D3D12_VIEWPORT                  m_viewport;
    D3D12_RECT                      m_rectScissor;
    
	// Initialize helper classes
	ResourceViewHeaps               m_resourceViewHeaps;
	UploadHeap                      m_UploadHeap;
	DynamicBufferRing               m_ConstantBufferRing;
	StaticBufferPool                m_VidMemBufferPool;
	CommandListRing                 m_CommandListRing;
	GPUTimestamps                   m_GPUTimer;


	//gltf passes
	GLTFTexturesAndBuffers         *m_pGLTFTexturesAndBuffers;
	GltfPbrPass                    *m_gltfPBR;
	GltfDepthPass                  *m_gltfDepth;
	GltfBBoxPass                   *m_gltfBBox;

	// effects
    Bloom                           m_bloom;
    SkyDome                         m_skyDome;
    DownSamplePS                    m_downSample;
    SkyDomeProc                     m_skyDomeProc;
	ToneMapping                     m_toneMappingPS;

	// GUI
	ImGUI                           m_ImGUI;

	// Temporary render targets

	// depth buffer
	Texture                         m_depthBuffer;
    DSV                             m_depthBufferDSV;

    // shadowmaps
	Texture                         m_shadowMap;
    CBV_SRV_UAV                     m_ShadowMapSRV;
    DSV                             m_ShadowMapDSV;

    // MSAA RT
    Texture                         m_HDRMSAA;
    RTV                             m_HDRRTVMSAA;

    // Resolved RT
    Texture                         m_HDR;
    CBV_SRV_UAV                     m_HDRSRV;
    CBV_SRV_UAV                     m_HDRUAV;
    RTV                             m_HDRRTV;

	// widgets
    Wireframe                       m_wireframe;
	WireframeBox                    m_wireframeBox;

	std::vector<TimeStamp>          m_TimeStamps;
};

