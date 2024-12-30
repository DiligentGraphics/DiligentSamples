struct OITLayers
{
    uint4 Data;
};

void UnpackOITLayers(in OITLayers Layers,
                     out uint D0, out uint D1, out uint D2, out uint D3, out uint D4,
                     out uint T0, out uint T1, out uint T2, out uint T3, out uint T4,
                     out uint TailCount)
{
    uint u16Mask = 0xFFFFu;
    uint u8Mask  = 0xFFu;
    
    D0 = (Layers.Data.x >>  0u) & u16Mask;
    D1 = (Layers.Data.x >> 16u) & u16Mask;
    D2 = (Layers.Data.y >>  0u) & u16Mask;
    D3 = (Layers.Data.y >> 16u) & u16Mask;
    D4 = (Layers.Data.z >>  0u) & u16Mask;

    T0 = (Layers.Data.z >> 16u) & u8Mask;
    T1 = (Layers.Data.z >> 24u) & u8Mask;
    T2 = (Layers.Data.w >>  0u) & u8Mask;
    T3 = (Layers.Data.w >>  8u) & u8Mask;
    T4 = (Layers.Data.w >> 16u) & u8Mask;

    TailCount = (Layers.Data.w >> 24u) & u8Mask; 
}

OITLayers PackOITLayers(uint D0, uint D1, uint D2, uint D3, uint D4,
                        uint T0, uint T1, uint T2, uint T3, uint T4,
                        uint TailCount)
{
    OITLayers Layers;
   
    Layers.Data.x = D0 | (D1 << 16u);
    Layers.Data.y = D2 | (D3 << 16u);
    Layers.Data.z = D4 | (T0 << 16u) | (T1 << 24u);
    Layers.Data.w = T2 | (T3 <<  8u) | (T4 << 16u) | (TailCount << 24u);
 
    return Layers;
}

OITLayers AddOITLayer(in OITLayers Layers, float Depth, float Transmittance)
{
    uint D = uint(clamp(Depth, 0.0, 1.0) * 65535.0);
    uint T = uint(clamp(Transmittance, 0.0, 1.0) * 255.0);

    uint D0, D1, D2, D3, D4;
    uint T0, T1, T2, T3, T4;
    uint TailCount;
    UnpackOITLayers(Layers,
                    D0, D1, D2, D3, D4,
                    T0, T1, T2, T3, T4,
                    TailCount);
    
    if (D > D3)
    {
        //  D0      D1      D2      D3      D4      D
        //  |-------|-------|-------|-------|-------|       D > D4
        //  T0      T1      T2      T3      T4      T
        //
        //  D0      D1      D2      D3      D       D4
        //  |-------|-------|-------|-------|-------|       D <= D4
        //  T0      T1      T2      T3      T       T4  
        T4 = (T4 * T) >> 8u;
        D4 = min(D, D4);
        //  D0      D1      D2      D3      D4
        //  |-------|-------|-------|-------|
        //  T0      T1      T2      T3     T4*T
    }
    else if (D == D3)
    {
        T3 = (T3 * T) >> 8u;
    }
    else if (D == D2)
    {
        T2 = (T2 * T) >> 8u;
    }
    else if (D == D1)
    {
        T1 = (T1 * T) >> 8u;
    }
    else if (D == D0)
    {
        T0 = (T0 * T) >> 8u;
    }
    else // D < D3
    {
        T4 = (T4 * T3) >> 8u;
        D4 = D3;
        //  D0      D1      D2      D3      D3
        //  |-------|-------|-------|-------|
        //  T0      T1      T2      T3     T3*T4
        if (D > D2)
        {
            T3 = T;
            D3 = D;
            //  D0      D1      D2      D       D3
            //  |-------|-------|-------|-------|
            //  T0      T1      T2      T      T3*T4
        }
        else // D < D2
        {
            T3 = T2;
            D3 = D2;
            //  D0      D1      D2      D2      D3
            //  |-------|-------|-------|-------|
            //  T0      T1      T2      T2     T3*T4
            if (D > D1)
            {
                T2 = T;
                D2 = D;
                //  D0      D1      D       D2      D3
                //  |-------|-------|-------|-------|
                //  T0      T1      T       T2     T3*T4
            }
            else // D < D1
            {
                T2 = T1;
                D2 = D1;
                //  D0      D1      D1      D2      D3
                //  |-------|-------|-------|-------|
                //  T0      T1      T1      T2     T3*T4
                if (D > D0)
                {
                    T1 = T;
                    D1 = D;
                    //  D0      D       D1      D2      D3
                    //  |-------|-------|-------|-------|
                    //  T0      T       T1      T2     T3*T4
                }
                else // D < D0
                {
                    D1 = D0;
                    T1 = T0;
                    T0 = T;
                    D0 = D;
                    //  D       D0      D1      D2      D3
                    //  |-------|-------|-------|-------|
                    //  T       T0      T1      T2     T3*T4
                }
            }
        }
    }
    
    TailCount = min(TailCount + 1u, 255u);
    
    return PackOITLayers(D0, D1, D2, D3, D4,
                         T0, T1, T2, T3, T4,
                         TailCount);
}

OITLayers GetDefaultOITLayers()
{
    uint D = 65535u;
    uint T = 255u;
    // After adding 4 layers, the tail count will wrap to 0,
    // so that the 5th layer will set the count to 1.
    uint TailCount = 256u - 4u;
    return PackOITLayers(D, D, D, D, D,
                         T, T, T, T, T,
                         TailCount); 
}

float GetOITTransmittance(in OITLayers Layers, float Depth, out float TailFactor)
{
    uint uD0, uD1, uD2, uD3, uD4;
    uint uT0, uT1, uT2, uT3, uT4;
    uint TailCount;
    UnpackOITLayers(Layers,
                    uD0, uD1, uD2, uD3, uD4,
                    uT0, uT1, uT2, uT3, uT4,
                    TailCount);

    float D0 = float(uD0);
    float D1 = float(uD1);
    float D2 = float(uD2);
    float D3 = float(uD3);
    float D4 = float(uD4);
    float D  = floor(Depth * 65535.0);
    
    // Note that we need to compute transmittance from the layer,
    // excluding the layer itself.
    float T0 = 1.0;
    float T1 = float(uT0) / 255.0;
    float T2 = float(uT1) / 255.0 * T1;
    float T3 = float(uT2) / 255.0 * T2;
    float T4 = float(uT3) / 255.0 * T3;
    float TB = float(uT4) / 255.0 * T4;
    
    float2 StartEndD;
    float2 StartEndT;
    if (D < D0)
    {
        StartEndD = float2(0.0, D0);
        StartEndT = float2(1.0, T0);
    }
    else if (D < D1)
    {
        StartEndD = float2(D0, D1);
        StartEndT = float2(T0, T1);
    }
    else if (D < D2)
    {
        StartEndD = float2(D1, D2);
        StartEndT = float2(T1, T2);
    }
    else if (D < D3)
    {
        StartEndD = float2(D2, D3);
        StartEndT = float2(T2, T3);
    }
    else if (D < D4)
    {
        StartEndD = float2(D3, D4);
        StartEndT = float2(T3, T4);
    }
    else
    {
        StartEndD = float2(D4, 1.0);
        StartEndT = float2(T4, TB);
    }
    
    TailFactor = lerp(1.0, 1.0 / max(float(TailCount), 1.0), clamp((D - D3) / max(D4 - D3, 1e-6), 0.0, 1.0));
    
    return lerp(StartEndT.x, StartEndT.y, clamp((D - StartEndD.x) / max(StartEndD.y - StartEndD.x, 1e-6), 0.0, 1.0));
}
