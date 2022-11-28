#ifndef _STRUCTURES_FXH_
#define _STRUCTURES_FXH_

#define MAT_TYPE_NONE          0
#define MAT_TYPE_SMITH_GGX     1
#define MAT_TYPE_GLASS         2
#define MAT_TYPE_MIRROR        3
#define MAT_TYPE_DIFFUSE_LIGHT 4

#define NUM_BALLS 6

#define IOR_GBUFFER_SCALE 5.0

struct Material
{
    float4 BaseColor;
    float4 Emittance;

    int   Type;
    float Metallic;
    float Roughness;
    float IOR;
};

struct BoxInfo
{
    float3 Center;
    float3 Size;

    Material Mat;
};

struct SphereInfo
{
    float3 Center;
    float  Radius;

    Material Mat;
};

struct LightAttribs
{
	float2 f2PosXZ;
	float2 f2SizeXZ;
	float4 f4Intensity;
	float4 f4Normal;
};

struct SceneAttribs
{
	LightAttribs Light;
	SphereInfo   Balls[NUM_BALLS];
};

struct ShaderConstants
{
	uint2  u2ScreenSize;
	float2 f2ScreenSize;

	float fLastSampleCount;
	float fCurrSampleCount;
	int   iNumBounces;
	int   iNumSamplesPerFrame;

	uint uFrameSeed1;
	uint uFrameSeed2;
	int  iShowOnlyLastBounce;
	int  iUseNEE;

	float fBalanceHeuristicsPower;
	float fPadding0;
	float fPadding1;
	float fPadding2;

	float4x4 ViewProjMat;
	float4x4 ViewProjInvMat;
	float4   CameraPos;

	SceneAttribs Scene;
};

#endif // _STRUCTURES_FXH_
