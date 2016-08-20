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

#include "Structures.fxh"



// Using static definitions instead of constant buffer variables is 
// more efficient because the compiler is able to optimize the code 
// more aggressively


#define MIN_MAX_DATA_FORMAT float2



cbuffer cbParticipatingMediaScatteringParams : register( b1 )
{
    AirScatteringAttribs g_MediaParams;
}

// Frame parameters
cbuffer cbCameraAttribs : register( b2 )
{
    CameraAttribs g_CameraAttribs;
}

cbuffer cbLightParams : register( b3 )
{
    LightAttribs g_LightAttribs;
}

cbuffer cbMiscDynamicParams : register( b4 )
{
    MiscDynamicParams g_MiscParams;
}

SamplerState samLinearClamp : register( s0 )
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

Texture2D<float>  g_tex2DDepthBuffer            : register( t0 );
Texture2D<float>  g_tex2DCamSpaceZ              : register( t0 );
Texture2D<float4> g_tex2DSliceEndPoints         : register( t4 );
Texture2D<float2> g_tex2DCoordinates            : register( t1 );
Texture2D<float>  g_tex2DEpipolarCamSpaceZ      : register( t2 );
Texture2D<uint2>  g_tex2DInterpolationSource    : register( t6 );
Texture2DArray<float> g_tex2DLightSpaceDepthMap : register( t3 );
Texture2D<float4> g_tex2DSliceUVDirAndOrigin    : register( t6 );
Texture2D<MIN_MAX_DATA_FORMAT> g_tex2DMinMaxLightSpaceDepth  : register( t4 );
Texture2D<float3> g_tex2DInitialInsctrIrradiance: register( t5 );
Texture2D<float4> g_tex2DColorBuffer            : register( t1 );
Texture2D<float3> g_tex2DScatteredColor         : register( t3 );
Texture2D<float2> g_tex2DOccludedNetDensityToAtmTop : register( t5 );
Texture2D<float3> g_tex2DEpipolarExtinction     : register( t6 );
Texture3D<float3> g_tex3DSingleSctrLUT          : register( t7 );
Texture3D<float3> g_tex3DHighOrderSctrLUT       : register( t8 );
Texture3D<float3> g_tex3DMultipleSctrLUT        : register( t9 );
Texture2D<float3> g_tex2DSphereRandomSampling   : register( t1 );
Texture3D<float3> g_tex3DPreviousSctrOrder      : register( t0 );
Texture3D<float3> g_tex3DPointwiseSctrRadiance  : register( t0 );
Texture2D<float>  g_tex2DAverageLuminance       : register( t10 );
Texture2D<float>  g_tex2DLowResLuminance        : register( t0 );


