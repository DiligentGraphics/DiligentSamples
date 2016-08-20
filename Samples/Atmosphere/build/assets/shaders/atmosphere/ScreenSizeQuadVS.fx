#include "AtmosphereShadersCommon.fxh"

void ScreenSizeQuadVS(in uint VertexId : SV_VertexID,
                      in uint InstID : SV_InstanceID,
                      out ScreenSizeQuadVSOutput VSOut,
                      // IMPORTANT: non-system generated pixel shader input
                      // arguments must have the exact same name as vertex shader 
                      // outputs and must go in the same order.
                      // Moreover, even if the shader is not using the argument,
                      // it still must be declared.

                      out float4 f4Pos : SV_Position)
{
    float4 Bounds = float4(-1.0, -1.0, 1.0, 1.0);
    
    float2 PosXY[4] = 
    {
        Bounds.xy,
        Bounds.xw,
        Bounds.zy,
        Bounds.zw
    };

    float2 f2XY = PosXY[VertexId];
    VSOut.m_f2PosPS = f2XY;
    VSOut.m_fInstID = float( InstID );
    
    // Write 0 to the depth buffer
    // NDC_MIN_Z ==  0 in DX
    // NDC_MIN_Z == -1 in GL
    float z = NDC_MIN_Z;
    f4Pos = float4(f2XY, z, 1.0);
}
