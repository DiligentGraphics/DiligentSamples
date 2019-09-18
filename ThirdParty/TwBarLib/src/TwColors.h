//  ---------------------------------------------------------------------------
//
//  @file       TwColors.h
//  @brief      Color conversions
//  @author     Philippe Decaudin
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  note:       Private header
//
//  ---------------------------------------------------------------------------


#if !defined ANT_TW_COLORS_INCLUDED
#define ANT_TW_COLORS_INCLUDED


//  ---------------------------------------------------------------------------


typedef unsigned int color32;


const color32 COLOR32_BLACK     = 0xff000000;   // Black 
const color32 COLOR32_WHITE     = 0xffffffff;   // White 
const color32 COLOR32_ZERO      = 0x00000000;   // Zero 
const color32 COLOR32_RED       = 0xffff0000;   // Red 
const color32 COLOR32_GREEN     = 0xff00ff00;   // Green 
const color32 COLOR32_BLUE      = 0xff0000ff;   // Blue 
   

template <typename Type> inline const Type TClamp(const Type& X, const Type& Limit1, const Type& Limit2)
{
    if( Limit1<Limit2 )
        return (X<=Limit1) ? Limit1 : ( (X>=Limit2) ? Limit2 : X );
    else
        return (X<=Limit2) ? Limit2 : ( (X>=Limit1) ? Limit1 : X );
}

inline color32 Color32FromARGBi(int A, int R, int G, int B)
{
    return (((color32)TClamp(A, 0, 255))<<24) | (((color32)TClamp(R, 0, 255))<<16) | (((color32)TClamp(G, 0, 255))<<8) | ((color32)TClamp(B, 0, 255));
}

inline color32 Color32FromARGBf(float A, float R, float G, float B)
{
    return (((color32)TClamp(A*256.0f, 0.0f, 255.0f))<<24) | (((color32)TClamp(R*256.0f, 0.0f, 255.0f))<<16) | (((color32)TClamp(G*256.0f, 0.0f, 255.0f))<<8) | ((color32)TClamp(B*256.0f, 0.0f, 255.0f));
}

inline void Color32ToARGBi(color32 Color, int *A, int *R, int *G, int *B)
{
    if(A) *A = (Color>>24)&0xff;
    if(R) *R = (Color>>16)&0xff;
    if(G) *G = (Color>>8)&0xff;
    if(B) *B = Color&0xff;
}

inline void Color32ToARGBf(color32 Color, float *A, float *R, float *G, float *B)
{
    if(A) *A = (1.0f/255.0f)*float((Color>>24)&0xff);
    if(R) *R = (1.0f/255.0f)*float((Color>>16)&0xff);
    if(G) *G = (1.0f/255.0f)*float((Color>>8)&0xff);
    if(B) *B = (1.0f/255.0f)*float(Color&0xff);
}

void ColorRGBToHLSf(float R, float G, float B, float *Hue, float *Light, float *Saturation);

void ColorRGBToHLSi(int R, int G, int B, int *Hue, int *Light, int *Saturation);

void ColorHLSToRGBf(float Hue, float Light, float Saturation, float *R, float *G, float *B);

void ColorHLSToRGBi(int Hue, int Light, int Saturation, int *R, int *G, int *B);

color32 ColorBlend(color32 Color1, color32 Color2, float S);


//  ---------------------------------------------------------------------------


#endif // !defined ANT_TW_COLORS_INCLUDED
