#ifndef _STRUCTURES_FXH_
#define _STRUCTURES_FXH_

struct LightAttribs
{
	float2 f2PosXZ;
	float2 f2SizeXZ;
	float4 f4Intensity;
};

struct ShaderConstants
{
	uint  uScreenWidth;
	uint  uScreenHeight;
	float fScreenWidth;
	float fScreenHeight;

	float fLastSampleCount;
	float fCurrSampleCount;
	int   iNumBounces;
	int   iNumSamplesPerFrame;

	uint uFrameSeed1;
	uint uFrameSeed2;
	int  iShowOnlyLastBounce;
	int  Padding0;

	float4x4 ViewProjMat;
	float4x4 ViewProjInvMat;

	LightAttribs Light;
};

#endif // _STRUCTURES_FXH_
