/*     Copyright 2015-2017 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

//  ---------------------------------------------------------------------------
// This file is based on TwSimpleDX11.cpp by Philippe Decaudin
//
//  @file       MengerSponge.cpp
//
//  @brief      Example that uses AntTweakBar with DirectX11.
//
//              It draws a Menger sponge, aka Sierpinski cube:
//              http://en.wikipedia.org/wiki/Menger_sponge .
//
//              Cubes shading is augmented with some simple ambient occlusion
//              applied by subdividing each cube face into a 3x3 grid.
//              AntTweakBar is used to add some interacitve controls.
//
//              Note that most of the code here is related to DirectX and
//              Menger sponge generation. AntTweakBar calls are localized
//              in the WinMain function.
//
//              AntTweakBar: http://anttweakbar.sourceforge.net/doc
//              DirectX:     http://msdn.microsoft.com/directx
//  
//
//  ---------------------------------------------------------------------------
#include "pch.h"

#define TW_STATIC
#include "AntTweakBar.h"

#include <cmath>
#include <vector>
#include "MengerSpongeSample.h"
#include "MapHelper.h"
#include "ConvenienceFunctions.h"

using namespace Diligent;

SampleBase* CreateSample(IRenderDevice *pDevice, IDeviceContext *pImmediateContext, ISwapChain *pSwapChain)
{
    return new MengerSpongeSample( pDevice, pImmediateContext, pSwapChain );
}


const float FLOAT_PI = 3.14159265f;
struct Vertex
{
    float3 Position;
    float3 Normal;
    unsigned int AmbientColor;
};
struct ShaderConstants
{
    float4x4 WorldViewProjT;
    float4x4 WorldNormT;
    float3 LightDir;
    float LightCoeff;
};
// Each cube face is split into a 3x3 grid
const int CUBE_FACE_VERTEX_COUNT = 4 * 4;       // 16 vertices per face
const int CUBE_FACE_TRIANGLE_COUNT = 2 * 3 * 3; // 18 triangles to be drawn for each face
// Faces color of the sponge wrt to recursion level
const unsigned int COLORS[] = { 0xffffffff, 0xff007fff, 0xff7fff00, 0xffff007f, 0xff0000ff, 0xff00ff00, 0xffff0000 };


// Callback function called by AntTweakBar to set the sponge recursion level
void MengerSpongeSample::SetSpongeLevelCB(const void *value, void * clientData)
{
    MengerSpongeSample *pTheSample = reinterpret_cast<MengerSpongeSample*>( clientData );
    pTheSample->m_SpongeLevel = *static_cast<const int *>(value);
    pTheSample->BuildSponge(pTheSample->m_SpongeLevel, pTheSample->m_SpongeAO);
}


// Callback function called by AntTweakBar to get the sponge recursion level
void MengerSpongeSample::GetSpongeLevelCB(void *value, void * clientData)
{
    MengerSpongeSample *pTheSample = reinterpret_cast<MengerSpongeSample*>( clientData );
    *static_cast<int *>(value) = pTheSample->m_SpongeLevel;
}


// Callback function called by AntTweakBar to enable/disable ambient occlusion
void MengerSpongeSample::SetSpongeAOCB(const void *value, void * clientData)
{
    MengerSpongeSample *pTheSample = reinterpret_cast<MengerSpongeSample*>( clientData );
    pTheSample->m_SpongeAO = *static_cast<const bool *>(value);
    pTheSample->BuildSponge(pTheSample->m_SpongeLevel, pTheSample->m_SpongeAO);
}


// Callback function called by AntTweakBar to get ambient occlusion state
void MengerSpongeSample::GetSpongeAOCB(void *value, void * clientData)
{
    MengerSpongeSample *pTheSample = reinterpret_cast<MengerSpongeSample*>( clientData );
    *static_cast<bool *>(value) = pTheSample->m_SpongeAO;
}

MengerSpongeSample::MengerSpongeSample(IRenderDevice *pDevice, IDeviceContext *pImmediateContext, ISwapChain *pSwapChain) : 
    SampleBase(pDevice, pImmediateContext, pSwapChain)
{
    m_SpongeLevel = 2;                       // number of recursions
    m_SpongeAO = true;                      // apply ambient occlusion
    m_LightDir[0] = -0.5f;
    m_LightDir[1] = -0.2f;
    m_LightDir[2] = 1;
    m_CamDistance = 0.7f;                  // camera distance
    m_BackgroundColor[0] = 0;
    m_BackgroundColor[1] = 0;
    m_BackgroundColor[2] = 0.5f;
    m_BackgroundColor[3] = 1;
    m_Animate = false;                       // enable animation
    m_AnimationSpeed = 0.2f;               // animation speed

#if defined(PLATFORM_UNIVERSAL_WINDOWS)
    FileSystem::SetWorkingDirectory("assets");
#endif

    BufferDesc BuffDesc;
    BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
    BuffDesc.Usage = USAGE_DYNAMIC;
    BuffDesc.uiSizeInBytes = sizeof(ShaderConstants);
    BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer( BuffDesc, BufferData(), &m_pConstantBuffer );

    m_pRenderScript = CreateRenderScriptFromFile( "MengerSponge.lua", m_pDevice, m_pDeviceContext, [&]( ScriptParser *pScriptParser )
    {
        pScriptParser->SetGlobalVariable( "extConstantBuffer", m_pConstantBuffer );
    } );

    // Create vertex and index buffers
    BuildSponge(m_SpongeLevel, m_SpongeAO);

    // Init model rotation
    float3 axis(-1, 1, 0);
    m_SpongeRotation = RotationFromAxisAngle(axis, FLOAT_PI/4);

    // Create a tweak bar
    TwBar *bar = TwNewBar("TweakBar");
    TwDefine(" GLOBAL help='This example shows how to integrate AntTweakBar into a DirectX11 application.' "); // Message added to the help bar.
    int barSize[2] = {224, 320};
#ifdef ANDROID
    barSize[0] *= 3;
    barSize[1] *= 3;
#endif
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    // Add variables to the tweak bar
    TwAddVarCB(bar, "Level", TW_TYPE_INT32, SetSpongeLevelCB, GetSpongeLevelCB, this, "min=0 max=3 group=Sponge keyincr=l keydecr=L");
    TwAddVarCB(bar, "Ambient Occlusion", TW_TYPE_BOOLCPP, SetSpongeAOCB, GetSpongeAOCB, this, "group=Sponge key=o");
    TwAddVarRW(bar, "Rotation", TW_TYPE_QUAT4F, &m_SpongeRotation, "opened=true axisz=-z group=Sponge");
    TwAddVarRW(bar, "Animation", TW_TYPE_BOOLCPP, &m_Animate, "group=Sponge key=a");
    TwAddVarRW(bar, "Animation speed", TW_TYPE_FLOAT, &m_AnimationSpeed, "min=-10 max=10 step=0.1 group=Sponge keyincr=+ keydecr=-");
    TwAddVarRW(bar, "Light direction", TW_TYPE_DIR3F, &m_LightDir, "opened=true axisz=-z showval=false");
    TwAddVarRW(bar, "Camera distance", TW_TYPE_FLOAT, &m_CamDistance, "min=0 max=4 step=0.01 keyincr=PGUP keydecr=PGDOWN");
    TwAddVarRW(bar, "Background", TW_TYPE_COLOR4F, &m_BackgroundColor, "colormode=hls");
}

MengerSpongeSample::~MengerSpongeSample()
{
}

// Append vertices and indices of a cube to the index and vertex buffers.
// The cube has gradient ambient-occlusion defined per edge.
void AppendCubeToBuffers(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                         const float4x4& xform, float aoRatio, const bool aoEdges[12], 
                         const unsigned int faceColors[6])
{
    unsigned int indicesOffset = (unsigned int)vertices.size();

    // Fill vertex buffer

    // Cube faces and edges numbering:
    //       __________           _____6____ 
    //      /         /|         /|        /|
    //     /    4    / |<2     10 5       9 |
    //    /_________/  |       /__|__2___/  7
    //    |         | 1|       |  |___4__|__|
    //  3>|    0    |  /       3  /      1  /
    //    |         | /        | 11      | 8 
    //    |_________|/         |/____0___|/  
    //         5^
    // Each face is split in a 3x3 grid, which gives 16 vertices per face and 3x3x2(=18) triangles per face.
    // Ambient occlusion color is set for each of these vertices wrt aoEdges flags.

    const float R = 0.5f; // unit cube radius
    // the 4 corner coordinates for each of the 6 faces
    const float3 A[6] = { float3(-R, -R, -R), float3(+R, -R, -R), float3(+R, -R, +R), float3(-R, -R, +R), float3(-R, +R, -R), float3(-R, -R, -R) }; 
    const float3 B[6] = { float3(+R, -R, -R), float3(+R, -R, +R), float3(-R, -R, +R), float3(-R, -R, -R), float3(+R, +R, -R), float3(+R, -R, -R) }; 
    const float3 C[6] = { float3(-R, +R, -R), float3(+R, +R, -R), float3(+R, +R, +R), float3(-R, +R, +R), float3(-R, +R, +R), float3(-R, -R, +R) }; 
    const float3 D[6] = { float3(+R, +R, -R), float3(+R, +R, +R), float3(-R, +R, +R), float3(-R, +R, -R), float3(+R, +R, +R), float3(+R, -R, +R) };
    // the 6 face normals
    const float3 N[6] = { float3( 0,  0, -1), float3(+1,  0,  0), float3( 0,  0, +1), float3(-1,  0,  0), float3( 0, +1,  0), float3( 0, -1,  0) };
    // association between edge indices and the 6 faces
    const int E[6][4] = { {0, 1, 2, 3}, {8, 7, 9, 1}, {4, 5, 6, 7}, {11, 3, 10, 5}, {2, 9, 6, 10}, {0, 8, 4, 11} }; 
    
    int face, i, j;
    float u, v;
    bool ao;
    Vertex vertex;
    for (face = 0; face < 6; face++)
        for (j = 0; j < 4; j++)
        {
            v = (j == 1) ? aoRatio : ((j == 2) ? 1.0f - aoRatio : j/3);
            for (i = 0; i < 4; i++)
            {
                u = (i == 1) ? aoRatio : ((i == 2) ? 1.0f - aoRatio : i/3);
                
                vertex.Position = (1.0f - v) * ((1.0f - u) * A[face] + u * B[face]) 
                                  + v * ((1.0f - u) * C[face] + u * D[face]);
                vertex.Position = vertex.Position * xform;

                vertex.Normal = N[face];

                ao  = (j == 0) && aoEdges[E[face][0]];
                ao |= (i == 3) && aoEdges[E[face][1]];
                ao |= (j == 3) && aoEdges[E[face][2]];
                ao |= (i == 0) && aoEdges[E[face][3]];

                #define DARKEN(r, s) ( (unsigned int)(float(r)*(s)) > 255 ? 255 : (unsigned int)(float(r)*(s)) )
                #define DARKEN_COLOR(c, s) ( 0xff000000 | (DARKEN(((c)>>16)&0xff, s)<<16) | (DARKEN(((c)>>8)&0xff, s)<<8) | DARKEN((c)&0xff, s) )
                vertex.AmbientColor = ao ? DARKEN_COLOR(faceColors[face], 0.75f) : faceColors[face];

                vertices.push_back(vertex);
            }
        }

    // Fill index buffer

    // 3 indices per triangle, 2*3*3 triangles per faces, 6 faces.
    // Vertex index numbering of each face:
    //    12__13__14___15
    //     |'. | .'| .'|
    //     8__'9'_10'__11
    //     | .'| .'| .'|
    //     4'__5'__6'__7
    //     | .'| .'|'. |
    //     0'__1'__2__'3
  
    const unsigned short I[CUBE_FACE_TRIANGLE_COUNT][3] = 
    { 
        {0, 5, 4}, {0, 1, 5},  {1, 6, 5}, {1, 2, 6},  {3, 6, 2}, {3, 7, 6},
        {4, 9, 8}, {4, 5, 9},  {5, 10, 9}, {5, 6, 10},  {6, 11, 10}, {6, 7, 11},
        {8, 9, 12}, {9, 13, 12},  {9, 14, 13}, {9, 10, 14},  {10, 15, 14}, {10, 11, 15} 
    };
    int tri;
    for (face = 0; face < 6; face++)
        for (tri = 0; tri < CUBE_FACE_TRIANGLE_COUNT; tri++)
            for (i = 0; i < 3; i++) 
                indices.push_back(indicesOffset + I[tri][i] + 16*face); // 16 vertices per face
}


// Recursive function called to fill the vertex and index buffers with the cubes forming the Menger sponge.
void FillSpongeBuffers(int level, int levelMax, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, 
                       const float3& center, bool aoEnabled, const bool aoEdges[12], const unsigned int faceColors[6])
{
    float scale = pow(1.0f/3.0f, level);

    if (level == levelMax)
    {
        float aoRatio = pow(3.0f, level) * 0.02f;
        if (aoRatio > 0.4999f)
            aoRatio = 0.4999f;
        float4x4 xform = scaleMatrix( scale, scale, scale ) * translationMatrix( center );
        AppendCubeToBuffers(vertices, indices, xform, aoRatio, aoEdges, faceColors);
    }
    else
    {
        // Local function which applies AO in one direction
        struct Local
        {
            static void ApplyAO(int i, int j, bool& e0, bool& e1, bool& e2, bool& e3)
            {
                if (i == -1 && j == 0) e0 = e1 = true;
                if (i == +1 && j <= 0) e1 = false;
                if (i == +1 && j >= 0) e0 = false;

                if (i == +1 && j == 0) e2 = e3 = true;
                if (i == -1 && j <= 0) e2 = false;
                if (i == -1 && j >= 0) e3 = false;

                if (j == -1 && i == 0) e1 = e2 = true;
                if (j == +1 && i <= 0) e1 = false;
                if (j == +1 && i >= 0) e2 = false;

                if (j == +1 && i == 0) e0 = e3 = true;
                if (j == -1 && i <= 0) e0 = false;
                if (j == -1 && i >= 0) e3 = false;
            }
        };

        bool aoEdgesCopy[12];
        unsigned int faceColorsCopy[6];
        int i, j, k, l;
        for (i = -1; i <= 1; i++)
            for (j = -1; j <= 1; j++)
                for (k = -1; k <= 1; k++)
                    if ( !( (i == 0 && j == 0) || (i == 0 && k == 0) || (j == 0 && k == 0) ) )
                    {
                        float s = 1.0f/3.0f * scale;
                        float3 t ( center[0] + s * i, center[1] + s * j, center[2] + s * k );

                        for (l = 0; l < 12; l++)
                            aoEdgesCopy[l] = aoEdges[l];
                        if (aoEnabled)
                        {
                            Local::ApplyAO( i, j, aoEdgesCopy[8], aoEdgesCopy[9], aoEdgesCopy[10], aoEdgesCopy[11]); // z direction
                            Local::ApplyAO( i, k, aoEdgesCopy[1], aoEdgesCopy[7], aoEdgesCopy[5],  aoEdgesCopy[3] ); // y direction
                            Local::ApplyAO(-k, j, aoEdgesCopy[0], aoEdgesCopy[2], aoEdgesCopy[6],  aoEdgesCopy[4] ); // x direction
                        }

                        for (l = 0; l < 6; l++)
                            faceColorsCopy[l] = faceColors[l];
                        if (k == +1) faceColorsCopy[0] = COLORS[level+1];
                        if (i == -1) faceColorsCopy[1] = COLORS[level+1];
                        if (k == -1) faceColorsCopy[2] = COLORS[level+1];
                        if (i == +1) faceColorsCopy[3] = COLORS[level+1];
                        if (j == -1) faceColorsCopy[4] = COLORS[level+1];
                        if (j == +1) faceColorsCopy[5] = COLORS[level+1];

                        FillSpongeBuffers(level + 1, levelMax, vertices, indices, t, aoEnabled, aoEdgesCopy, faceColorsCopy);
                    }
    }
}


// Build sponge vertex and index buffers 
void MengerSpongeSample::BuildSponge(int levelMax, bool aoEnabled)
{
    // Fill vertex and index memory buffers
    static std::vector<Vertex> vertices;
    static std::vector<unsigned int> indices;
    vertices.clear();
    indices.clear();
    bool aoEdges[12] = { false, false, false, false, false, false, false, false, false, false, false, false };
    unsigned int faceColors[6] = { COLORS[0], COLORS[0], COLORS[0], COLORS[0], COLORS[0], COLORS[0] };
    FillSpongeBuffers(0, levelMax, vertices, indices, float3(), aoEnabled, aoEdges, faceColors);

    BufferDesc BuffDesc;
    BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    BuffDesc.Usage = USAGE_STATIC;
    BuffDesc.uiSizeInBytes = (Uint32)vertices.size() * sizeof(Vertex);
    BufferData BuffData;
    BuffData.pData = vertices.data();
    BuffData.DataSize = BuffDesc.uiSizeInBytes;

    RefCntAutoPtr<IBuffer> pVertexBuffer;
    m_pDevice->CreateBuffer( BuffDesc, BuffData, &pVertexBuffer );

    // Create index buffer
    BuffDesc.uiSizeInBytes = (Uint32)indices.size() * sizeof(unsigned int);
    BuffDesc.BindFlags = BIND_INDEX_BUFFER;
    BuffData.pData = &indices[0];
    BuffData.DataSize = BuffDesc.uiSizeInBytes;

    RefCntAutoPtr<IBuffer> pIndexBuffer;
    m_pDevice->CreateBuffer( BuffDesc, BuffData, &pIndexBuffer );

    auto SpongeIndicesCount = (unsigned int)indices.size();

    m_pRenderScript->SetGlobalVariable( "extSpongeVB", pVertexBuffer );
    m_pRenderScript->SetGlobalVariable( "extSpongeIB", pIndexBuffer );
    m_pRenderScript->Run( "SetNumIndices", SpongeIndicesCount );
}



// Copy world/view/proj matrices and light parameters to shader constants
void MengerSpongeSample::SetShaderConstants(const float4x4& world, const float4x4& view, const float4x4& proj)
{
    MapHelper<ShaderConstants> MappedData( m_pDeviceContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD );
    ShaderConstants *cst = MappedData;
    cst->WorldViewProjT = transposeMatrix( world * view * proj );
    cst->WorldNormT = transposeMatrix( world );
    cst->LightDir = (1.0f / length(m_LightDir)) * m_LightDir;
    cst->LightCoeff = 0.85f;
}


// Render a frame
void MengerSpongeSample::Render()
{
    // Clear the back buffer 
    m_pDeviceContext->ClearRenderTarget(nullptr, m_BackgroundColor);
    m_pDeviceContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f);

    const auto &DeviceCaps = m_pDevice->GetDeviceCaps();

    const auto& SCDesc = m_pSwapChain->GetDesc();
    // Set world/view/proj matrices and global shader constants
    float aspectRatio = (float)SCDesc.Width / SCDesc.Height;
    float4x4 proj = Projection(FLOAT_PI/4, aspectRatio, 0.1f, 100.0f, DeviceCaps.DevType == DeviceType::D3D11 || DeviceCaps.DevType == DeviceType::D3D12);
    float dist = m_CamDistance + 0.4f;
    float3 camPosInv ( dist * 0.3f, dist * 0.0f, dist * 2.0f );
    float4x4 view = translationMatrix(camPosInv);
    float4x4 world = QuaternionToMatrix(m_SpongeRotation);
    SetShaderConstants(world, view, proj);

    // Draw the sponge
    m_pRenderScript->Run( m_pDeviceContext, "Draw" );
}


// Rotating sponge
void MengerSpongeSample::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    float dt = (float)ElapsedTime;
    if (m_Animate && dt > 0 && dt < 0.2f)
    {
        float3 axis;
        float angle = 0;
        AxisAngleFromRotation(axis, angle, m_SpongeRotation);
        if (length(axis) < 1.0e-6f) 
            axis[1] = 1;
        angle += m_AnimationSpeed * dt;
        if (angle >= 2.0f*FLOAT_PI)
            angle -= 2.0f*FLOAT_PI;
        else if (angle <= 0)
            angle += 2.0f*FLOAT_PI;
        m_SpongeRotation = RotationFromAxisAngle(axis, angle);
    }
}
