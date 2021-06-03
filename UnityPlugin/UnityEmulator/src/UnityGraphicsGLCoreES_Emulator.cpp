#include "UnityGraphicsGL_Impl.h"

#include "UnityGraphicsGLCoreES_Emulator.h"

#if GL_SUPPORTED || GLES_SUPPORTED

#include "DebugUtilities.hpp"
#include "Errors.hpp"

std::unique_ptr<UnityGraphicsGL_Impl> UnityGraphicsGLCoreES_Emulator::m_GraphicsImpl;

UnityGraphicsGLCoreES_Emulator::UnityGraphicsGLCoreES_Emulator()
{
    //GeUnityInterfaces().RegisterInterface(IUnityGraphicsGLCoreES__GUID, GetUnityGraphicsAPIInterface());
}

void UnityGraphicsGLCoreES_Emulator::InitGLContext(void *pNativeWndHandle, 
                                                   #if PLATFORM_LINUX
                                                       void *pDisplay,
                                                   #endif
                                                   int MajorVersion, int MinorVersion)
{
    VERIFY(!m_GraphicsImpl, "Another emulator has already been initialized");
    m_GraphicsImpl.reset( new UnityGraphicsGL_Impl );
    m_GraphicsImpl->InitGLContext(pNativeWndHandle,
                                  #if PLATFORM_LINUX
                                     pDisplay,
                                  #endif
                                  MajorVersion, MinorVersion);
}

UnityGraphicsGLCoreES_Emulator& UnityGraphicsGLCoreES_Emulator::GetInstance()
{
    static UnityGraphicsGLCoreES_Emulator TheInstance;
    return TheInstance;
}

void UnityGraphicsGLCoreES_Emulator::Present()
{
    m_GraphicsImpl->SwapBuffers();
}

void UnityGraphicsGLCoreES_Emulator::Release()
{
    m_GraphicsImpl.reset();
}

void UnityGraphicsGLCoreES_Emulator::ResizeSwapChain(unsigned int Width, unsigned int Height)
{
#if PLATFORM_ANDROID
    m_GraphicsImpl->UpdateScreenSize();
    GetBackBufferSize(Width, Height);
#endif
    m_GraphicsImpl->ResizeSwapchain(Width, Height);
}

void UnityGraphicsGLCoreES_Emulator::GetBackBufferSize(unsigned int &Width, unsigned int &Height)
{
    Width = m_GraphicsImpl->GetBackBufferWidth();
    Height = m_GraphicsImpl->GetBackBufferHeight();
}

bool UnityGraphicsGLCoreES_Emulator::SwapChainInitialized()
{
    return m_GraphicsImpl->GetContext() != NULL;
}


UnityGraphicsGL_Impl* UnityGraphicsGLCoreES_Emulator::GetGraphicsImpl()
{
    return m_GraphicsImpl.get();
}

IUnityInterface* UnityGraphicsGLCoreES_Emulator::GetUnityGraphicsAPIInterface()
{
    return nullptr;
}

UnityGfxRenderer UnityGraphicsGLCoreES_Emulator::GetUnityGfxRenderer()
{
    return kUnityGfxRendererOpenGLCore;
}

void UnityGraphicsGLCoreES_Emulator::BeginFrame()
{
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_GraphicsImpl->GetDefaultFBO() );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, m_GraphicsImpl->GetDefaultFBO() );
    glClearDepthf( UsesReverseZ() ? 0.f : 1.f );
    static const float ClearColor[4] = { 0, 0, 0.5f, 1 };
    glClearColor(ClearColor[0], ClearColor[1], ClearColor[2], ClearColor[3]);
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable( GL_SCISSOR_TEST );
    glViewport(0, 0, m_GraphicsImpl->GetBackBufferWidth(), m_GraphicsImpl->GetBackBufferHeight() );
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glDepthRangef( 0, 1 );
    auto err = glGetError();
    if( err != GL_NO_ERROR)
        LOG_ERROR_MESSAGE("GL Error: ", err);
}

void UnityGraphicsGLCoreES_Emulator::EndFrame()
{
}

#endif // GL_SUPPORTED || GLES_SUPPORTED
