
#include "HostSharedTerrainStructs.fxh"
#include "TerrainShadersCommon.fxh"

cbuffer cbTerrainAttribs
{
    TerrainAttribs g_TerrainAttribs;
};

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
}

cbuffer cbLightAttribs
{
    LightAttribs g_LightAttribs;
};


#define EARTH_REFLECTANCE 0.4


// Normal map stores only x,y components. z component is calculated as sqrt(1 - x^2 - y^2)
Texture2D g_tex2DNormalMap;
SamplerState g_tex2DNormalMap_sampler; // Linear Mirror

Texture2D< float4 > g_tex2DMtrlMap;
SamplerState g_tex2DMtrlMap_sampler; // Linear Mirror

Texture2DArray < float >  g_tex2DShadowMap;
SamplerComparisonState g_tex2DShadowMap_sampler; // Comparison

Texture2D< float3 > g_tex2DTileDiffuse[5];   // Material diffuse
SamplerState g_tex2DTileDiffuse_sampler;   

Texture2D< float3 > g_tex2DTileNM[5];   // Material NM
SamplerState g_tex2DTileNM_sampler;   



void FindCascade(float3 f3PosInLightViewSpace,
                 float fCameraViewSpaceZ,
                 out float3 f3PosInCascadeProjSpace,
                 out float3 f3CascadeLightSpaceScale,
                 out int Cascade)
{
    f3PosInCascadeProjSpace = float3(0.0, 0.0, 0.0);
    f3CascadeLightSpaceScale = float3(0.0, 0.0, 0.0);
    Cascade = 0;
#if BEST_CASCADE_SEARCH
    while(Cascade < NUM_SHADOW_CASCADES)
    {
        // Find the smallest cascade which covers current point
        CascadeAttribs CascadeAttribs = g_LightAttribs.ShadowAttribs.Cascades[ Cascade];
        f3CascadeLightSpaceScale = CascadeAttribs.f4LightSpaceScale.xyz;
        f3PosInCascadeProjSpace = f3PosInLightViewSpace * f3CascadeLightSpaceScale + g_LightAttribs.ShadowAttribs.Cascades[Cascade].f4LightSpaceScaledBias.xyz;
        
        // In order to perform PCF filtering without getting out of the cascade shadow map,
        // we need to be far enough from its boundaries.
        if( //Cascade == (NUM_SHADOW_CASCADES - 1) || 
            abs(f3PosInCascadeProjSpace.x) < 1.0/*- CascadeAttribs.f4LightProjSpaceFilterRadius.x*/ &&
            abs(f3PosInCascadeProjSpace.y) < 1.0/*- CascadeAttribs.f4LightProjSpaceFilterRadius.y*/ &&
            // It is necessary to check f3PosInCascadeProjSpace.z as well since it could be behind
            // the far clipping plane of the current cascade
            // Besides, if VSM or EVSM filtering is performed, there is also z boundary
            NDC_MIN_Z /*+ CascadeAttribs.f4LightProjSpaceFilterRadius.z*/ < f3PosInCascadeProjSpace.z && f3PosInCascadeProjSpace.z < 1.0  /*- CascadeAttribs.f4LightProjSpaceFilterRadius.w*/ )
            break;
        else
            Cascade++;
    }
#else
    [unroll]
    for(int i=0; i<(NUM_SHADOW_CASCADES+3)/4; ++i)
    {
        float4 f4CascadeZEnd = g_LightAttribs.ShadowAttribs.f4CascadeCamSpaceZEnd[i];
        float4 v = float4( f4CascadeZEnd.x < fCameraViewSpaceZ ? 1.0 : 0.0, 
                           f4CascadeZEnd.y < fCameraViewSpaceZ ? 1.0 : 0.0,
                           f4CascadeZEnd.z < fCameraViewSpaceZ ? 1.0 : 0.0,
                           f4CascadeZEnd.w < fCameraViewSpaceZ ? 1.0 : 0.0);
	    //float4 v = float4(g_LightAttribs.ShadowAttribs.f4CascadeCamSpaceZEnd[i] < fCameraViewSpaceZ);
	    Cascade += int(dot(float4(1.0,1.0,1.0,1.0), v));
    }
    if( Cascade < NUM_SHADOW_CASCADES )
    {
    //Cascade = min(Cascade, NUM_SHADOW_CASCADES - 1);
        f3CascadeLightSpaceScale = g_LightAttribs.ShadowAttribs.Cascades[Cascade].f4LightSpaceScale.xyz;
        f3PosInCascadeProjSpace = f3PosInLightViewSpace * f3CascadeLightSpaceScale + g_LightAttribs.ShadowAttribs.Cascades[Cascade].f4LightSpaceScaledBias.xyz;
    }
#endif
}

float2 ComputeReceiverPlaneDepthBias(float3 ShadowUVDepthDX, float3 ShadowUVDepthDY)
{    
    // Compute (dDepth/dU, dDepth/dV):
    //  
    //  | dDepth/dU |    | dX/dU    dX/dV |T  | dDepth/dX |     | dU/dX    dU/dY |-1T | dDepth/dX |
    //                 =                                     =                                      =
    //  | dDepth/dV |    | dY/dU    dY/dV |   | dDepth/dY |     | dV/dX    dV/dY |    | dDepth/dY |
    //
    //  | A B |-1   | D  -B |                      | A B |-1T   | D  -C |                                   
    //            =           / det                           =           / det                    
    //  | C D |     |-C   A |                      | C D |      |-B   A |
    //
    //  | dDepth/dU |           | dV/dY   -dV/dX |  | dDepth/dX |
    //                 = 1/det                                       
    //  | dDepth/dV |           |-dU/dY    dU/dX |  | dDepth/dY |

    float2 biasUV;
    //               dV/dY       V      dDepth/dX    D       dV/dX       V     dDepth/dY     D
    biasUV.x =   ShadowUVDepthDY.y * ShadowUVDepthDX.z - ShadowUVDepthDX.y * ShadowUVDepthDY.z;
    //               dU/dY       U      dDepth/dX    D       dU/dX       U     dDepth/dY     D
    biasUV.y = - ShadowUVDepthDY.x * ShadowUVDepthDX.z + ShadowUVDepthDX.x * ShadowUVDepthDY.z;

    float Det = (ShadowUVDepthDX.x * ShadowUVDepthDY.y) - (ShadowUVDepthDX.y * ShadowUVDepthDY.x);
	biasUV /= sign(Det) * max( abs(Det), 1e-20 );
    //biasUV = abs(Det) > 1e-7 ? biasUV / abs(Det) : 0;// sign(Det) * max( abs(Det), 1e-10 );
    return biasUV;
}


float ComputeShadowAmount(in float3 f3PosInLightViewSpace, in float fCameraSpaceZ, out int Cascade)
{
    float3 f3PosInCascadeProjSpace = float3(0.0, 0.0, 0.0), f3CascadeLightSpaceScale = float3(0.0, 0.0, 0.0);
    FindCascade( f3PosInLightViewSpace.xyz, fCameraSpaceZ, f3PosInCascadeProjSpace, f3CascadeLightSpaceScale, Cascade);
    if( Cascade == NUM_SHADOW_CASCADES )
        return 1.0;

    float3 f3ShadowMapUVDepth;
    f3ShadowMapUVDepth.xy = NormalizedDeviceXYToTexUV( f3PosInCascadeProjSpace.xy );
    f3ShadowMapUVDepth.z = NormalizedDeviceZToDepth( f3PosInCascadeProjSpace.z );
        
    float3 f3ddXShadowMapUVDepth = ddx(f3PosInLightViewSpace) * f3CascadeLightSpaceScale * F3NDC_XYZ_TO_UVD_SCALE;
    float3 f3ddYShadowMapUVDepth = ddy(f3PosInLightViewSpace) * f3CascadeLightSpaceScale * F3NDC_XYZ_TO_UVD_SCALE;

    float2 f2DepthSlopeScaledBias = ComputeReceiverPlaneDepthBias(f3ddXShadowMapUVDepth, f3ddYShadowMapUVDepth);
    uint SMWidth, SMHeight, Elems; 
    g_tex2DShadowMap.GetDimensions(SMWidth, SMHeight, Elems);
    float2 ShadowMapDim = float2(SMWidth, SMHeight);
    f2DepthSlopeScaledBias /= ShadowMapDim.xy;

    float fractionalSamplingError = dot( float2(1.f, 1.f), abs(f2DepthSlopeScaledBias.xy) );
    f3ShadowMapUVDepth.z -= fractionalSamplingError;
    
    float fLightAmount = g_tex2DShadowMap.SampleCmp( g_tex2DShadowMap_sampler, float3(f3ShadowMapUVDepth.xy, Cascade), float( f3ShadowMapUVDepth.z ) );

#if SMOOTH_SHADOWS
        float2 Offsets[] = 
        {
            float2(-1.,-1.),
            float2(+1.,-1.),
            float2(-1.,+1.),
            float2(+1.,+1.),
        };
        [unroll]
        for(int i=0; i<4; ++i)
        {
            float fDepthBias = dot(Offsets[i].xy, f2DepthSlopeScaledBias.xy);
            float2 f2Offset = Offsets[i].xy /  ShadowMapDim.xy;
            fLightAmount += g_tex2DShadowMap.SampleCmp( g_tex2DShadowMap_sampler, float3(f3ShadowMapUVDepth.xy + f2Offset.xy, Cascade), f3ShadowMapUVDepth.z + fDepthBias );
        }
        fLightAmount /= 5.0;
#endif
    return fLightAmount;
}

void CombineMaterials(in float4 MtrlWeights,
                      in float2 f2TileUV,
                      out float3 SurfaceColor,
                      out float3 SurfaceNormalTS)
{
    SurfaceColor = float3(0.0, 0.0, 0.0);
    SurfaceNormalTS = float3(0.0, 0.0, 1.0);

    // Normalize weights and compute base material weight
    MtrlWeights /= max( dot(MtrlWeights, float4(1.0, 1.0, 1.0, 1.0)) , 1.0 );
    float BaseMaterialWeight = saturate(1.0 - dot(MtrlWeights, float4(1.0, 1.0, 1.0, 1.0)));
    
    // The mask is already sharp

    ////Sharpen the mask
    //float2 TmpMin2 = min(MtrlWeights.rg, MtrlWeights.ba);
    //float Min = min(TmpMin2.r, TmpMin2.g);
    //Min = min(Min, BaseMaterialWeight);
    //float p = 4;
    //BaseMaterialWeight = pow(BaseMaterialWeight-Min, p);
    //MtrlWeights = pow(MtrlWeights-Min, p);
    //float NormalizationFactor = dot(MtrlWeights, float4(1,1,1,1)) + BaseMaterialWeight;
    //MtrlWeights /= NormalizationFactor;
    //BaseMaterialWeight /= NormalizationFactor;

	// Get diffuse color of the base material
    float3 BaseMaterialDiffuse = g_tex2DTileDiffuse[0].Sample( g_tex2DTileDiffuse_sampler, f2TileUV.xy / g_TerrainAttribs.m_fBaseMtrlTilingScale );
    float3 MaterialColors[NUM_TILE_TEXTURES];

    // Get tangent space normal of the base material
#if TEXTURING_MODE == TM_MATERIAL_MASK_NM
    float3 BaseMaterialNormal = g_tex2DTileNM[0].Sample(g_tex2DTileNM_sampler, f2TileUV.xy / g_TerrainAttribs.m_fBaseMtrlTilingScale);
    float3 MaterialNormals[NUM_TILE_TEXTURES];
#endif

    float4 f4TilingScale = g_TerrainAttribs.m_f4TilingScale;
    float fTilingScale[5] = {0.0, f4TilingScale.x, f4TilingScale.y, f4TilingScale.z, f4TilingScale.w};
    // Load material colors and normals
    const float fThresholdWeight = 3.f/256.f;
    MaterialColors[1] = MtrlWeights.x > fThresholdWeight ? g_tex2DTileDiffuse[1].Sample(g_tex2DTileDiffuse_sampler, f2TileUV.xy  / fTilingScale[1]) : float3(0.0, 0.0, 0.0);
    MaterialColors[2] = MtrlWeights.y > fThresholdWeight ? g_tex2DTileDiffuse[2].Sample(g_tex2DTileDiffuse_sampler, f2TileUV.xy  / fTilingScale[2]) : float3(0.0, 0.0, 0.0);
    MaterialColors[3] = MtrlWeights.z > fThresholdWeight ? g_tex2DTileDiffuse[3].Sample(g_tex2DTileDiffuse_sampler, f2TileUV.xy  / fTilingScale[3]) : float3(0.0, 0.0, 0.0);
    MaterialColors[4] = MtrlWeights.w > fThresholdWeight ? g_tex2DTileDiffuse[4].Sample(g_tex2DTileDiffuse_sampler, f2TileUV.xy  / fTilingScale[4]) : float3(0.0, 0.0, 0.0);

#if TEXTURING_MODE == TM_MATERIAL_MASK_NM
    MaterialNormals[1] = MtrlWeights.x > fThresholdWeight ? g_tex2DTileNM[1].Sample(g_tex2DTileNM_sampler, f2TileUV.xy  / fTilingScale[1]) : float3(0.0, 0.0, 1.0);
    MaterialNormals[2] = MtrlWeights.y > fThresholdWeight ? g_tex2DTileNM[2].Sample(g_tex2DTileNM_sampler, f2TileUV.xy  / fTilingScale[2]) : float3(0.0, 0.0, 1.0);
    MaterialNormals[3] = MtrlWeights.z > fThresholdWeight ? g_tex2DTileNM[3].Sample(g_tex2DTileNM_sampler, f2TileUV.xy  / fTilingScale[3]) : float3(0.0, 0.0, 1.0);
    MaterialNormals[4] = MtrlWeights.w > fThresholdWeight ? g_tex2DTileNM[4].Sample(g_tex2DTileNM_sampler, f2TileUV.xy  / fTilingScale[4]) : float3(0.0, 0.0, 1.0);
#endif
    // Blend materials and normals using the weights
    SurfaceColor = BaseMaterialDiffuse.rgb * BaseMaterialWeight + 
        MaterialColors[1] * MtrlWeights.x + 
        MaterialColors[2] * MtrlWeights.y + 
        MaterialColors[3] * MtrlWeights.z + 
        MaterialColors[4] * MtrlWeights.w;

#if TEXTURING_MODE == TM_MATERIAL_MASK_NM
    SurfaceNormalTS = BaseMaterialNormal * BaseMaterialWeight + 
        MaterialNormals[1] * MtrlWeights.x + 
        MaterialNormals[2] * MtrlWeights.y + 
        MaterialNormals[3] * MtrlWeights.z + 
        MaterialNormals[4] * MtrlWeights.w;
    SurfaceNormalTS = SurfaceNormalTS * float3(2.0, 2.0, 2.0) - float3(1.0, 1.0, 1.0);
#endif
}

void HemispherePS(in float4 f4Pos : SV_Position,
                  HemisphereVSOutput VSOut,
                  // IMPORTANT: non-system generated pixel shader input
                  // arguments must have the exact same name as vertex shader 
                  // outputs and must go in the same order.
                  
                  out float4 f4OutColor : SV_Target)
{
    float3 EarthNormal = normalize(VSOut.f3Normal);
    float3 EarthTangent = normalize(VSOut.f3Tangent);
    float3 EarthBitangent = normalize(VSOut.f3Bitangent);
    float3 f3TerrainNormal;
    f3TerrainNormal.xz = g_tex2DNormalMap.Sample(g_tex2DNormalMap_sampler, VSOut.f2MaskUV0.xy).xy * float2(2.0,2.0) - float2(1.0,1.0);
    // Since UVs are mirrored, we have to adjust normal coords accordingly:
    float2 f2XZSign = sign( float2(0.5,0.5) - frac(VSOut.f2MaskUV0.xy/2.0) );
    f3TerrainNormal.xz *= f2XZSign;

    f3TerrainNormal.y = sqrt( saturate(1.0 - dot(f3TerrainNormal.xz,f3TerrainNormal.xz)) );
    //float3 Tangent   = normalize(float3(1,0,VSOut.HeightMapGradients.x));
    //float3 Bitangent = normalize(float3(0,1,VSOut.HeightMapGradients.y));
    f3TerrainNormal = normalize( mul(f3TerrainNormal, float3x3(EarthTangent, EarthNormal, EarthBitangent)) );

    float4 MtrlWeights = g_tex2DMtrlMap.Sample( g_tex2DMtrlMap_sampler, VSOut.f2MaskUV0.xy );

    float3 SurfaceColor, SurfaceNormalTS = float3(0.0, 0.0, 1.0);
    CombineMaterials(MtrlWeights, VSOut.TileTexUV, SurfaceColor, SurfaceNormalTS);

    float3 f3TerrainTangent = normalize( cross(f3TerrainNormal, float3(0.0, 0.0, 1.0)) );
    float3 f3TerrainBitangent = normalize( cross(f3TerrainTangent, f3TerrainNormal) );
    float3 f3Normal = normalize(  SurfaceNormalTS.x * f3TerrainTangent + SurfaceNormalTS.z * f3TerrainNormal + SurfaceNormalTS.y * f3TerrainBitangent );


    // Attenuate extraterrestrial sun color with the extinction factor
    float3 f3SunLight = g_LightAttribs.f4ExtraterrestrialSunColor.rgb * VSOut.f3SunLightExtinction;
    // Ambient sky light is not pre-multiplied with the sun intensity
    float3 f3AmbientSkyLight = g_LightAttribs.f4ExtraterrestrialSunColor.rgb * VSOut.f3AmbientSkyLight;
    // Account for occlusion by the ground plane
    f3AmbientSkyLight *= saturate((1.0 + dot(EarthNormal, f3Normal))/2.f);

    // We need to divide diffuse color by PI to get the reflectance value
    float3 SurfaceReflectance = SurfaceColor * EARTH_REFLECTANCE / PI;

    int Cascade = 0;
    float fLightAmount = ComputeShadowAmount( VSOut.f3PosInLightViewSpace.xyz, VSOut.fCameraSpaceZ, Cascade );
    float DiffuseIllumination = max(0.0, dot(f3Normal, g_LightAttribs.f4DirOnLight.xyz));
    
    float3 f3CascadeColor = float3(0.0, 0.0, 0.0);
    if( g_LightAttribs.ShadowAttribs.bVisualizeCascades )
    {
        f3CascadeColor = (Cascade < NUM_SHADOW_CASCADES ? g_TerrainAttribs.f4CascadeColors[Cascade].rgb : float3(1.0, 1.0, 1.0)) / 8.0;
    }
    
    f4OutColor.rgb = f3CascadeColor +  SurfaceReflectance * (fLightAmount*DiffuseIllumination*f3SunLight + f3AmbientSkyLight);
    f4OutColor.a = 1.0;
}
