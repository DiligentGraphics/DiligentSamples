#include "AtmosphereShadersCommon.fxh"

Texture2D<float4> g_tex2DSliceEndPoints;
Texture2D<float4> g_tex2DSliceUVDirAndOrigin;
Texture2DArray<float> g_tex2DLightSpaceDepthMap;
SamplerState g_tex2DLightSpaceDepthMap_sampler;

Texture2D<float2> g_tex2DMinMaxLightSpaceDepth;

cbuffer cbLightParams
{
    LightAttribs g_LightAttribs;
}

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
}

cbuffer cbPostProcessingAttribs
{
    PostProcessingAttribs g_PPAttribs;
};

cbuffer cbMiscDynamicParams
{
    MiscDynamicParams g_MiscParams;
}

#define f4IncorrectSliceUVDirAndStart float4(-10000, -10000, 0, 0)
void RenderSliceUVDirInShadowMapTexturePS(in ScreenSizeQuadVSOutput VSOut,
                                          // IMPORTANT: non-system generated pixel shader input
                                          // arguments must have the exact same name as vertex shader 
                                          // outputs and must go in the same order.
                                          // Moreover, even if the shader is not using the argument,
                                          // it still must be declared.

                                          in float4 f4PosPS : SV_Position,
                                          out float4 f4SliceUVDirAndStart : SV_Target )
{
    int iSliceInd = int(f4PosPS.x);
    // Load epipolar slice endpoints
    float4 f4SliceEndpoints = g_tex2DSliceEndPoints.Load(  int3(iSliceInd,0,0) );
    // All correct entry points are completely inside the [-1+1/W,1-1/W]x[-1+1/H,1-1/H] area
    if( !IsValidScreenLocation(f4SliceEndpoints.xy) )
    {
        f4SliceUVDirAndStart = f4IncorrectSliceUVDirAndStart;
        return;
    }

    int iCascadeInd = int(f4PosPS.y);
    matrix mWorldToShadowMapUVDepth = g_LightAttribs.ShadowAttribs.mWorldToShadowMapUVDepth[iCascadeInd];

    // Reconstruct slice exit point position in world space
    float3 f3SliceExitWS = ProjSpaceXYZToWorldSpace( float3(f4SliceEndpoints.zw, g_LightAttribs.ShadowAttribs.Cascades[iCascadeInd].f4StartEndZ.y), g_CameraAttribs.mProj, g_CameraAttribs.mViewProjInv );
    // Transform it to the shadow map UV
    float2 f2SliceExitUV = WorldSpaceToShadowMapUV(f3SliceExitWS, mWorldToShadowMapUVDepth).xy;
    
    // Compute camera position in shadow map UV space
    float2 f2SliceOriginUV = WorldSpaceToShadowMapUV(g_CameraAttribs.f4CameraPos.xyz, mWorldToShadowMapUVDepth).xy;

    // Compute slice direction in shadow map UV space
    float2 f2SliceDir = f2SliceExitUV - f2SliceOriginUV;
    f2SliceDir /= max(abs(f2SliceDir.x), abs(f2SliceDir.y));
    
    float4 f4BoundaryMinMaxXYXY = float4(0.0, 0.0, 1.0, 1.0) + float4(0.5, 0.5, -0.5, -0.5)*g_PPAttribs.m_f2ShadowMapTexelSize.xyxy;
    if( any( Less( (f2SliceOriginUV.xyxy - f4BoundaryMinMaxXYXY) * float4( 1.0, 1.0, -1.0, -1.0), F4ZERO ) ) )
    {
        // If slice origin in UV coordinates falls beyond [0,1]x[0,1] region, we have
        // to continue the ray and intersect it with this rectangle
        //                  
        //    f2SliceOriginUV
        //       *
        //        \
        //         \  New f2SliceOriginUV
        //    1   __\/___
        //       |       |
        //       |       |
        //    0  |_______|
        //       0       1
        //           
        
        // First, compute signed distances from the slice origin to all four boundaries
        bool4 b4IsValidIsecFlag = Greater(abs(f2SliceDir.xyxy), 1e-6 * F4ONE);
        float4 f4DistToBoundaries = (f4BoundaryMinMaxXYXY - f2SliceOriginUV.xyxy) / (f2SliceDir.xyxy + BoolToFloat( Not(b4IsValidIsecFlag) ) );

        //We consider only intersections in the direction of the ray
        b4IsValidIsecFlag = And( b4IsValidIsecFlag, Greater(f4DistToBoundaries, F4ZERO) );
        // Compute the second intersection coordinate
        float4 f4IsecYXYX = f2SliceOriginUV.yxyx + f4DistToBoundaries * f2SliceDir.yxyx;
        
        // Select only these coordinates that fall onto the boundary
        b4IsValidIsecFlag = And( b4IsValidIsecFlag, GreaterEqual(f4IsecYXYX, f4BoundaryMinMaxXYXY.yxyx));
        b4IsValidIsecFlag = And( b4IsValidIsecFlag, LessEqual(f4IsecYXYX, f4BoundaryMinMaxXYXY.wzwz) );
        // Replace distances to all incorrect boundaries with the large value
        f4DistToBoundaries = BoolToFloat(b4IsValidIsecFlag) * f4DistToBoundaries + 
                             // It is important to make sure compiler does not use mad here,
                             // otherwise operations with FLT_MAX will lose all precision
                             BoolToFloat( Not(b4IsValidIsecFlag) ) * float4(+FLT_MAX, +FLT_MAX, +FLT_MAX, +FLT_MAX);
        // Select the closest valid intersection
        float2 f2MinDist = min(f4DistToBoundaries.xy, f4DistToBoundaries.zw);
        float fMinDist = min(f2MinDist.x, f2MinDist.y);
        
        // Update origin
        f2SliceOriginUV = f2SliceOriginUV + fMinDist * f2SliceDir;
    }
    
    f2SliceDir *= g_PPAttribs.m_f2ShadowMapTexelSize;

    f4SliceUVDirAndStart = float4(f2SliceDir, f2SliceOriginUV);
}



// Note that min/max shadow map does not contain finest resolution level
// The first level it contains corresponds to step == 2
void InitializeMinMaxShadowMapPS(in ScreenSizeQuadVSOutput VSOut,
                                 in float4 f4Pos : SV_Position,
                                 out float2 f2MinMaxDepth : SV_Target)
{
    uint uiSliceInd;
    float fCascadeInd;
#if USE_COMBINED_MIN_MAX_TEXTURE
    fCascadeInd = floor(f4Pos.y / float(NUM_EPIPOLAR_SLICES));
    uiSliceInd = uint(f4Pos.y - fCascadeInd * float(NUM_EPIPOLAR_SLICES));
    fCascadeInd += g_PPAttribs.m_fFirstCascade;
#else
    uiSliceInd = uint(f4Pos.y);
    fCascadeInd = g_MiscParams.fCascadeInd;
#endif
    // Load slice direction in shadow map
    float4 f4SliceUVDirAndOrigin = g_tex2DSliceUVDirAndOrigin.Load( uint3(uiSliceInd, fCascadeInd, 0) );
    // Calculate current sample position on the ray
    float2 f2CurrUV = f4SliceUVDirAndOrigin.zw + f4SliceUVDirAndOrigin.xy * floor(f4Pos.x) * 2.f;
    
    float4 f4MinDepth = F4ONE;
    float4 f4MaxDepth = F4ZERO;
    // Gather 8 depths which will be used for PCF filtering for this sample and its immediate neighbor 
    // along the epipolar slice
    // Note that if the sample is located outside the shadow map, Gather() will return 0 as 
    // specified by the samLinearBorder0. As a result volumes outside the shadow map will always be lit
    for( float i=0.0; i<=1.0; ++i )
    {
        float4 f4Depths = g_tex2DLightSpaceDepthMap.Gather(g_tex2DLightSpaceDepthMap_sampler, float3(f2CurrUV + i * f4SliceUVDirAndOrigin.xy, fCascadeInd) );
        f4MinDepth = min(f4MinDepth, f4Depths);
        f4MaxDepth = max(f4MaxDepth, f4Depths);
    }

    f4MinDepth.xy = min(f4MinDepth.xy, f4MinDepth.zw);
    f4MinDepth.x = min(f4MinDepth.x, f4MinDepth.y);

    f4MaxDepth.xy = max(f4MaxDepth.xy, f4MaxDepth.zw);
    f4MaxDepth.x = max(f4MaxDepth.x, f4MaxDepth.y);
#if !IS_32BIT_MIN_MAX_MAP
    const float R16_UNORM_PRECISION = 1.0 / float(1<<16);
    f4MinDepth.x = floor(f4MinDepth.x/R16_UNORM_PRECISION)*R16_UNORM_PRECISION;
    f4MaxDepth.x =  ceil(f4MaxDepth.x/R16_UNORM_PRECISION)*R16_UNORM_PRECISION;
#endif
    f2MinMaxDepth = float2(f4MinDepth.x, f4MaxDepth.x);
}


// 1D min max mip map is arranged as follows:
//
//    g_MiscParams.ui4SrcDstMinMaxLevelOffset.x
//     |
//     |      g_MiscParams.ui4SrcDstMinMaxLevelOffset.z
//     |_______|____ __
//     |       |    |  |
//     |       |    |  |
//     |       |    |  |
//     |       |    |  |
//     |_______|____|__|
//     |<----->|<-->|
//         |     |
//         |    uiMinMaxShadowMapResolution/
//      uiMinMaxShadowMapResolution/2
//                         
void ComputeMinMaxShadowMapLevelPS(in ScreenSizeQuadVSOutput VSOut,
                                   in float4 f4Pos : SV_Position,
                                   out float2 f2MinMaxDepth : SV_Target)
{
    uint2 uiDstSampleInd = uint2(f4Pos.xy);
    uint2 uiSrcSample0Ind = uint2(g_MiscParams.ui4SrcDstMinMaxLevelOffset.x + (uiDstSampleInd.x - g_MiscParams.ui4SrcDstMinMaxLevelOffset.z)*2u, uiDstSampleInd.y);
    uint2 uiSrcSample1Ind = uiSrcSample0Ind + uint2(1,0);
    float2 f2MinMaxDepth0 = g_tex2DMinMaxLightSpaceDepth.Load( int3(uiSrcSample0Ind,0) ).xy;
    float2 f2MinMaxDepth1 = g_tex2DMinMaxLightSpaceDepth.Load( int3(uiSrcSample1Ind,0) ).xy;

    f2MinMaxDepth.x = min(f2MinMaxDepth0.x, f2MinMaxDepth1.x);
    f2MinMaxDepth.y = max(f2MinMaxDepth0.y, f2MinMaxDepth1.y);
}
