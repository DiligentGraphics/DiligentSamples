
Texture2D g_Texture;
SamplerState g_Texture_sampler; // By convention, texture samplers must use the '_sampler' suffix

struct PSInput
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};


void main(in PSInput PSIn,
          out PSOutput PSOut)
{    
    if (length(PSIn.Color.xyz) > 0.0f)
    {
        PSOut.Color = PSIn.Color;
    }
    else
    {
        PSOut.Color = g_Texture.Sample(g_Texture_sampler, PSIn.UV);
    }
    
}
