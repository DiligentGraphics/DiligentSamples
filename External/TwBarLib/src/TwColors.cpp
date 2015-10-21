//  ---------------------------------------------------------------------------
//
//  @file       TwColors.cpp
//  @author     Philippe Decaudin
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  ---------------------------------------------------------------------------


#include "TwPrecomp.h"
#include "TwColors.h"


void ColorRGBToHLSf(float R, float G, float B, float *Hue, float *Light, float *Saturation)
{
    // Compute HLS from RGB. The r,g,b triplet is between [0,1], 
    // hue is between [0,360], light and saturation are [0,1].

    float rnorm, gnorm, bnorm, minval, maxval, msum, mdiff, r, g, b;
    r = g = b = 0;
    if(R>0) r = R; if(r>1) r = 1;
    if(G>0) g = G; if(g>1) g = 1;
    if(B>0) b = B; if(b>1) b = 1;

    minval = r;
    if(g<minval) minval = g;
    if(b<minval) minval = b;
    maxval = r;
    if(g>maxval) maxval = g;
    if(b>maxval) maxval = b;

    rnorm = gnorm = bnorm = 0;
    mdiff = maxval - minval;
    msum  = maxval + minval;
    float l = 0.5f * msum;
    if(Light) 
        *Light = l;
    if(maxval!=minval) 
    {
        rnorm = (maxval - r)/mdiff;
        gnorm = (maxval - g)/mdiff;
        bnorm = (maxval - b)/mdiff;
    } 
    else 
    {
        if(Saturation)
            *Saturation = 0;
        if(Hue)
            *Hue = 0;
        return;
    }

    if(Saturation)
    {
        if(l<0.5f)
            *Saturation = mdiff/msum;
        else
            *Saturation = mdiff/(2.0f - msum);
    }

    if(Hue)
    {
        if(r==maxval)
            *Hue = 60.0f * (6.0f + bnorm - gnorm);
        else if(g==maxval)
            *Hue = 60.0f * (2.0f + rnorm - bnorm);
        else
            *Hue = 60.0f * (4.0f + gnorm - rnorm);

        if(*Hue>360.0f)
            *Hue -= 360.0f;
    }
}


void ColorRGBToHLSi(int R, int G, int B, int *Hue, int *Light, int *Saturation)
{
    float h, l, s;
    ColorRGBToHLSf((1.0f/255.0f)*float(R), (1.0f/255.0f)*float(G), (1.0f/255.0f)*float(B), &h, &l, &s);
    if(Hue)        *Hue       = (int)TClamp(h*(256.0f/360.0f), 0.0f, 255.0f);
    if(Light)      *Light     = (int)TClamp(l*256.0f, 0.0f, 255.0f);
    if(Saturation) *Saturation= (int)TClamp(s*256.0f, 0.0f, 255.0f);
}


void ColorHLSToRGBf(float Hue, float Light, float Saturation, float *R, float *G, float *B)
{
    // Compute RGB from HLS. The light and saturation are between [0,1]
    // and hue is between [0,360]. The returned r,g,b triplet is between [0,1].

    // a local auxiliary function
    struct CLocal
    {
        static float HLSToRGB(float Rn1, float Rn2, float Huei)
        {
            float hue = Huei;
            if(hue>360) hue = hue - 360;
            if(hue<0)   hue = hue + 360;
            if(hue<60 ) return Rn1 + (Rn2-Rn1)*hue/60;
            if(hue<180) return Rn2;
            if(hue<240) return Rn1 + (Rn2-Rn1)*(240-hue)/60;
            return Rn1;
        }
    };

    float rh, rl, rs, rm1, rm2;
    rh = rl = rs = 0;
    if(Hue>0)        rh = Hue;        if(rh>360) rh = 360;
    if(Light>0)      rl = Light;      if(rl>1)   rl = 1;
    if(Saturation>0) rs = Saturation; if(rs>1)   rs = 1;

    if(rl<=0.5f)
        rm2 = rl*(1.0f + rs);
    else
        rm2 = rl + rs - rl*rs;
    rm1 = 2.0f*rl - rm2;

    if(!rs) 
    { 
        if(R) *R = rl; 
        if(G) *G = rl; 
        if(B) *B = rl; 
    }
    else
    {
        if(R) *R = CLocal::HLSToRGB(rm1, rm2, rh+120);
        if(G) *G = CLocal::HLSToRGB(rm1, rm2, rh);
        if(B) *B = CLocal::HLSToRGB(rm1, rm2, rh-120);
    }
}


void ColorHLSToRGBi(int Hue, int Light, int Saturation, int *R, int *G, int *B)
{
    float r, g, b;
    ColorHLSToRGBf((360.0f/255.0f)*float(Hue), (1.0f/255.0f)*float(Light), (1.0f/255.0f)*float(Saturation), &r, &g, &b);
    if(R) *R = (int)TClamp(r*256.0f, 0.0f, 255.0f);
    if(G) *G = (int)TClamp(g*256.0f, 0.0f, 255.0f);
    if(B) *B = (int)TClamp(b*256.0f, 0.0f, 255.0f);
}


color32 ColorBlend(color32 Color1, color32 Color2, float S)
{
    float a1, r1, g1, b1, a2, r2, g2, b2;
    Color32ToARGBf(Color1, &a1, &r1, &g1, &b1);
    Color32ToARGBf(Color2, &a2, &r2, &g2, &b2);
    float t = 1.0f-S;
    return Color32FromARGBf(t*a1+S*a2, t*r1+S*r2, t*g1+S*g2, t*b1+S*b2);
}
