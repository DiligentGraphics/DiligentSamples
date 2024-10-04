/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp"
#include "octree/octree.h"
#include <AdvancedMath.hpp>


namespace Diligent
{
    class Tutorial20_MeshShader final : public SampleBase
    {
    public:
        virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;
        virtual void Initialize(const SampleInitInfo& InitInfo) override final;
    
        virtual void Render() override final;
        virtual void Update(double CurrTime, double ElapsedTime) override final;
    
        virtual const Char* GetSampleName() const override final { return "Tutorial20: Mesh shader"; }
    
        ~Tutorial20_MeshShader();
    
    private:
        void GetPointCloudFromMesh(std::string meshPath);
        
        void CreateDrawTasksFromLoadedMesh();
        void CreateDrawTasks();
        
        void CreatePipelineState();        
        void CreateSortedIndexBuffer(std::vector<int>& sortedNodeBuffer);
        void CreateGPUOctreeNodeBuffer(std::vector<VoxelOC::GPUOctreeNode>& octreeNodeBuffer);
        void CreateStatisticsBuffer();
        void CreateConstantsBuffer();

        void LoadTexture();
        void UpdateUI();

        void PreWindowResize() override;
        void WindowResize(Uint32 Width, Uint32 Height) override;

        // 2 Pass Depth OC
        void                   CreateDepthBuffers();
    
        RefCntAutoPtr<IBuffer>      m_CubeBuffer;
        RefCntAutoPtr<ITextureView> m_CubeTextureSRV;
    
        RefCntAutoPtr<IBuffer> m_pStatisticsBuffer;
        RefCntAutoPtr<IBuffer> m_pStatisticsStaging;
        RefCntAutoPtr<IFence>  m_pStatisticsAvailable;
        Uint64                 m_FrameId               = 1; // Can't signal 0
        const Uint32           m_StatisticsHistorySize = 8;
    
        static constexpr Int32 ASGroupSize = 32;
    
        Uint32                 m_DrawTaskCount = 0;
        Uint32                 m_DrawTaskPadding = 0;

        RefCntAutoPtr<IBuffer> m_pDrawTasks;
        RefCntAutoPtr<IBuffer> m_pGridIndices;
        RefCntAutoPtr<IBuffer> m_pOctreeNodes;
        RefCntAutoPtr<IBuffer> m_pConstants;
        RefCntAutoPtr<ITexture>     m_pDepthBufferCpy;
        RefCntAutoPtr<ITextureView> m_pDepthBufferCpySRV;
        RefCntAutoPtr<ITextureView> m_pDepthBufferCpyUAV;

        RefCntAutoPtr<ITexture>     m_pPrevDepthBuffer;
        RefCntAutoPtr<ITextureView> m_pPrevDepthBufferSRV;

    

        RefCntAutoPtr<IPipelineState>         m_pPSO;
        RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    
        FirstPersonCamera fpc{};
        ViewFrustum       Frustum{};


        float4x4    m_ViewProjMatrix;
        float4x4    m_ViewMatrix;
        float       m_RotationAngle  = 0;
        bool        m_MSDebugViz     = false;
        bool        m_OTDebugViz     = false;
        bool        m_FrustumCulling = true;
        bool        m_OcclusionCulling = true;
        bool        m_SyncCamPosition  = true;
        const float m_FOV            = PI_F / 4.0f;
        const float m_CoTanHalfFov   = 1.0f / std::tan(m_FOV * 0.5f);
        float       m_LodScale       = 4.0f;
        float       m_CameraHeight   = 10.0f;
        float       m_CurrTime       = 0.0f;
        Uint32      m_VisibleCubes   = 0;
        Uint32      m_VisibleOTNodes = 0;
    
        OctreeNode<VoxelOC::DrawTask>* p_occlusionOctreeRoot = nullptr;
    };

} // namespace Diligent
