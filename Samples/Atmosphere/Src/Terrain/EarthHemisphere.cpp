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

// This file is derived from the open source project provided by Intel Corportaion that
// requires the following notice to be kept:
//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "EarthHemisphere.h"

#include "Structures.fxh"
#include "ElevationDataSource.h"
#include "ConvenienceFunctions.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"
#include "BasicShaderSourceStreamFactory.h"
#include "ShaderMacroHelper.h"
#include "TextureUtilities.h"

using namespace Diligent;
using namespace Diligent;
using namespace Diligent;


struct HemisphereVertex
{
    float3 f3WorldPos;
    float2 f2MaskUV0;
    HemisphereVertex() : f3WorldPos(0,0,0), f2MaskUV0(0,0){}
};

enum QUAD_TRIANGULATION_TYPE
{
    QUAD_TRIANG_TYPE_UNDEFINED = 0,

    // 01      11
    //  *------*
    //  |   .' |
    //  | .'   |
    //  * -----*
    // 00      10
    QUAD_TRIANG_TYPE_00_TO_11,

    // 01      11
    //  *------*
    //  | '.   |
    //  |   '. |
    //  * -----*
    // 00      10
    QUAD_TRIANG_TYPE_01_TO_10
};

template<typename IndexType, class IndexGenerator>
class TriStrip
{
public:
    TriStrip(std::vector<IndexType> &Indices, IndexGenerator indexGenerator):
        m_Indices(Indices),
        m_IndexGenerator(indexGenerator),
        m_QuadTriangType(QUAD_TRIANG_TYPE_UNDEFINED)
    {
    }
    
    void AddStrip(int iBaseIndex,
                  int iStartCol,
                  int iStartRow,
                  int iNumCols,
                  int iNumRows,
                  QUAD_TRIANGULATION_TYPE QuadTriangType)
    {
        VERIFY_EXPR( QuadTriangType == QUAD_TRIANG_TYPE_00_TO_11 || QuadTriangType == QUAD_TRIANG_TYPE_01_TO_10 );
        int iFirstVertex = iBaseIndex + m_IndexGenerator(iStartCol, iStartRow + (QuadTriangType==QUAD_TRIANG_TYPE_00_TO_11 ? 1:0));
        if(m_QuadTriangType != QUAD_TRIANG_TYPE_UNDEFINED)
        {
            // To move from one strip to another, we have to generate two degenerate triangles
            // by duplicating the last vertex in previous strip and the first vertex in new strip
            m_Indices.push_back( m_Indices.back() );
            m_Indices.push_back( iFirstVertex );
        }

        if(m_QuadTriangType != QUAD_TRIANG_TYPE_UNDEFINED && m_QuadTriangType != QuadTriangType || 
           m_QuadTriangType == QUAD_TRIANG_TYPE_UNDEFINED && QuadTriangType == QUAD_TRIANG_TYPE_01_TO_10)
        {
            // If triangulation orientation changes, or if start strip orientation is 01 to 10, 
            // we also have to add one additional vertex to preserve winding order
            m_Indices.push_back( iFirstVertex );
        }
        m_QuadTriangType = QuadTriangType;

        for(int iRow = 0; iRow < iNumRows-1; ++iRow)
        {
            for(int iCol = 0; iCol < iNumCols; ++iCol)
            {
                int iV00 = iBaseIndex + m_IndexGenerator(iStartCol+iCol, iStartRow+iRow);
                int iV01 = iBaseIndex + m_IndexGenerator(iStartCol+iCol, iStartRow+iRow+1);
                if( m_QuadTriangType == QUAD_TRIANG_TYPE_01_TO_10 )
                {
                    if( iCol == 0 && iRow == 0)
                        VERIFY_EXPR(iFirstVertex == iV00)
                    // 01      11
                    //  *------*
                    //  | '.   |
                    //  |   '. |
                    //  * -----*
                    // 00      10
                    m_Indices.push_back(iV00);
                    m_Indices.push_back(iV01);
                }
                else if( m_QuadTriangType == QUAD_TRIANG_TYPE_00_TO_11 )
                {
                    if( iCol == 0 && iRow == 0)
                        VERIFY_EXPR(iFirstVertex == iV01)
                    // 01      11
                    //  *------*
                    //  |   .' |
                    //  | .'   |
                    //  * -----*
                    // 00      10
                    m_Indices.push_back(iV01);
                    m_Indices.push_back(iV00);
                }
                else
                {
                    VERIFY_EXPR(false);
                }
            }
        
            if(iRow < iNumRows-2)
            {
                m_Indices.push_back( m_Indices.back() );
                m_Indices.push_back( iBaseIndex + m_IndexGenerator(iStartCol, iStartRow+iRow+1 + (QuadTriangType==QUAD_TRIANG_TYPE_00_TO_11 ? 1:0)) );
            }
        }
    }

private:
    QUAD_TRIANGULATION_TYPE m_QuadTriangType;
    std::vector<IndexType> &m_Indices;
    IndexGenerator m_IndexGenerator;
};

class StdIndexGenerator
{
public:
    StdIndexGenerator(int iPitch):m_iPitch(iPitch){}
    Uint32 operator ()(int iCol, int iRow){return iCol + iRow*m_iPitch;}

private:
    int m_iPitch;
};

typedef TriStrip<Uint32,  StdIndexGenerator> StdTriStrip32;


void ComputeVertexHeight(HemisphereVertex &Vertex, 
                         class ElevationDataSource *pDataSource,
                         float fSamplingStep,
                         float fSampleScale)
{
    float3 &f3PosWS = Vertex.f3WorldPos;

    float fCol = f3PosWS.x / fSamplingStep;
    float fRow = f3PosWS.z / fSamplingStep;
    float fDispl = pDataSource->GetInterpolatedHeight(fCol, fRow);
    int iColOffset, iRowOffset;
    pDataSource->GetOffsets(iColOffset, iRowOffset);
    Vertex.f2MaskUV0.x = (fCol + (float)iColOffset + 0.5f)/(float)pDataSource->GetNumCols();
    Vertex.f2MaskUV0.y = (fRow + (float)iRowOffset + 0.5f)/(float)pDataSource->GetNumRows();
    
    float3 f3SphereNormal = normalize(f3PosWS);
    f3PosWS += f3SphereNormal * fDispl * fSampleScale;
}
                         

class RingMeshBuilder
{
public:
    RingMeshBuilder(IRenderDevice *pDevice,
                     const std::vector<HemisphereVertex> &VB,
                     int iGridDimenion,
                     std::vector<RingSectorMesh> &RingMeshes) : m_pDevice(pDevice), m_VB(VB), m_iGridDimenion(iGridDimenion), m_RingMeshes(RingMeshes){}

    void CreateMesh(int iBaseIndex, 
                    int iStartCol, 
                    int iStartRow, 
                    int iNumCols, 
                    int iNumRows, 
                    enum QUAD_TRIANGULATION_TYPE QuadTriangType)
    {
        m_RingMeshes.push_back( RingSectorMesh() );
        auto& CurrMesh = m_RingMeshes.back();

        std::vector<Uint32> IB;
        StdTriStrip32 TriStrip( IB, StdIndexGenerator(m_iGridDimenion) );
        TriStrip.AddStrip(iBaseIndex, iStartCol, iStartRow, iNumCols, iNumRows, QuadTriangType);

        CurrMesh.uiNumIndices = (Uint32)IB.size();

        // Prepare buffer description
        BufferDesc IndexBufferDesc;
        IndexBufferDesc.Name = "Ring mesh index buffer";
        IndexBufferDesc.uiSizeInBytes = (Uint32)(IB.size() * sizeof( IB[0] ));
        IndexBufferDesc.BindFlags = BIND_INDEX_BUFFER;
        IndexBufferDesc.Usage = USAGE_STATIC;
        BufferData IBInitData;
        IBInitData.pData = IB.data();
        IBInitData.DataSize = IndexBufferDesc.uiSizeInBytes;
        // Create the buffer
        m_pDevice->CreateBuffer( IndexBufferDesc, IBInitData, &CurrMesh.pIndBuff );
        VERIFY(CurrMesh.pIndBuff, "Failed to create index buffer");

        // Compute bounding box
        auto &BB = CurrMesh.BndBox;
        BB.fMaxX =BB.fMaxY = BB.fMaxZ = -FLT_MAX;
        BB.fMinX =BB.fMinY = BB.fMinZ = +FLT_MAX;
        for(auto Ind = IB.begin(); Ind != IB.end(); ++Ind)
        {
            const auto &CurrVert = m_VB[*Ind].f3WorldPos;
            BB.fMinX = std::min(BB.fMinX, CurrVert.x);
            BB.fMinY = std::min(BB.fMinY, CurrVert.y);
            BB.fMinZ = std::min(BB.fMinZ, CurrVert.z);

            BB.fMaxX = std::max(BB.fMaxX, CurrVert.x);
            BB.fMaxY = std::max(BB.fMaxY, CurrVert.y);
            BB.fMaxZ = std::max(BB.fMaxZ, CurrVert.z);
        }
    }

private:
    Diligent::RefCntAutoPtr<IRenderDevice> m_pDevice;
    std::vector<RingSectorMesh> &m_RingMeshes;
    const std::vector<HemisphereVertex> &m_VB;
    const int m_iGridDimenion;
};


void GenerateSphereGeometry(IRenderDevice *pDevice,
                            const float fEarthRadius,
                            int iGridDimension, 
                            const int iNumRings,
                            class ElevationDataSource *pDataSource,
                            float fSamplingStep,
                            float fSampleScale,
                            std::vector<HemisphereVertex> &VB,
                            std::vector<Uint32> &StitchIB,
                            std::vector<RingSectorMesh> &SphereMeshes)
{
    if( (iGridDimension - 1) % 4 != 0 )
    {
        VERIFY_EXPR(false);
        iGridDimension = RenderingParams().m_iRingDimension;
    }
    const int iGridMidst = (iGridDimension-1)/2;
    const int iGridQuart = (iGridDimension-1)/4;

    const int iLargestGridScale = iGridDimension << (iNumRings-1);
    
    RingMeshBuilder RingMeshBuilder(pDevice, VB, iGridDimension, SphereMeshes);

    int iStartRing = 0;
    VB.reserve( (iNumRings-iStartRing) * iGridDimension * iGridDimension );
    for(int iRing = iStartRing; iRing < iNumRings; ++iRing)
    {
        int iCurrGridStart = (int)VB.size();
        VB.resize(VB.size() + iGridDimension * iGridDimension);
        float fGridScale = 1.f / (float)(1<<(iNumRings-1 - iRing));
        // Fill vertex buffer
        for(int iRow = 0; iRow < iGridDimension; ++iRow)
            for(int iCol = 0; iCol < iGridDimension; ++iCol)
            {
                auto &CurrVert = VB[iCurrGridStart + iCol + iRow*iGridDimension];
                auto &f3Pos = CurrVert.f3WorldPos;
                f3Pos.x = static_cast<float>(iCol) / static_cast<float>(iGridDimension-1);
                f3Pos.z = static_cast<float>(iRow) / static_cast<float>(iGridDimension-1);
                f3Pos.x = f3Pos.x*2 - 1;
                f3Pos.z = f3Pos.z*2 - 1;
                f3Pos.y = 0;
                float fDirectionScale = 1;
                if( f3Pos.x != 0 || f3Pos.z != 0 )
                {
                    float fDX = fabs(f3Pos.x);
                    float fDZ = fabs(f3Pos.z);
                    float fMaxD = std::max(fDX, fDZ);
                    float fMinD = std::min(fDX, fDZ);
                    float fTan = fMinD/fMaxD;
                    fDirectionScale = 1 / sqrt(1 + fTan*fTan);
                }
            
                f3Pos.x *= fDirectionScale*fGridScale;
                f3Pos.z *= fDirectionScale*fGridScale;
                f3Pos.y = sqrt( std::max(0.f, 1.f - (f3Pos.x*f3Pos.x + f3Pos.z*f3Pos.z)) );

                f3Pos.x *= fEarthRadius;
                f3Pos.z *= fEarthRadius;
                f3Pos.y *= fEarthRadius;

                ComputeVertexHeight(CurrVert, pDataSource, fSamplingStep, fSampleScale);
                f3Pos.y -= fEarthRadius;
            }

        // Align vertices on the outer boundary
        if( iRing < iNumRings-1 )
        {
            for(int i=1; i < iGridDimension-1; i+=2)
            {
                // Top & bottom boundaries
                for(int iRow=0; iRow < iGridDimension; iRow += iGridDimension-1)
                {
                    const auto &V0 = VB[iCurrGridStart + i - 1 + iRow*iGridDimension].f3WorldPos;
                          auto &V1 = VB[iCurrGridStart + i + 0 + iRow*iGridDimension].f3WorldPos;
                    const auto &V2 = VB[iCurrGridStart + i + 1 + iRow*iGridDimension].f3WorldPos;
                    V1 = (V0+V2)/2.f;
                }

                // Left & right boundaries
                for(int iCol=0; iCol < iGridDimension; iCol += iGridDimension-1)
                {
                    const auto &V0 = VB[iCurrGridStart + iCol + (i - 1)*iGridDimension].f3WorldPos;
                          auto &V1 = VB[iCurrGridStart + iCol + (i + 0)*iGridDimension].f3WorldPos;
                    const auto &V2 = VB[iCurrGridStart + iCol + (i + 1)*iGridDimension].f3WorldPos;
                    V1 = (V0+V2)/2.f;
                }
            }


            // Add triangles stitching this ring with the next one
            int iNextGridStart = (int)VB.size();
            VERIFY_EXPR( iNextGridStart == iCurrGridStart + iGridDimension*iGridDimension);

            // Bottom boundary
            for(int iCol=0; iCol < iGridDimension-1; iCol += 2)
            {
                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2) + iGridQuart * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iCol+1) + 0 * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iCol+0) + 0 * iGridDimension); 

                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2) + iGridQuart * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iCol+2) + 0 * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iCol+1) + 0 * iGridDimension); 

                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2)   + iGridQuart * iGridDimension); 
                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2+1) + iGridQuart * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iCol+2) + 0 * iGridDimension); 
            }

            // Top boundary
            for(int iCol=0; iCol < iGridDimension-1; iCol += 2)
            {
                StitchIB.push_back(iCurrGridStart + (iCol+0) + (iGridDimension-1) * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iCol+1) + (iGridDimension-1) * iGridDimension); 
                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2) + iGridQuart* 3 * iGridDimension); 

                StitchIB.push_back(iCurrGridStart + (iCol+1) + (iGridDimension-1) * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iCol+2) + (iGridDimension-1) * iGridDimension); 
                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2) + iGridQuart* 3 * iGridDimension); 

                StitchIB.push_back(iCurrGridStart + (iCol+2) + (iGridDimension-1) * iGridDimension); 
                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2 + 1) + iGridQuart* 3 * iGridDimension); 
                StitchIB.push_back(iNextGridStart + (iGridQuart + iCol/2)     + iGridQuart* 3 * iGridDimension); 
            }

            // Left boundary
            for(int iRow=0; iRow < iGridDimension-1; iRow += 2)
            {
                StitchIB.push_back(iNextGridStart + iGridQuart + (iGridQuart+ iRow/2) * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + 0 + (iRow+0) * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + 0 + (iRow+1) * iGridDimension); 

                StitchIB.push_back(iNextGridStart + iGridQuart + (iGridQuart+ iRow/2) * iGridDimension);  
                StitchIB.push_back(iCurrGridStart + 0 + (iRow+1) * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + 0 + (iRow+2) * iGridDimension); 

                StitchIB.push_back(iNextGridStart + iGridQuart + (iGridQuart + iRow/2 + 1) * iGridDimension); 
                StitchIB.push_back(iNextGridStart + iGridQuart + (iGridQuart + iRow/2)     * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + 0 + (iRow+2) * iGridDimension); 
            }

            // Right boundary
            for(int iRow=0; iRow < iGridDimension-1; iRow += 2)
            {
                StitchIB.push_back(iCurrGridStart + (iGridDimension-1) + (iRow+1) * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iGridDimension-1) + (iRow+0) * iGridDimension); 
                StitchIB.push_back(iNextGridStart + iGridQuart*3 + (iGridQuart+ iRow/2) * iGridDimension); 

                StitchIB.push_back(iCurrGridStart + (iGridDimension-1) + (iRow+2) * iGridDimension); 
                StitchIB.push_back(iCurrGridStart + (iGridDimension-1) + (iRow+1) * iGridDimension); 
                StitchIB.push_back(iNextGridStart + iGridQuart*3 + (iGridQuart+ iRow/2) * iGridDimension); 

                StitchIB.push_back(iCurrGridStart + (iGridDimension-1) + (iRow+2) * iGridDimension); 
                StitchIB.push_back(iNextGridStart + iGridQuart*3 + (iGridQuart+ iRow/2)     * iGridDimension); 
                StitchIB.push_back(iNextGridStart + iGridQuart*3 + (iGridQuart+ iRow/2 + 1) * iGridDimension); 
            }
        }


        // Generate indices for the current ring
        if( iRing == 0 )
        {
            RingMeshBuilder.CreateMesh( iCurrGridStart, 0,                   0, iGridMidst+1, iGridMidst+1, QUAD_TRIANG_TYPE_00_TO_11);
            RingMeshBuilder.CreateMesh( iCurrGridStart, iGridMidst,          0, iGridMidst+1, iGridMidst+1, QUAD_TRIANG_TYPE_01_TO_10);
            RingMeshBuilder.CreateMesh( iCurrGridStart, 0,          iGridMidst, iGridMidst+1, iGridMidst+1, QUAD_TRIANG_TYPE_01_TO_10);
            RingMeshBuilder.CreateMesh( iCurrGridStart, iGridMidst, iGridMidst, iGridMidst+1, iGridMidst+1, QUAD_TRIANG_TYPE_00_TO_11);
        }
        else
        {
            RingMeshBuilder.CreateMesh( iCurrGridStart,            0,            0,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_00_TO_11);
            RingMeshBuilder.CreateMesh( iCurrGridStart,   iGridQuart,            0,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_00_TO_11);

            RingMeshBuilder.CreateMesh( iCurrGridStart,   iGridMidst,            0,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_01_TO_10);
            RingMeshBuilder.CreateMesh( iCurrGridStart, iGridQuart*3,            0,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_01_TO_10);
                                        
            RingMeshBuilder.CreateMesh( iCurrGridStart,            0,   iGridQuart,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_00_TO_11);
            RingMeshBuilder.CreateMesh( iCurrGridStart,            0,   iGridMidst,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_01_TO_10);
                                        
            RingMeshBuilder.CreateMesh( iCurrGridStart, iGridQuart*3,   iGridQuart,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_01_TO_10);
            RingMeshBuilder.CreateMesh( iCurrGridStart, iGridQuart*3,   iGridMidst,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_00_TO_11);

            RingMeshBuilder.CreateMesh( iCurrGridStart,            0, iGridQuart*3,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_01_TO_10);
            RingMeshBuilder.CreateMesh( iCurrGridStart,   iGridQuart, iGridQuart*3,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_01_TO_10);

            RingMeshBuilder.CreateMesh( iCurrGridStart,   iGridMidst, iGridQuart*3,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_00_TO_11);
            RingMeshBuilder.CreateMesh( iCurrGridStart, iGridQuart*3, iGridQuart*3,   iGridQuart+1, iGridQuart+1, QUAD_TRIANG_TYPE_00_TO_11);
        }
    }
    
    // We do not need per-vertex normals as we use normal map to shade terrain
    // Sphere tangent vertex are computed in the shader
#if 0
    // Compute normals
    const float3 *pV0 = nullptr;
    const float3 *pV1 = &VB[ IB[0] ].f3WorldPos;
    const float3 *pV2 = &VB[ IB[1] ].f3WorldPos;
    float fSign = +1;
    for(Uint32 Ind=2; Ind < m_uiIndicesInIndBuff; ++Ind)
    {
        fSign = -fSign;
        pV0 = pV1;
        pV1 = pV2;
        pV2 =  &VB[ IB[Ind] ].f3WorldPos;
        float3 Rib0 = *pV0 - *pV1;
        float3 Rib1 = *pV1 - *pV2;
        float3 TriN;
        D3DXVec3Cross(&TriN, &Rib0, &Rib1);
        float fLength = D3DXVec3Length(&TriN);
        if( fLength > 0.1 )
        {
            TriN /= fLength*fSign;
            for(int i=-2; i <= 0; ++i)
                VB[ IB[Ind+i] ].f3Normal += TriN;
        }
    }
    for(auto VBIt=VB.begin(); VBIt != VB.end(); ++VBIt)
    {
        float fLength = D3DXVec3Length(&VBIt->f3Normal);
        if( fLength > 1 )
            VBIt->f3Normal /= fLength;
    }

    // Adjust normals on boundaries
    for(int iRing = iStartRing; iRing < iNumRings-1; ++iRing)
    {
        int iCurrGridStart = (iRing-iStartRing) * iGridDimension*iGridDimension;
        int iNextGridStart = (iRing-iStartRing+1) * iGridDimension*iGridDimension;
        for(int i=0; i < iGridDimension; i+=2)
        {
            for(int Bnd=0; Bnd < 2; ++Bnd)
            {
                const int CurrGridOffsets[] = {0, iGridDimension-1};
                const int NextGridPffsets[] = {iGridQuart, iGridQuart*3};
                // Left and right boundaries
                {
                    auto &CurrGridN = VB[iCurrGridStart + CurrGridOffsets[Bnd] + i*iGridDimension].f3Normal;
                    auto &NextGridN = VB[iNextGridStart + NextGridPffsets[Bnd] + (iGridQuart+i/2)*iGridDimension].f3Normal;
                    auto NewN = CurrGridN + NextGridN;
                    D3DXVec3Normalize(&NewN, &NewN);
                    CurrGridN = NextGridN = NewN;
                    if( i > 1 )
                    {
                        auto &PrevCurrGridN = VB[iCurrGridStart + CurrGridOffsets[Bnd] + (i-2)*iGridDimension].f3Normal;
                        auto MiddleN = PrevCurrGridN + NewN;
                        D3DXVec3Normalize( &VB[iCurrGridStart + CurrGridOffsets[Bnd] + (i-1)*iGridDimension].f3Normal, &MiddleN);
                    }
                }

                // Bottom and top boundaries
                {
                    auto &CurrGridN = VB[iCurrGridStart +                i + CurrGridOffsets[Bnd]*iGridDimension].f3Normal;
                    auto &NextGridN = VB[iNextGridStart + (iGridQuart+i/2) + NextGridPffsets[Bnd]*iGridDimension].f3Normal;
                    auto NewN = CurrGridN + NextGridN;
                    D3DXVec3Normalize(&NewN, &NewN);
                    CurrGridN = NextGridN = NewN;
                    if( i > 1 )
                    {
                        auto &PrevCurrGridN = VB[iCurrGridStart + (i-2) + CurrGridOffsets[Bnd]*iGridDimension].f3Normal;
                        auto MiddleN = PrevCurrGridN + NewN;
                        D3DXVec3Normalize( &VB[iCurrGridStart + (i-1) + CurrGridOffsets[Bnd]*iGridDimension].f3Normal, &MiddleN);
                    }
                }
            }
        }
    }
#endif
}


void EarthHemsiphere::RenderNormalMap(IRenderDevice* pDevice,
                                       IDeviceContext* pContext,
                                       const Uint16 *pHeightMap,
                                       size_t HeightMapPitch,
                                       int iHeightMapDim,
                                       ITexture *ptex2DNormalMap,
                                       IResourceMapping *pResMapping)
{
    TextureDesc HeightMapDesc;
    HeightMapDesc.Name = "Height map texture";
    HeightMapDesc.Type = RESOURCE_DIM_TEX_2D;
    HeightMapDesc.Width = iHeightMapDim;
    HeightMapDesc.Height = iHeightMapDim;
    HeightMapDesc.Format = TEX_FORMAT_R16_UINT;
    HeightMapDesc.Usage = USAGE_STATIC;
    HeightMapDesc.BindFlags = BIND_SHADER_RESOURCE;
    HeightMapDesc.MipLevels = ComputeMipLevelsCount(HeightMapDesc.Width, HeightMapDesc.Height);

    std::vector<Uint16> CoarseMipLevels;
    CoarseMipLevels.resize( iHeightMapDim/2 * iHeightMapDim );

    std::vector<TextureSubResData> InitData(HeightMapDesc.MipLevels);
    InitData[0].pData = pHeightMap;
    InitData[0].Stride = (Uint32)HeightMapPitch*sizeof(pHeightMap[0]);
    const Uint16 *pFinerMipLevel = pHeightMap;
    Uint16 *pCurrMipLevel = &CoarseMipLevels[0];
    size_t FinerMipPitch = HeightMapPitch;
    size_t CurrMipPitch = iHeightMapDim/2;
    for(Uint32 uiMipLevel = 1; uiMipLevel < HeightMapDesc.MipLevels; ++uiMipLevel)
    {
        auto MipWidth  = HeightMapDesc.Width >> uiMipLevel;
        auto MipHeight = HeightMapDesc.Height >> uiMipLevel;
        for(Uint32 uiRow=0; uiRow < MipHeight; ++uiRow)
            for(Uint32 uiCol=0; uiCol < MipWidth; ++uiCol)
            {
                int iAverageHeight = 0;
                for(int i=0; i<2; ++i)
                    for(int j=0; j<2; ++j)
                        iAverageHeight += pFinerMipLevel[ (uiCol*2+i) + (uiRow*2+j)*FinerMipPitch];
                pCurrMipLevel[uiCol + uiRow*CurrMipPitch] = (Uint16)(iAverageHeight>>2);
            }

        InitData[uiMipLevel].pData = pCurrMipLevel;
        InitData[uiMipLevel].Stride = (Uint32)CurrMipPitch*sizeof(*pCurrMipLevel);
        pFinerMipLevel = pCurrMipLevel;
        FinerMipPitch = CurrMipPitch;
        pCurrMipLevel += MipHeight*CurrMipPitch;
        CurrMipPitch = iHeightMapDim/2;
    }

    RefCntAutoPtr<ITexture> ptex2DHeightMap;
    TextureData HeigtMapInitData;
    HeigtMapInitData.pSubResources = InitData.data();
    HeigtMapInitData.NumSubresources = (Uint32)InitData.size();
    pDevice->CreateTexture(HeightMapDesc, HeigtMapInitData, &ptex2DHeightMap);
    VERIFY(ptex2DHeightMap, "Failed to create height map texture");

    pResMapping->AddResource( "g_tex2DElevationMap", ptex2DHeightMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE), true );
    
    RefCntAutoPtr<IBuffer> pcbNMGenerationAttribs;
    CreateUniformBuffer(pDevice, sizeof(NMGenerationAttribs), &pcbNMGenerationAttribs);

    pResMapping->AddResource( "cbNMGenerationAttribs", pcbNMGenerationAttribs, true );
    m_pTerrainScript->Run( pContext, "CreateRenderNormalMapShaders" );

    m_pTerrainScript->Run( pContext, "SetRenderNormalMapShadersAndStates" );

    const auto &NormalMapDesc = ptex2DNormalMap->GetDesc();
    for(Uint32 uiMipLevel = 0; uiMipLevel < NormalMapDesc.MipLevels; ++uiMipLevel)
    {
        TextureViewDesc TexViewDesc;
        TexViewDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        TexViewDesc.MostDetailedMip = uiMipLevel;
        RefCntAutoPtr<ITextureView> ptex2DNormalMapRTV;
        ptex2DNormalMap->CreateView( TexViewDesc, &ptex2DNormalMapRTV );

        ITextureView *pRTVs[] = { ptex2DNormalMapRTV };
        pContext->SetRenderTargets(_countof(pRTVs), pRTVs, nullptr);

        {
            MapHelper<NMGenerationAttribs> NMGenerationAttribs( pContext, pcbNMGenerationAttribs, MAP_WRITE_DISCARD, 0 );
            NMGenerationAttribs->m_fElevationScale = m_Params.m_TerrainAttribs.m_fElevationScale;
            NMGenerationAttribs->m_fSampleSpacingInterval = m_Params.m_TerrainAttribs.m_fElevationSamplingInterval;
            NMGenerationAttribs->m_iMIPLevel = static_cast<int>(uiMipLevel);
        }

        DrawAttribs DrawAttrs;
        DrawAttrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        DrawAttrs.NumVertices = 4;
        pContext->Draw( DrawAttrs );
    }

    // Remove elevation map from resource mapping to release the resource
    pResMapping->RemoveResourceByName( "g_tex2DElevationMap" );

    // Restore default render target
    pContext->SetRenderTargets(0, nullptr, nullptr);
}


void EarthHemsiphere::Create( class ElevationDataSource *pDataSource,
                               const RenderingParams &Params,
                               IRenderDevice* pDevice,
                               IDeviceContext* pContext,
                               const Char* MaterialMaskPath,
							   const Char* TileTexturePath[],
                               const Char* TileNormalMapPath[],
                               IBuffer *pcbCameraAttribs,
                               IBuffer *pcbLightAttribs,
                               IBuffer *pcMediaScatteringParams)
{
    m_Params = Params;
    m_pDevice = pDevice;

    const Uint16 *pHeightMap;
    size_t HeightMapPitch;
    pDataSource->GetDataPtr(pHeightMap, HeightMapPitch);
    int iHeightMapDim = pDataSource->GetNumCols();
    VERIFY_EXPR(iHeightMapDim == pDataSource->GetNumRows() );

    TextureDesc NormalMapDesc;
    NormalMapDesc.Name = "Normal map texture";
    NormalMapDesc.Type = RESOURCE_DIM_TEX_2D;
    NormalMapDesc.Width = iHeightMapDim;
    NormalMapDesc.Height = iHeightMapDim;
    NormalMapDesc.Format = TEX_FORMAT_RG8_UNORM;
    NormalMapDesc.Usage = USAGE_DEFAULT;
    NormalMapDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    NormalMapDesc.MipLevels = 0;
  
    RefCntAutoPtr<ITexture>  ptex2DNormalMap;
    pDevice->CreateTexture(NormalMapDesc, TextureData(), &ptex2DNormalMap);
    RefCntAutoPtr<ITextureView> m_ptex2DNormalMapSRV( ptex2DNormalMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) );

    CreateUniformBuffer( pDevice, sizeof( TerrainAttribs ), &m_pcbTerrainAttribs );

    RefCntAutoPtr<IResourceMapping> pResMapping;
    ResourceMappingDesc ResMappingDesc;
    ResourceMappingEntry pEntries[] = 
    { 
        { "cbCameraAttribs", pcbCameraAttribs }, 
        { "cbTerrainAttribs", m_pcbTerrainAttribs}, 
        { "cbLightAttribs", pcbLightAttribs}, 
        { "g_tex2DNormalMap", ptex2DNormalMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) }, 
        { "cbParticipatingMediaScatteringParams", pcMediaScatteringParams },
        {} 
    };
    ResMappingDesc.pEntries = pEntries;
    pDevice->CreateResourceMapping( ResMappingDesc, &pResMapping );

    RefCntAutoPtr<ITexture> ptex2DMtrlMask;
    CreateTextureFromFile(MaterialMaskPath, TextureLoadInfo(), pDevice, &ptex2DMtrlMask);
    auto ptex2DMtrlMaskSRV = ptex2DMtrlMask->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    pResMapping->AddResource("g_tex2DMtrlMap", ptex2DMtrlMaskSRV, true);

    // Load tiles
    IDeviceObject* ptex2DTileDiffuseSRV[NUM_TILE_TEXTURES] = {};
    RefCntAutoPtr<ITexture> ptex2DTileDiffuse[NUM_TILE_TEXTURES];
    IDeviceObject* ptex2DTileNMSRV[NUM_TILE_TEXTURES] = {};
    RefCntAutoPtr<ITexture> ptex2DTileNM[NUM_TILE_TEXTURES];
	for(int iTileTex = 0; iTileTex < (int)NUM_TILE_TEXTURES; iTileTex++)
    {
        {
            TextureLoadInfo DiffMapLoadInfo;
            DiffMapLoadInfo.IsSRGB = false;
            CreateTextureFromFile(TileTexturePath[iTileTex], DiffMapLoadInfo, pDevice, &ptex2DTileDiffuse[iTileTex]);
            ptex2DTileDiffuseSRV[iTileTex] = ptex2DTileDiffuse[iTileTex]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
        
        {
            
            CreateTextureFromFile(TileNormalMapPath[iTileTex], TextureLoadInfo(), pDevice, &ptex2DTileNM[iTileTex]);
            ptex2DTileNMSRV[iTileTex] = ptex2DTileNM[iTileTex]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
	}
    pResMapping->AddResourceArray("g_tex2DTileDiffuse", 0,  ptex2DTileDiffuseSRV, NUM_TILE_TEXTURES, true);
    pResMapping->AddResourceArray("g_tex2DTileNM", 0, ptex2DTileNMSRV, NUM_TILE_TEXTURES, true);


    m_pTerrainScript = CreateRenderScriptFromFile( "shaders\\Terrain.lua", pDevice, pContext, [&]( ScriptParser *pScriptParser )
    {
        pScriptParser->SetGlobalVariable( "extResourceMapping", pResMapping );
    } );

    m_pTerrainScript->GetSamplerByName( "ComparisonSampler", &m_pComparisonSampler );

    RenderNormalMap( pDevice, pContext, pHeightMap, HeightMapPitch, iHeightMapDim, ptex2DNormalMap, pResMapping );

    m_pTerrainScript->Run( pContext, "CreateHemisphereShaders" );

    std::vector<HemisphereVertex> VB;
    std::vector<Uint32> StitchIB;
    GenerateSphereGeometry(pDevice, AirScatteringAttribs().fEarthRadius, m_Params.m_iRingDimension, m_Params.m_iNumRings, pDataSource, m_Params.m_TerrainAttribs.m_fElevationSamplingInterval, m_Params.m_TerrainAttribs.m_fElevationScale, VB, StitchIB, m_SphereMeshes);

    BufferDesc VBDesc;
    VBDesc.Name = "Hemisphere vertex buffer";
    VBDesc.uiSizeInBytes = (Uint32)(VB.size() * sizeof( VB[0] ));
    VBDesc.Usage = USAGE_STATIC;
    VBDesc.BindFlags = BIND_VERTEX_BUFFER;
    BufferData VBInitData;
    VBInitData.pData = VB.data();
    VBInitData.DataSize = VBDesc.uiSizeInBytes;
    pDevice->CreateBuffer( VBDesc, VBInitData, &m_pVertBuff );
    VERIFY( m_pVertBuff, "Failed to create VB" );

    m_uiNumStitchIndices = (Uint32)StitchIB.size();
    BufferDesc StitchIndexBufferDesc;
    StitchIndexBufferDesc.uiSizeInBytes = (Uint32)(m_uiNumStitchIndices * sizeof( StitchIB[0] ));
    StitchIndexBufferDesc.Usage = USAGE_STATIC;
    StitchIndexBufferDesc.BindFlags = BIND_INDEX_BUFFER;
    BufferData IBInitData;
    IBInitData.pData = StitchIB.data();
    IBInitData.DataSize = StitchIndexBufferDesc.uiSizeInBytes;
    // Create the buffer
    pDevice->CreateBuffer( StitchIndexBufferDesc, IBInitData, &m_pStitchIndBuff);
    VERIFY( m_pStitchIndBuff, "Failed to create stitch IB" );
}

void EarthHemsiphere::Render(IDeviceContext* pContext,
                              const RenderingParams &NewParams,
                              const float3 &vCameraPosition, 
                              const float4x4 &CameraViewProjMatrix,
                              ITextureView *pShadowMapSRV,
                              ITextureView *pPrecomputedNetDensitySRV,
                              ITextureView *pAmbientSkylightSRV,
                              bool bZOnlyPass)
{
    if( m_Params.m_iNumShadowCascades != NewParams.m_iNumShadowCascades    ||
        m_Params.m_bBestCascadeSearch != NewParams.m_bBestCascadeSearch    || 
        m_Params.m_bSmoothShadows     != NewParams.m_bSmoothShadows ||
        m_Params.DstRTVFormat         != NewParams.DstRTVFormat )
    {
        m_pHemispherePS.Release();
    }

    m_Params = NewParams;

#if 0
    if( GetAsyncKeyState(VK_F9) )
    {
        m_RenderEarthHemisphereTech.Release();
    }
#endif

    if( !m_pHemispherePS )
    {
        ShaderCreationAttribs Attrs;
        Attrs.FilePath = "HemispherePS.fx";
        Attrs.EntryPoint = "HemispherePS";
        Attrs.SearchDirectories = "shaders;shaders\\terrain;";
        Attrs.Desc.ShaderType = SHADER_TYPE_PIXEL;
        Attrs.Desc.Name = "HemispherePS";
        ShaderVariableDesc ShaderVars[] = 
        {
            {"g_tex2DShadowMap", SHADER_VARIABLE_TYPE_DYNAMIC},
        };
        Attrs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        BasicShaderSourceStreamFactory BasicSSSFactory(Attrs.SearchDirectories);
        Attrs.pShaderSourceStreamFactory = &BasicSSSFactory;

        StaticSamplerDesc StaticSamplers[5];
        StaticSamplers[0].TextureName = "g_tex2DTileDiffuse";
        StaticSamplers[0].Desc.AddressU = TEXTURE_ADDRESS_WRAP;
        StaticSamplers[0].Desc.AddressV = TEXTURE_ADDRESS_WRAP;
        StaticSamplers[0].Desc.AddressW = TEXTURE_ADDRESS_WRAP;

        StaticSamplers[1].TextureName = "g_tex2DTileNM";
        StaticSamplers[1].Desc = StaticSamplers[0].Desc;

        StaticSamplers[2].TextureName = "g_tex2DNormalMap";
        StaticSamplers[2].Desc.AddressU = TEXTURE_ADDRESS_MIRROR;
        StaticSamplers[2].Desc.AddressV = TEXTURE_ADDRESS_MIRROR;
        StaticSamplers[2].Desc.AddressW = TEXTURE_ADDRESS_MIRROR;

        StaticSamplers[3].TextureName = "g_tex2DMtrlMap";
        StaticSamplers[3].Desc = StaticSamplers[2].Desc;

        StaticSamplers[4].TextureName = "g_tex2DShadowMap";
        StaticSamplers[4].Desc.MinFilter = FILTER_TYPE_COMPARISON_LINEAR;
        StaticSamplers[4].Desc.MagFilter = FILTER_TYPE_COMPARISON_LINEAR;
        StaticSamplers[4].Desc.MipFilter = FILTER_TYPE_COMPARISON_LINEAR;
        StaticSamplers[4].Desc.ComparisonFunc = COMPARISON_FUNC_LESS;


        Attrs.Desc.StaticSamplers = StaticSamplers;
        Attrs.Desc.NumStaticSamplers = _countof(StaticSamplers);

        ShaderMacroHelper Macros;
        Macros.AddShaderMacro("TEXTURING_MODE", m_Params.m_TexturingMode);
        Macros.AddShaderMacro("NUM_TILE_TEXTURES", NUM_TILE_TEXTURES);
        Macros.AddShaderMacro("NUM_SHADOW_CASCADES", m_Params.m_iNumShadowCascades);
        Macros.AddShaderMacro("BEST_CASCADE_SEARCH", m_Params.m_bBestCascadeSearch ? true : false);
        Macros.AddShaderMacro("SMOOTH_SHADOWS", m_Params.m_bSmoothShadows ? true : false);
        Macros.Finalize();
        Attrs.Macros = Macros;

        Attrs.Desc.VariableDesc = ShaderVars;
        Attrs.Desc.NumVariables = _countof(ShaderVars);

        m_pDevice->CreateShader( Attrs, &m_pHemispherePS );
        m_pTerrainScript->Run( "SetHemispherePS", m_pHemispherePS, GetTextureFormatAttribs(m_Params.DstRTVFormat).Name );
    }


    ViewFrustum ViewFrustum;
    auto DevType = m_pDevice->GetDeviceCaps().DevType;
    ExtractViewFrustumPlanesFromMatrix(CameraViewProjMatrix, ViewFrustum, DevType == DeviceType::D3D11 || DevType == DeviceType::D3D12);

    {
        MapHelper<TerrainAttribs> TerrainAttribs( pContext, m_pcbTerrainAttribs, MAP_WRITE_DISCARD, 0 );
        *TerrainAttribs = m_Params.m_TerrainAttribs;
    }

#if 0
    ID3D11ShaderResourceView *pSRVs[3 + 2*NUM_TILE_TEXTURES] = 
    {
        m_ptex2DNormalMapSRV,
        m_ptex2DMtrlMaskSRV,
        pShadowMapSRV
    };
    for(int iTileTex = 0; iTileTex < NUM_TILE_TEXTURES; iTileTex++)
    {
        pSRVs[3+iTileTex] = m_ptex2DTilesSRV[iTileTex];
        pSRVs[3+NUM_TILE_TEXTURES+iTileTex] = m_ptex2DTilNormalMapsSRV[iTileTex];
    }
    pd3dImmediateContext->PSSetShaderResources(1, _countof(pSRVs), pSRVs);
    pSRVs[0] = pPrecomputedNetDensitySRV;
    pSRVs[1] = pAmbientSkylightSRV;
    pd3dImmediateContext->VSSetShaderResources(0, 2, pSRVs);

    ID3D11SamplerState *pSamplers[] = {m_psamLinearMirror, m_psamLinearWrap, m_psamComaprison, m_psamLinearClamp};
	pd3dImmediateContext->VSSetSamplers(0, _countof(pSamplers), pSamplers);
	pd3dImmediateContext->PSSetSamplers(0, _countof(pSamplers), pSamplers);
#endif

    Uint32 offset[1] = { 0 };
    Uint32 stride[1] = { sizeof(HemisphereVertex) };
    IBuffer* ppBuffers[1] = { m_pVertBuff };
    pContext->SetVertexBuffers( 0, 1, ppBuffers, stride, offset, SET_VERTEX_BUFFERS_FLAG_RESET);

    if( bZOnlyPass )
        m_pTerrainScript->Run( pContext, "RenderHemisphereShadow" );
    else
    {
        pShadowMapSRV->SetSampler( m_pComparisonSampler );
        m_pTerrainScript->Run( pContext, "RenderHemisphere", pPrecomputedNetDensitySRV, pAmbientSkylightSRV, pShadowMapSRV );
    }

    for(auto MeshIt = m_SphereMeshes.begin();  MeshIt != m_SphereMeshes.end(); ++MeshIt)
    {
        if(IBoxVisible(ViewFrustum, MeshIt->BndBox))
        {
            pContext->SetIndexBuffer(MeshIt->pIndBuff, 0);
            DrawAttribs DrawAttrs;
            DrawAttrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            DrawAttrs.IndexType = VT_UINT32;
            DrawAttrs.NumIndices = MeshIt->uiNumIndices;
            DrawAttrs.IsIndexed = true;
            pContext->Draw(DrawAttrs);
        }
    }
    
    pContext->SetIndexBuffer(m_pStitchIndBuff, 0);
    DrawAttribs DrawAttrs;
    DrawAttrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    DrawAttrs.IndexType = VT_UINT32;
    DrawAttrs.NumIndices = m_uiNumStitchIndices;
    DrawAttrs.IsIndexed = true;
    pContext->Draw(DrawAttrs);
}
