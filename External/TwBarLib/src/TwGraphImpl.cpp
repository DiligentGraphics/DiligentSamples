//  ---------------------------------------------------------------------------
//
//  @file       TwGraphImpl.cpp
//  @author     Philippe Decaudin, modified by Egor Yusov
//  @brief      This file is based on TwDirect3D11.cpp file from the original
//              AntTweakBar library. It uses Graphics Engine instead of D3D11
//              to implement rendering routines
//
//  ---------------------------------------------------------------------------


#include "TwPrecomp.h"
#include "TwGraphImpl.h"
#include "TwMgr.h"
#include "TwColors.h"
#include "MapHelper.h"

ITwGraph *CreateGraphInst()
{
    return new CTwGraphImpl;
}

using namespace std;
using namespace Diligent;

const char *g_ErrCreateShader    = "Shader creation failed";
const char *g_ErrCreateLayout    = "Vertex layout creation failed";
const char *g_ErrCreateBuffer    = "Buffer creation failed";
const char *g_ErrCreateSampler   = "Sampler state creation failed";

static const char* g_LineRectVS_DX = 
    #include "ShadersInc\LineRectVS_DX.h"
;

static const char* g_LineRectVS_GL = 
    #include "ShadersInc\LineRectVS_GL.h"
;

static const char* g_LineRectCstColorVS_DX = 
    #include "ShadersInc\LineRectCstColorVS_DX.h"
;

static const char* g_LineRectCstColorVS_GL = 
    #include "ShadersInc\LineRectCstColorVS_GL.h"
;

static const char* g_LineRectPS_DX = 
    #include "ShadersInc\LineRectPS_DX.h"
;

static const char* g_LineRectPS_GL = 
    #include "ShadersInc\LineRectPS_GL.h"
;

static const char* g_TextVS_DX = 
    #include "ShadersInc\TextVS_DX.h"
;

static const char* g_TextVS_GL = 
    #include "ShadersInc\TextVS_GL.h"
;

static const char* g_TextCstColorVS_DX = 
    #include "ShadersInc\TextCstColorVS_DX.h"
;

static const char* g_TextCstColorVS_GL = 
    #include "ShadersInc\TextCstColorVS_GL.h"
;

static const char* g_TextPS_DX = 
    #include "ShadersInc\TextPS_DX.h"
;

static const char* g_TextPS_GL = 
    #include "ShadersInc\TextPS_GL.h"
;


//  ---------------------------------------------------------------------------

bool operator == (const Rect& l, const Rect& r) 
{ 
    return r.left   == l.left  && 
           r.right  == l.right && 
           r.top    == l.top   && 
           r.bottom == l.bottom; 
}

//  ---------------------------------------------------------------------------

static void BindFont(IRenderDevice *_Dev, const CTexFont *_Font, ITexture**_Tex)
{
    assert(_Font!=NULL);
    *_Tex = NULL;

    int w = _Font->m_TexWidth;
    int h = _Font->m_TexHeight;
    color32 *font32 = new color32[w*h];
    color32 *p = font32;
    for( int i=0; i<w*h; ++i, ++p )
        *p = 0x00ffffff | (((color32)(_Font->m_TexBytes[i]))<<24);

    TextureDesc desc;
    desc.Type = TEXTURE_TYPE_2D;
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = TEX_FORMAT_RGBA8_UNORM;
    desc.SampleCount = 1;
    desc.Usage = USAGE_STATIC;
    desc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData SubRes0Data;
    SubRes0Data.pData = font32;
    SubRes0Data.Stride = w*sizeof(color32);
    TextureData TexData;
    TexData.NumSubresources = 1;
    TexData.pSubResources = &SubRes0Data;

    _Dev->CreateTexture( desc, TexData, _Tex );

    delete[] font32;
}

//  ---------------------------------------------------------------------------

static void UnbindFont(IRenderDevice *_Dev, RefCntAutoPtr<ITexture> &_Tex, IShaderVariable *pFontFar)
{
    (void)_Dev;
    _Tex.Release();
    pFontFar->Set( nullptr );
}

//  ---------------------------------------------------------------------------


int CTwGraphImpl::Init()
{
    assert(g_TwMgr!=NULL);
    assert(g_TwMgr->m_Device!=NULL);
    assert(g_TwMgr->m_ImmediateContext!=NULL);


    m_pDev = static_cast<IRenderDevice*>(g_TwMgr->m_Device);
    m_pDevImmContext = static_cast<IDeviceContext*>(g_TwMgr->m_ImmediateContext);
    const auto& DevCaps = m_pDev->GetDeviceCaps();
    m_bIsGLDevice = DevCaps.DevType != DeviceType::DirectX;

    m_Drawing = false;
    m_OffsetX = m_OffsetY = 0;
    m_FontTex = NULL;
    m_WndWidth = 0;
    m_WndHeight = 0;
    m_TrianglesVertexBufferCount = 0;

    try
    {
        // Create line vertex buffer
        BufferDesc BuffDesc;
        BuffDesc.Name = "AntTwBar: Line VB";
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.uiSizeInBytes = 2 * sizeof( CLineRectVtx );
        BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDev->CreateBuffer( BuffDesc, BufferData(), &m_pLineVertexBuffer );

        // Create rect vertex buffer
        BuffDesc.Name = "AntTwBar: Rect VB";
        BuffDesc.uiSizeInBytes = 4 * sizeof(CLineRectVtx);
        m_pDev->CreateBuffer(BuffDesc, BufferData(), &m_pRectVertexBuffer);

        // Create constant buffer
        BuffDesc.Name = "AntTwBar: const buff";
        BuffDesc.uiSizeInBytes = sizeof(CConstants);
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        m_pDev->CreateBuffer(BuffDesc, BufferData(), &m_pConstantBuffer);
    }
    catch(const std::runtime_error& )
    {
        g_TwMgr->SetLastError(g_ErrCreateBuffer);
        Shut();
        return 0;
    }
    ResourceMappingDesc ResMpDesc;
    ResourceMappingEntry pResMpEntries[] = { { "Constants", m_pConstantBuffer }, {nullptr, nullptr} };
    ResMpDesc.pEntries = pResMpEntries;
    m_pDev->CreateResourceMapping( ResMpDesc, &m_pResourceMapping );

    try
    {
        bool bIsDX = m_pDev->GetDeviceCaps().DevType == DeviceType::DirectX;
        ShaderCreationAttribs CreationAttribs;

        CreationAttribs.Source = bIsDX ? g_LineRectVS_DX : g_LineRectVS_GL;
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CreationAttribs.Desc.Name = "AntTwBar: LineRectVS";
        m_pDev->CreateShader( CreationAttribs, &m_pLineRectVS );
        m_pLineRectVS->BindResources( m_pResourceMapping, 0 );

        CreationAttribs.Source = bIsDX ? g_LineRectCstColorVS_DX : g_LineRectCstColorVS_GL;
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CreationAttribs.Desc.Name = "AntTwBar: LineRectCstColorVS";
        m_pDev->CreateShader( CreationAttribs, &m_pLineRectCstColorVS );
        m_pLineRectCstColorVS->BindResources( m_pResourceMapping, 0 );

        CreationAttribs.Source = bIsDX ? g_LineRectPS_DX : g_LineRectPS_GL;
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
        CreationAttribs.Desc.Name = "AntTwBar: LineRectPS";
        m_pDev->CreateShader( CreationAttribs, &m_pLineRectPS );
        m_pLineRectPS->BindResources( m_pResourceMapping, 0 );

        CreationAttribs.Source = bIsDX ? g_TextVS_DX : g_TextVS_GL;
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CreationAttribs.Desc.Name = "AntTwBar: TexVS";
        m_pDev->CreateShader( CreationAttribs, &m_pTextVS );
        m_pTextVS->BindResources( m_pResourceMapping, 0 );

        CreationAttribs.Source = bIsDX ? g_TextCstColorVS_DX : g_TextCstColorVS_GL;
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CreationAttribs.Desc.Name = "AntTwBar: TextCstColorVS";
        m_pDev->CreateShader( CreationAttribs, &m_pTextCstColorVS );
        m_pTextCstColorVS->BindResources( m_pResourceMapping, 0 );

        CreationAttribs.Source = bIsDX ? g_TextPS_DX : g_TextPS_GL;
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
        CreationAttribs.Desc.Name = "AntTwBar: TextPS";
        m_pDev->CreateShader( CreationAttribs, &m_pTextPS );
        m_pTextPS->BindResources( m_pResourceMapping, 0 );
        m_psvFont = m_pTextPS->GetShaderVariable( "g_Font" );
    }
    catch( const std::runtime_error& )
    {
        g_TwMgr->SetLastError( g_ErrCreateShader );
        Shut();
        return 0;
    }

    try
    {
        LayoutDesc lineRectLayout;
        lineRectLayout.Name = "AntTwBar: vertex layout";
        LayoutElement Elems[] = 
        {
            LayoutElement( 0, 0, 3, VT_FLOAT32, False ),
            LayoutElement( 1, 0, 4, VT_UINT8, True )
        };
        lineRectLayout.LayoutElements = Elems;
        lineRectLayout.NumElements = _countof( Elems );
        m_pDev->CreateVertexDescription( lineRectLayout, m_pLineRectVS, &m_pLineRectVertexLayout );
    }
    catch( const std::runtime_error& )
    {
        g_TwMgr->SetLastError(g_ErrCreateLayout);
        Shut();
        return 0;
    }


    // Create sampler
    SamplerDesc sd;
    sd.Name = "AntTwBar: Point Border sampler";
    sd.AddressU = sd.AddressV = sd.AddressW = TEXTURE_ADDRESS_BORDER;
    sd.BorderColor[0] = sd.BorderColor[1] = sd.BorderColor[2] = sd.BorderColor[3] = 0;
    sd.MinFilter = sd.MagFilter = sd.MipFilter = FILTER_TYPE_POINT;
    sd.MaxLOD = sd.MinLOD = 0;
    try
    {
        m_pDev->CreateSampler(sd, &m_pSamplerState);
    }
    catch(const std::runtime_error& )
    {
        g_TwMgr->SetLastError(g_ErrCreateSampler);
        Shut();
        return 0;
    }


    try
    {
        LayoutDesc lineRectLayout;
        lineRectLayout.Name = "AntTwBar: line rect vertex layout";
        LayoutElement Elems[] = 
        {
            LayoutElement( 0, 0, 3, VT_FLOAT32, False ),
            LayoutElement( 1, 0, 4, VT_UINT8, True ),
            LayoutElement( 2, 0, 2, VT_FLOAT32, False ),
        };
        lineRectLayout.LayoutElements = Elems;
        lineRectLayout.NumElements = _countof( Elems );
        m_pDev->CreateVertexDescription( lineRectLayout, m_pTextVS, &m_pTextVertexLayout );
    }
    catch( const std::runtime_error& )
    {
        g_TwMgr->SetLastError(g_ErrCreateLayout);
        Shut();
        return 0;
    }

    DepthStencilStateDesc DSSDesc;
    DSSDesc.Name = "AntTwBar: Disable depth DSS";
    DSSDesc.DepthEnable = False;
    DSSDesc.DepthWriteEnable = False;
    m_pDev->CreateDepthStencilState(DSSDesc, &m_pDepthStencilState);

    BlendStateDesc BSDesc;
    BSDesc.Name = "AntTwBar: alpha blend state";
    BSDesc.IndependentBlendEnable = False;
    auto &RT0 = BSDesc.RenderTargets[0];
    RT0.BlendEnable = True;
    RT0.RenderTargetWriteMask = COLOR_MASK_ALL;
    RT0.SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOp     =  BLEND_OPERATION_ADD;
    RT0.SrcBlendAlpha   = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlendAlpha  = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOpAlpha    = BLEND_OPERATION_ADD;
    m_pDev->CreateBlendState(BSDesc, &m_pBlendState);
    
    // Create rasterizer state object
    RasterizerStateDesc RSDesc;
    RSDesc.Name = "AntTwBar: solid fill no cull RS";
    RSDesc.FillMode = FILL_MODE_SOLID;
    RSDesc.CullMode = CULL_MODE_NONE;
    RSDesc.FrontCounterClockwise = True;
    RSDesc.ScissorEnable = True;
    //RSDesc.MultisampleEnable = false; // do not allow msaa (fonts would be degraded)
    RSDesc.AntialiasedLineEnable = False;
    m_pDev->CreateRasterizerState(RSDesc, &m_pRasterState);

    //rd.AntialiasedLineEnable = true;
    RSDesc.Name = "AntTwBar: solid fill no cull antialiased RS";
    m_pDev->CreateRasterizerState(RSDesc, &m_pRasterStateAntialiased);
    RSDesc.AntialiasedLineEnable = False;

    // the three following raster states allow msaa
    //RSDesc.MultisampleEnable = true;
    RSDesc.Name = "AntTwBar: solid fill no cull multisample RS";
    m_pDev->CreateRasterizerState(RSDesc, &m_pRasterStateMultisample);

    RSDesc.CullMode = CULL_MODE_BACK;
    RSDesc.Name = "AntTwBar: solid fill cull back RS";
    m_pDev->CreateRasterizerState(RSDesc, &m_pRasterStateCullCW);

    RSDesc.CullMode = CULL_MODE_FRONT;
    RSDesc.Name = "AntTwBar: solid fill cull front RS";
    m_pDev->CreateRasterizerState(RSDesc, &m_pRasterStateCullCCW);

    return 1;
}

//  ---------------------------------------------------------------------------

int CTwGraphImpl::Shut()
{
    assert(m_Drawing==false);

    UnbindFont(m_pDev, m_pFontGPUTex, m_psvFont);

    m_pDepthStencilState.Release();
    m_pBlendState.Release();
    m_pRasterState.Release();
    m_pRasterStateAntialiased.Release();
    m_pRasterStateMultisample.Release();
    m_pRasterStateCullCW.Release();
    m_pRasterStateCullCCW.Release();


    m_pLineRectVS.Release();
    m_pLineRectCstColorVS.Release();
    m_pLineRectPS.Release();
    m_pTextVS.Release();
    m_pTextCstColorVS.Release();
    m_pTextPS.Release();

    m_pLineRectVertexLayout.Release();
    m_pTextVertexLayout.Release();

    m_pLineVertexBuffer.Release();
    m_pRectVertexBuffer.Release();
    m_pTrianglesVertexBuffer.Release();
    m_TrianglesVertexBufferCount = 0;
    m_pConstantBuffer.Release();

    m_pResourceMapping.Release();
    m_pSamplerState.Release();

    m_pDevImmContext = nullptr;
    m_pDev.Release();

    return 1;
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::BeginDraw(int _WndWidth, int _WndHeight)
{
    assert(m_Drawing==false && _WndWidth>0 && _WndHeight>0);
    m_Drawing = true;

    m_WndWidth  = _WndWidth;
    m_WndHeight = _WndHeight;
    m_OffsetX = m_OffsetY = 0;

    // Setup the viewport
    m_ViewportInit.Width = (Float32)_WndWidth;
    m_ViewportInit.Height = (Float32)_WndHeight;
    m_ViewportInit.MinDepth = 0.0f;
    m_ViewportInit.MaxDepth = 1.0f;
    m_ViewportInit.TopLeftX = 0;
    m_ViewportInit.TopLeftY = 0;
    m_pDevImmContext->SetViewports(1, &m_ViewportInit, m_WndWidth, m_WndHeight);

    m_FullRect.left = 0;
    m_FullRect.right = m_WndWidth;
    m_FullRect.top = 0;
    m_FullRect.bottom = m_WndHeight;

    m_ViewportAndScissorRects[0] = m_FullRect;
    m_ViewportAndScissorRects[1] = m_FullRect;
    m_pDevImmContext->SetScissorRects(1, m_ViewportAndScissorRects, m_WndWidth, m_WndHeight);

    m_pDevImmContext->SetRasterizerState(m_pRasterState);

    m_pDevImmContext->SetDepthStencilState( m_pDepthStencilState );
    
    m_pDevImmContext->SetBlendState(m_pBlendState);
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::EndDraw()
{
    assert(m_Drawing==true);
    m_Drawing = false;
}

//  ---------------------------------------------------------------------------

bool CTwGraphImpl::IsDrawing()
{
    return m_Drawing;
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::Restore()
{
    UnbindFont(m_pDev, m_pFontGPUTex, m_psvFont);
    
    m_FontTex = NULL;
}

//  ---------------------------------------------------------------------------

static inline float ToNormScreenX(int x, int wndWidth)
{
    return 2.0f*((float)x-0.5f)/wndWidth - 1.0f;
}

static inline float ToNormScreenY(int y, int wndHeight)
{
    return 1.0f - 2.0f*((float)y-0.5f)/wndHeight;
}

static inline color32 ToR8G8B8A8(color32 col)
{
    return (col & 0xff00ff00) | ((col>>16) & 0xff) | ((col<<16) & 0xff0000);
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::DrawLine(int _X0, int _Y0, int _X1, int _Y1, color32 _Color0, color32 _Color1, bool _AntiAliased)
{
    assert(m_Drawing==true);

    float x0 = ToNormScreenX(_X0 + m_OffsetX, m_WndWidth);
    float y0 = ToNormScreenY(_Y0 + m_OffsetY, m_WndHeight);
    float x1 = ToNormScreenX(_X1 + m_OffsetX, m_WndWidth);
    float y1 = ToNormScreenY(_Y1 + m_OffsetY, m_WndHeight);
 
    MapHelper<CLineRectVtx> mappedVertices(m_pDevImmContext, m_pLineVertexBuffer, MAP_WRITE_DISCARD, 0);
    if( mappedVertices )
    {
        CLineRectVtx *vertices = mappedVertices;
        // Fill vertex buffer
        vertices[0].m_Pos[0] = x0;
        vertices[0].m_Pos[1] = y0;
        vertices[0].m_Pos[2] = 0;
        vertices[0].m_Color = ToR8G8B8A8(_Color0);
        vertices[1].m_Pos[0] = x1;
        vertices[1].m_Pos[1] = y1;
        vertices[1].m_Pos[2] = 0;
        vertices[1].m_Color = ToR8G8B8A8(_Color1);
        mappedVertices.Unmap();


        if( _AntiAliased )
            m_pDevImmContext->SetRasterizerState(m_pRasterStateAntialiased);

        // Reset shader constants
        MapHelper<CConstants> mappedConstBuffer(m_pDevImmContext, m_pConstantBuffer, MAP_WRITE_DISCARD, 0);
        if( mappedConstBuffer )
        {
            CConstants *constants = mappedConstBuffer;
            constants->m_Offset[0] = 0;
            constants->m_Offset[1] = 0;
            constants->m_Offset[2] = 0;
            constants->m_Offset[3] = 0;
            constants->m_CstColor[0] = 1;
            constants->m_CstColor[1] = 1;
            constants->m_CstColor[2] = 1;
            constants->m_CstColor[3] = 1;
            mappedConstBuffer.Unmap();
        }

        // Set the input layout
        m_pDevImmContext->SetVertexDescription(m_pLineRectVertexLayout);

        // Set vertex buffer
        Uint32 stride = sizeof(CLineRectVtx);
        Uint32 offset = 0;
        IBuffer *pBuffs[] = {m_pLineVertexBuffer};
        m_pDevImmContext->SetVertexBuffers(0, 1, pBuffs, &stride, &offset, SET_VERTEX_BUFFERS_FLAG_RESET);

        // Render the line
        IShader *ppShaders[] = { m_pLineRectVS, m_pLineRectPS };
        m_pDevImmContext->SetShaders(ppShaders, 2);
        m_pDevImmContext->BindShaderResources(m_pResourceMapping, BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED | BIND_SHADER_RESOURCES_ALL_RESOLVED);
        DrawAttribs DrawAttribs;
        DrawAttribs.Topology = PRIMITIVE_TOPOLOGY_LINE_LIST;
        DrawAttribs.NumVertices = 2;
        m_pDevImmContext->Draw(DrawAttribs);

        if( _AntiAliased )
            m_pDevImmContext->SetRasterizerState(m_pRasterState); // restore default raster state
    }
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::DrawRect(int _X0, int _Y0, int _X1, int _Y1, color32 _Color00, color32 _Color10, color32 _Color01, color32 _Color11)
{
    assert(m_Drawing==true);

    // border adjustment
    if(_X0<_X1)
        ++_X1;
    else if(_X0>_X1)
        ++_X0;
    if(_Y0<_Y1)
        ++_Y1;
    else if(_Y0>_Y1)
        ++_Y0;

    float x0 = ToNormScreenX(_X0 + m_OffsetX, m_WndWidth);
    float y0 = ToNormScreenY(_Y0 + m_OffsetY, m_WndHeight);
    float x1 = ToNormScreenX(_X1 + m_OffsetX, m_WndWidth);
    float y1 = ToNormScreenY(_Y1 + m_OffsetY, m_WndHeight);
 
    MapHelper<CLineRectVtx> mappedVertices( m_pDevImmContext, m_pRectVertexBuffer, MAP_WRITE_DISCARD, 0 );
    if( mappedVertices )
    {
        CLineRectVtx *vertices = mappedVertices;
        // Fill vertex buffer
        vertices[0].m_Pos[0] = x0;
        vertices[0].m_Pos[1] = y0;
        vertices[0].m_Pos[2] = 0;
        vertices[0].m_Color = ToR8G8B8A8(_Color00);
        vertices[1].m_Pos[0] = x1;
        vertices[1].m_Pos[1] = y0;
        vertices[1].m_Pos[2] = 0;
        vertices[1].m_Color = ToR8G8B8A8(_Color10);
        vertices[2].m_Pos[0] = x0;
        vertices[2].m_Pos[1] = y1;
        vertices[2].m_Pos[2] = 0;
        vertices[2].m_Color = ToR8G8B8A8(_Color01);
        vertices[3].m_Pos[0] = x1;
        vertices[3].m_Pos[1] = y1;
        vertices[3].m_Pos[2] = 0;
        vertices[3].m_Color = ToR8G8B8A8(_Color11);
        mappedVertices.Unmap();

        // Reset shader constants
        MapHelper<CConstants> mappedConstBuffer( m_pDevImmContext, m_pConstantBuffer, MAP_WRITE_DISCARD, 0 );
        if( mappedConstBuffer )
        {
            CConstants *constants = mappedConstBuffer;
            constants->m_Offset[0] = 0;
            constants->m_Offset[1] = 0;
            constants->m_Offset[2] = 0;
            constants->m_Offset[3] = 0;
            constants->m_CstColor[0] = 1;
            constants->m_CstColor[1] = 1;
            constants->m_CstColor[2] = 1;
            constants->m_CstColor[3] = 1;
            mappedConstBuffer.Unmap();
        }

        // Set the input layout
        m_pDevImmContext->SetVertexDescription(m_pLineRectVertexLayout);

        // Set vertex buffer
        Uint32 stride = sizeof(CLineRectVtx);
        Uint32 offset = 0;
        IBuffer *ppBuffers[] = {m_pRectVertexBuffer};
        m_pDevImmContext->SetVertexBuffers(0, 1, ppBuffers, &stride, &offset, SET_VERTEX_BUFFERS_FLAG_RESET);

        // Render the rect
        IShader *ppShaders[] = { m_pLineRectVS, m_pLineRectPS };
        m_pDevImmContext->SetShaders(ppShaders, 2);
        m_pDevImmContext->BindShaderResources(m_pResourceMapping, BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED | BIND_SHADER_RESOURCES_ALL_RESOLVED);

        DrawAttribs DrawAttribs;
        DrawAttribs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        DrawAttribs.NumVertices = 4;
        m_pDevImmContext->Draw(DrawAttribs);
    }
}

//  ---------------------------------------------------------------------------

void *CTwGraphImpl::NewTextObj()
{
    CTextObj *textObj = new CTextObj;
    memset(textObj, 0, sizeof(CTextObj));
    return textObj;
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::DeleteTextObj(void *_TextObj)
{
    assert(_TextObj!=NULL);
    CTextObj *textObj = static_cast<CTextObj *>(_TextObj);
    if( textObj->m_pTextVertexBuffer )
        textObj->m_pTextVertexBuffer.Release();
    if( textObj->m_pBgVertexBuffer )
        textObj->m_pBgVertexBuffer.Release();
    memset(textObj, 0, sizeof(CTextObj));
    delete textObj;
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::BuildText(void *_TextObj, const std::string *_TextLines, color32 *_LineColors, color32 *_LineBgColors, int _NbLines, const CTexFont *_Font, int _Sep, int _BgWidth)
{
    assert(m_Drawing==true);
    assert(_TextObj!=NULL);
    assert(_Font!=NULL);

    if( _Font != m_FontTex )
    {
        UnbindFont(m_pDev, m_pFontGPUTex, m_psvFont);
        BindFont(m_pDev, _Font, &m_pFontGPUTex);
        auto *pRV = m_pFontGPUTex->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE );
        pRV->SetSampler( m_pSamplerState );
        m_psvFont->Set(pRV);

        m_FontTex = _Font;
    }

    int nbTextVerts = 0;
    int line;
    for( line=0; line<_NbLines; ++line )
        nbTextVerts += 6 * (int)_TextLines[line].length();
    int nbBgVerts = 0;
    if( _BgWidth>0 )
        nbBgVerts = _NbLines*6;

    CTextObj *textObj = static_cast<CTextObj *>(_TextObj);
    textObj->m_LineColors = (_LineColors!=NULL);
    textObj->m_LineBgColors = (_LineBgColors!=NULL);

    // (re)create text vertex buffer if needed, and map it
    CTextVtx *textVerts = NULL;
    if( nbTextVerts>0 )
    {
        if( textObj->m_pTextVertexBuffer==nullptr || textObj->m_TextVertexBufferSize<nbTextVerts )
        {
            if( textObj->m_pTextVertexBuffer!=nullptr )
            {
                textObj->m_pTextVertexBuffer.Release();
            }
            textObj->m_TextVertexBufferSize = nbTextVerts + 6*256; // add a reserve of 256 characters
            BufferDesc BuffDesc;
            BuffDesc.Usage = USAGE_DYNAMIC;
            BuffDesc.uiSizeInBytes = textObj->m_TextVertexBufferSize * sizeof(CTextVtx);
            BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDev->CreateBuffer(BuffDesc, BufferData(), &textObj->m_pTextVertexBuffer);
        }

        if( textObj->m_pTextVertexBuffer!=nullptr )
        {
            PVoid pData;
            textObj->m_pTextVertexBuffer->Map( m_pDevImmContext, MAP_WRITE_DISCARD, 0, pData );
            textVerts = (CTextVtx *)pData;
        }
    }

    // (re)create bg vertex buffer if needed, and map it
    CLineRectVtx *bgVerts = NULL;
    if( nbBgVerts>0 )
    {
        if( textObj->m_pBgVertexBuffer==nullptr || textObj->m_BgVertexBufferSize<nbBgVerts )
        {
            if( textObj->m_pBgVertexBuffer!=nullptr )
            {
                textObj->m_pBgVertexBuffer.Release();
            }
            textObj->m_BgVertexBufferSize = nbBgVerts + 6*32; // add a reserve of 32 rects
            BufferDesc BuffDesc;
            BuffDesc.Usage = USAGE_DYNAMIC;
            BuffDesc.uiSizeInBytes = textObj->m_BgVertexBufferSize * sizeof(CLineRectVtx);
            BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDev->CreateBuffer(BuffDesc, BufferData(), &textObj->m_pBgVertexBuffer);
        }

        if( textObj->m_pBgVertexBuffer!=nullptr )
        {
            PVoid pData;
            textObj->m_pBgVertexBuffer->Map( m_pDevImmContext, MAP_WRITE_DISCARD, 0, pData );
            bgVerts = (CLineRectVtx *)pData;
        }
    }

    int x, x1, y, y1, i, len;
    float px, px1, py, py1;
    unsigned char ch;
    const unsigned char *text;
    color32 lineColor = COLOR32_RED;
    CTextVtx vtx;
    vtx.m_Pos[2] = 0;
    CLineRectVtx bgVtx;
    bgVtx.m_Pos[2] = 0;
    int textVtxIndex = 0;
    int bgVtxIndex = 0;
    for( line=0; line<_NbLines; ++line )
    {
        x = 0;
        y = line * (_Font->m_CharHeight+_Sep);
        y1 = y+_Font->m_CharHeight;
        len = (int)_TextLines[line].length();
        text = (const unsigned char *)(_TextLines[line].c_str());
        if( _LineColors!=NULL )
            lineColor = ToR8G8B8A8(_LineColors[line]);

        if( textVerts!=NULL )
            for( i=0; i<len; ++i )
            {
                ch = text[i];
                x1 = x + _Font->m_CharWidth[ch];

                px  = ToNormScreenX(x,  m_WndWidth);
                py  = ToNormScreenY(y,  m_WndHeight);
                px1 = ToNormScreenX(x1, m_WndWidth);
                py1 = ToNormScreenY(y1, m_WndHeight);

                vtx.m_Color  = lineColor;

                vtx.m_Pos[0] = px;
                vtx.m_Pos[1] = py;
                vtx.m_UV [0] = _Font->m_CharU0[ch];
                vtx.m_UV [1] = _Font->m_CharV0[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px1;
                vtx.m_Pos[1] = py;
                vtx.m_UV [0] = _Font->m_CharU1[ch];
                vtx.m_UV [1] = _Font->m_CharV0[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px;
                vtx.m_Pos[1] = py1;
                vtx.m_UV [0] = _Font->m_CharU0[ch];
                vtx.m_UV [1] = _Font->m_CharV1[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px1;
                vtx.m_Pos[1] = py;
                vtx.m_UV [0] = _Font->m_CharU1[ch];
                vtx.m_UV [1] = _Font->m_CharV0[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px1;
                vtx.m_Pos[1] = py1;
                vtx.m_UV [0] = _Font->m_CharU1[ch];
                vtx.m_UV [1] = _Font->m_CharV1[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px;
                vtx.m_Pos[1] = py1;
                vtx.m_UV [0] = _Font->m_CharU0[ch];
                vtx.m_UV [1] = _Font->m_CharV1[ch];
                textVerts[textVtxIndex++] = vtx;

                x = x1;
            }

        if( _BgWidth>0 && bgVerts!=NULL )
        {
            if( _LineBgColors!=NULL )
                bgVtx.m_Color = ToR8G8B8A8(_LineBgColors[line]);
            else
                bgVtx.m_Color = ToR8G8B8A8(COLOR32_BLACK);

            px  = ToNormScreenX(-1, m_WndWidth);
            py  = ToNormScreenY(y,  m_WndHeight);
            px1 = ToNormScreenX(_BgWidth+1, m_WndWidth);
            py1 = ToNormScreenY(y1, m_WndHeight);

            bgVtx.m_Pos[0] = px;
            bgVtx.m_Pos[1] = py;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px1;
            bgVtx.m_Pos[1] = py;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px;
            bgVtx.m_Pos[1] = py1;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px1;
            bgVtx.m_Pos[1] = py;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px1;
            bgVtx.m_Pos[1] = py1;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px;
            bgVtx.m_Pos[1] = py1;
            bgVerts[bgVtxIndex++] = bgVtx;
        }
    }
    assert( textVtxIndex==nbTextVerts );
    assert( bgVtxIndex==nbBgVerts );
    textObj->m_NbTextVerts = nbTextVerts;
    textObj->m_NbBgVerts = nbBgVerts;

    if( textVerts!=NULL )
        textObj->m_pTextVertexBuffer->Unmap(m_pDevImmContext);
    if( bgVerts!=NULL )
        textObj->m_pBgVertexBuffer->Unmap(m_pDevImmContext);
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::DrawText(void *TextObj, int X, int Y, color32 Color, color32 BgColor)
{
    assert(m_Drawing==true);
    assert(TextObj!=NULL);
    CTextObj *textObj = static_cast<CTextObj *>(TextObj);
    float dx = 2.0f*(float)(X + m_OffsetX)/m_WndWidth;
    float dy = -2.0f*(float)(Y + m_OffsetY)/m_WndHeight;

    // Draw background
    if( textObj->m_NbBgVerts>=4 && textObj->m_pBgVertexBuffer!=nullptr )
    {
        // Set offset and constant color
        MapHelper<CConstants> mappedConstBuffer(m_pDevImmContext, m_pConstantBuffer, MAP_WRITE_DISCARD, 0);
        if( mappedConstBuffer )
        {
            CConstants *constants = mappedConstBuffer;
            constants->m_Offset[0] = dx;
            constants->m_Offset[1] = dy;
            constants->m_Offset[2] = 0;
            constants->m_Offset[3] = 0;
            Color32ToARGBf(BgColor, constants->m_CstColor+3, constants->m_CstColor+0, constants->m_CstColor+1, constants->m_CstColor+2);
            mappedConstBuffer.Unmap();
        }

        // Set the input layout
        m_pDevImmContext->SetVertexDescription(m_pLineRectVertexLayout);

        // Set vertex buffer
        Uint32 stride = sizeof(CLineRectVtx);
        Uint32 offset = 0;
        IBuffer *ppBuffers[] = {textObj->m_pBgVertexBuffer};
        m_pDevImmContext->SetVertexBuffers(0, 1, ppBuffers, &stride, &offset, SET_VERTEX_BUFFERS_FLAG_RESET);

        // Render the bg rectangles
        IShader *ppShaders[] = {nullptr, m_pLineRectPS};
        if( BgColor!=0 || !textObj->m_LineBgColors ) // use a constant bg color
            ppShaders[0] = m_pLineRectCstColorVS;
        else
            ppShaders[0] = m_pLineRectVS;
        m_pDevImmContext->SetShaders(ppShaders, 2);
        m_pDevImmContext->BindShaderResources(m_pResourceMapping, BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED | BIND_SHADER_RESOURCES_ALL_RESOLVED);

        DrawAttribs DrawAttribs;
        DrawAttribs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        DrawAttribs.NumVertices = textObj->m_NbBgVerts;
        m_pDevImmContext->Draw(DrawAttribs);
    }

    // Draw text
    if( textObj->m_NbTextVerts>=4 && textObj->m_pTextVertexBuffer!=nullptr )
    {
        // Set offset and constant color
        MapHelper<CConstants> mappedConstBuffer(m_pDevImmContext, m_pConstantBuffer, MAP_WRITE_DISCARD, 0);
        if( mappedConstBuffer )
        {
            CConstants *constants = mappedConstBuffer;
            constants->m_Offset[0] = dx;
            constants->m_Offset[1] = dy;
            constants->m_Offset[2] = 0;
            constants->m_Offset[3] = 0;
            Color32ToARGBf(Color, constants->m_CstColor+3, constants->m_CstColor+0, constants->m_CstColor+1, constants->m_CstColor+2);
            mappedConstBuffer.Unmap();
        }

        // Set the input layout
        m_pDevImmContext->SetVertexDescription(m_pTextVertexLayout);

        // Set vertex buffer
        Uint32 stride = sizeof(CTextVtx);
        Uint32 offset = 0;
        IBuffer *ppBuffers[] = { textObj->m_pTextVertexBuffer };
        m_pDevImmContext->SetVertexBuffers(0, 1, ppBuffers, &stride, &offset, SET_VERTEX_BUFFERS_FLAG_RESET);

        // Render the text
        IShader *ppShaders[] = { nullptr, m_pTextPS};
        if( Color!=0 || !textObj->m_LineColors ) // use a constant color
            ppShaders[0] = m_pTextCstColorVS;
        else
            ppShaders[0] = m_pTextVS;
        m_pDevImmContext->SetShaders(ppShaders, 2);
        m_pDevImmContext->BindShaderResources(m_pResourceMapping, BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED | BIND_SHADER_RESOURCES_ALL_RESOLVED);
        
        DrawAttribs DrawAttribs;
        DrawAttribs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        DrawAttribs.NumVertices = textObj->m_NbTextVerts;
        m_pDevImmContext->Draw(DrawAttribs);
    }
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::ChangeViewport(int _X0, int _Y0, int _Width, int _Height, int _OffsetX, int _OffsetY)
{
    if( _Width>0 && _Height>0 )
    {
	    /* viewport changes screen coordinates, use scissor instead
        D3D11_VIEWPORT vp;
        vp.TopLeftX = _X0;
        vp.TopLeftY = _Y0;
        vp.Width = _Width;
        vp.Height = _Height;
        vp.MinDepth = 0;
        vp.MaxDepth = 1;
        m_D3DDev->RSSetViewports(1, &vp);
        */
            
        m_ViewportAndScissorRects[0].left = _X0;
        m_ViewportAndScissorRects[0].right = _X0 + _Width - 1;
        m_ViewportAndScissorRects[0].top = _Y0;
        m_ViewportAndScissorRects[0].bottom = _Y0 + _Height - 1;
        if( m_ViewportAndScissorRects[1] == m_FullRect )
            m_pDevImmContext->SetScissorRects(1, m_ViewportAndScissorRects, m_WndWidth, m_WndHeight); // viewport clipping only
        else
            m_pDevImmContext->SetScissorRects(2, m_ViewportAndScissorRects, m_WndWidth, m_WndHeight);

        m_OffsetX = _X0 + _OffsetX;
        m_OffsetY = _Y0 + _OffsetY;
    }
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::RestoreViewport()
{
    m_pDevImmContext->SetViewports(1, &m_ViewportInit, m_WndWidth, m_WndHeight);
    m_ViewportAndScissorRects[0] = m_FullRect;
    m_pDevImmContext->SetScissorRects(1, m_ViewportAndScissorRects+1, m_WndWidth, m_WndHeight); // scissor only
        
    m_OffsetX = m_OffsetY = 0;
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::SetScissor(int _X0, int _Y0, int _Width, int _Height)
{
    if( _Width>0 && _Height>0 )
    {
        m_ViewportAndScissorRects[1].left = _X0 - 2;
        m_ViewportAndScissorRects[1].right = _X0 + _Width - 3;
        m_ViewportAndScissorRects[1].top = _Y0 - 1;
        m_ViewportAndScissorRects[1].bottom = _Y0 + _Height - 1;

        if( m_ViewportAndScissorRects[0] == m_FullRect )
            m_pDevImmContext->SetScissorRects(1, m_ViewportAndScissorRects+1, m_WndWidth, m_WndHeight); // no viewport clipping
        else
            m_pDevImmContext->SetScissorRects(2, m_ViewportAndScissorRects, m_WndWidth, m_WndHeight);
    }
    else
    {
        m_ViewportAndScissorRects[1] = m_FullRect;
        m_pDevImmContext->SetScissorRects(1, m_ViewportAndScissorRects, m_WndWidth, m_WndHeight); // apply viewport clipping only
    }
}

//  ---------------------------------------------------------------------------

void CTwGraphImpl::DrawTriangles(int _NumTriangles, int *_Vertices, color32 *_Colors, Cull _CullMode)
{
    assert(m_Drawing==true);

    if( _NumTriangles<=0 )
        return;

    if( m_TrianglesVertexBufferCount<3*_NumTriangles ) // force re-creation
    {
        m_pTrianglesVertexBuffer.Release();
        m_TrianglesVertexBufferCount = 0;
    }

    // DrawTriangles uses LineRect layout and shaders

    if( !m_pTrianglesVertexBuffer )
    {
        // Create triangles vertex buffer
        BufferDesc BuffDesc;
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        BuffDesc.uiSizeInBytes = 3*_NumTriangles * sizeof(CLineRectVtx);
        try
        {
            m_pDev->CreateBuffer( BuffDesc, BufferData(), &m_pTrianglesVertexBuffer );
            m_TrianglesVertexBufferCount = 3 * _NumTriangles;
        }
        catch(const std::runtime_error & )
        {
            m_TrianglesVertexBufferCount = 0;
            return; // Problem: cannot create triangles VB
        }
    }
    assert( m_TrianglesVertexBufferCount>=3*_NumTriangles );
    assert( m_pTrianglesVertexBuffer );

    MapHelper<CLineRectVtx> mappedVertices( m_pDevImmContext, m_pTrianglesVertexBuffer, MAP_WRITE_DISCARD, 0);
    if( mappedVertices )
    {
        CLineRectVtx *vertices = mappedVertices;
        // Fill vertex buffer
        for( int i=0; i<3*_NumTriangles; ++ i )
        {
            vertices[i].m_Pos[0] = ToNormScreenX(_Vertices[2*i+0] + m_OffsetX, m_WndWidth);
            vertices[i].m_Pos[1] = ToNormScreenY(_Vertices[2*i+1] + m_OffsetY, m_WndHeight);
            vertices[i].m_Pos[2] = 0;
            vertices[i].m_Color = ToR8G8B8A8(_Colors[i]);
        }
        mappedVertices.Unmap();

        // Reset shader constants
        MapHelper<CConstants> mappedConstBuffer(m_pDevImmContext, m_pConstantBuffer, MAP_WRITE_DISCARD, 0);
        if( mappedConstBuffer )
        {
            CConstants *constants = mappedConstBuffer;
            constants->m_Offset[0] = 0;
            constants->m_Offset[1] = 0;
            constants->m_Offset[2] = 0;
            constants->m_Offset[3] = 0;
            constants->m_CstColor[0] = 1;
            constants->m_CstColor[1] = 1;
            constants->m_CstColor[2] = 1;
            constants->m_CstColor[3] = 1;
            mappedConstBuffer.Unmap();
        }

        // Set the input layout
        m_pDevImmContext->SetVertexDescription(m_pLineRectVertexLayout);

        // Set vertex buffer
        Uint32 stride = sizeof(CLineRectVtx);
        Uint32 offset = 0;
        IBuffer *ppBuffers[] = {m_pTrianglesVertexBuffer};
        m_pDevImmContext->SetVertexBuffers(0, 1, ppBuffers, &stride, &offset, SET_VERTEX_BUFFERS_FLAG_RESET);

        if( _CullMode==CULL_CW )
            m_pDevImmContext->SetRasterizerState(m_pRasterStateCullCW);
        else if( _CullMode==CULL_CCW )
            m_pDevImmContext->SetRasterizerState(m_pRasterStateCullCCW);
        else 
            m_pDevImmContext->SetRasterizerState(m_pRasterStateMultisample);

        // Render the triangles
        IShader *ppShaders[] = {m_pLineRectVS, m_pLineRectPS};
        m_pDevImmContext->SetShaders(ppShaders, 2);
        m_pDevImmContext->BindShaderResources( m_pResourceMapping, BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED | BIND_SHADER_RESOURCES_ALL_RESOLVED);
        
        DrawAttribs DrawAttribs;
        DrawAttribs.NumVertices = 3*_NumTriangles;
        DrawAttribs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        m_pDevImmContext->Draw(DrawAttribs);

        m_pDevImmContext->SetRasterizerState(m_pRasterState); // restore default raster state
    }
}

//  ---------------------------------------------------------------------------
