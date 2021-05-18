#include "structures.fxh"

ConstantBuffer<GlobalConstants> g_Constants;
ConstantBuffer<ObjectConstants> g_ObjectConst;
StructuredBuffer<ObjectAttribs> g_ObjectAttribs;

struct VSInput
{
    float3 Pos  : ATTRIB0;
    float3 Norm : ATTRIB1;
    float2 UV   : ATTRIB2;
};

struct PSInput
{
    float4 Pos  : SV_POSITION;
    float4 WPos : WORLD_POS;
    float3 Norm : NORMAL; // world-space normal
    float2 UV   : TEX_COORD0;
    nointerpolation uint MatId : TEX_COORD1;
};

void main(in VSInput  VSIn,
          in uint     InstanceId : SV_InstanceID,
          out PSInput PSIn)
{
    ObjectAttribs Obj = g_ObjectAttribs[g_ObjectConst.ObjectAttribsOffset + InstanceId];

    PSIn.WPos  = mul(float4(VSIn.Pos, 1.0), Obj.ModelMat);
    PSIn.Pos   = mul(PSIn.WPos, g_Constants.ViewProj);
    PSIn.Norm  = normalize(mul(VSIn.Norm, (float3x3)Obj.NormalMat));
    PSIn.UV    = VSIn.UV;
    PSIn.MatId = Obj.MaterialId;
}
