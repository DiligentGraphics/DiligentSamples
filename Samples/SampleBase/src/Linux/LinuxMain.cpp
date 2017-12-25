/*     Copyright 2015-2017 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include <memory>
#include <iomanip>

#include <GL/glx.h>
#include <GL/gl.h>

#define TW_STATIC
#include "AntTweakBar.h"
 
 // Undef symbols defined by XLib
#ifdef Bool
# undef Bool
#endif
#ifdef True
#   undef True
#endif
#ifdef False
#   undef False
#endif

#include "SampleBase.h"
#include "RenderDeviceFactoryOpenGL.h"
#include "Timer.h"

#include "Errors.h"

using namespace Diligent;
std::unique_ptr<SampleBase> g_pSample;

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
 
 // Create Direct3D device and swap chain
void InitDevice(Window Wnd, Display *pDisplay, IRenderDevice **ppRenderDevice, IDeviceContext **ppImmediateContext, ISwapChain **ppSwapChain)
{
    SwapChainDesc SCDesc;
    SCDesc.SamplesCount = 1;

    EngineCreationAttribs EngineCreationAttribs;
    GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(
        EngineCreationAttribs, ppRenderDevice, ppImmediateContext, SCDesc, reinterpret_cast<void*>(static_cast<size_t>(Wnd)), pDisplay, ppSwapChain );
}

int main (int argc, char ** argv)
{
    Display *display = XOpenDisplay(0);
 
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
 
    //const char *extensions = glXQueryExtensionsString(display, DefaultScreen(display));
    //std::cout << extensions << std::endl;
 
    static int visual_attribs[] =
    {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,   true,

        // The largest available total RGBA color buffer size (sum of GLX_RED_SIZE, 
        // GLX_GREEN_SIZE, GLX_BLUE_SIZE, and GLX_ALPHA_SIZE) of at least the minimum
        // size specified for each color component is preferred.
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_ALPHA_SIZE,     8,

        // The largest available depth buffer of at least GLX_DEPTH_SIZE size is preferred
        GLX_DEPTH_SIZE,     24,

        //GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 1,
        None
     };
 
    int fbcount = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
    if (!fbc)
    {
        LOG_ERROR_MESSAGE("Failed to retrieve a framebuffer config");
        return 1;
    }
 
    XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[0]);
 
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = 
        StructureNotifyMask |  
        ExposureMask |  
        KeyPressMask | 
        KeyReleaseMask |
        ButtonPressMask | 
        ButtonReleaseMask | 
        PointerMotionMask;
 
    Window win = XCreateWindow(display, RootWindow(display, vi->screen), 0, 0, 1024, 768, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel|CWColormap|CWEventMask, &swa);
    if (!win)
    {
        LOG_ERROR_MESSAGE("Failed to create window.");
        return 1;
    }
 
    XMapWindow(display, win);
 
    // Create an oldstyle context first, to get the correct function pointer for glXCreateContextAttribsARB
    GLXContext ctx_old = glXCreateContext(display, vi, 0, GL_TRUE);
    glXCreateContextAttribsARB =  (glXCreateContextAttribsARBProc)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");
    glXMakeCurrent(display, 0, 0);
    glXDestroyContext(display, ctx_old);
 
    if (glXCreateContextAttribsARB == NULL)
    {
        LOG_ERROR("glXCreateContextAttribsARB entry point not found. Aborting.");
        return false;
    }
 
    static int context_attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        None
    };
 
    GLXContext ctx = glXCreateContextAttribsARB(display, fbc[0], NULL, true, context_attribs);
    if (!ctx)
    {
        LOG_ERROR("Failed to create GL context.");
        return 1;
    }
    XFree(fbc);
    
    glXMakeCurrent(display, win, ctx);
 
    RefCntAutoPtr<IRenderDevice> pRenderDevice;
    RefCntAutoPtr<IDeviceContext> pDeviceContext;
    Diligent::RefCntAutoPtr<ISwapChain> pSwapChain;
    InitDevice( win, display, &pRenderDevice, &pDeviceContext, &pSwapChain );
    //g_pSwapChain = pSwapChain;

    // Initialize AntTweakBar
    // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with 
    // very old GL specification. Using these modes results in applying some 
    // odd offsets which distorts everything
    // Latest OpenGL works very much like Direct3D11, and 
    // Tweak Bar will never know if D3D or OpenGL is actually used
    if (!TwInit(TW_DIRECT3D11, pRenderDevice.RawPtr(), pDeviceContext.RawPtr(), pSwapChain->GetDesc().ColorBufferFormat))
    {
        LOG_ERROR_MESSAGE("AntTweakBar initialization failed");
        return 1;
    }

    g_pSample.reset( CreateSample(pRenderDevice, pDeviceContext, pSwapChain) );
    g_pSample->WindowResize( pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height );
    std::string Title = g_pSample->GetSampleName(); 
 
    Timer Timer;
    auto PrevTime = Timer.GetElapsedTime();
    double filteredFrameTime = 0.0;
 

    while (true) 
    {
        bool exit = false;
        XEvent xev;
        // Handle all events in the queue
        while(XCheckMaskEvent(display, 0xFFFFFFFF, &xev))
        {
            TwEventX11(&xev);
            switch(xev.type)
            {
                case KeyPress:
                {
                    KeySym keysym;
                    char buffer[80];
                    int num_char = XLookupString((XKeyEvent *)&xev, buffer, _countof(buffer), &keysym, 0);
                    exit = (keysym==XK_Escape);
                }
                
                case ConfigureNotify:
                {
                    XConfigureEvent &xce = reinterpret_cast<XConfigureEvent &>(xev);
                    pSwapChain->Resize(static_cast<Uint32>(xce.width), static_cast<Uint32>(xce.height));
                    g_pSample->WindowResize( pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height );
                    break;
                }
            }
        }

        if(exit)
            break;

        // Render the scene
        auto CurrTime = Timer.GetElapsedTime();
        auto ElapsedTime = CurrTime - PrevTime;
        PrevTime = CurrTime;

        pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
        
        g_pSample->Update(CurrTime, ElapsedTime);
        g_pSample->Render();

        // Draw tweak bars
        // Restore default render target in case the sample has changed it
        pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
        TwDraw();
        
        glXSwapBuffers(display, win);
        //pSwapChain->Present();

        double filterScale = 0.2;
        filteredFrameTime = filteredFrameTime * (1.0 - filterScale) + filterScale * ElapsedTime;
        std::stringstream fpsCounterSS;
        fpsCounterSS << " - " << std::fixed << std::setprecision(1) << filteredFrameTime * 1000;
        fpsCounterSS << " ms (" << 1.0 / filteredFrameTime << " fps)";
        XStoreName(display, win, (Title + fpsCounterSS.str()).c_str());
    }
 
    g_pSample.reset();
    TwTerminate();
    pSwapChain.Release();
    pDeviceContext.Release();
    pRenderDevice.Release();
 
    ctx = glXGetCurrentContext();
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, win);
    XCloseDisplay(display);
}





#if 0
#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

// Helper to check for extension string presence.  Adapted from:
//   http://www.opengl.org/resources/features/OGLextensions/
static bool isExtensionSupported(const char *extList, const char *extension)
{
  const char *start;
  const char *where, *terminator;
  
  /* Extension names should not have spaces. */
  where = strchr(extension, ' ');
  if (where || *extension == '\0')
    return false;

  /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
  for (start=extList;;) {
    where = strstr(start, extension);

    if (!where)
      break;

    terminator = where + strlen(extension);

    if ( where == start || *(where - 1) == ' ' )
      if ( *terminator == ' ' || *terminator == '\0' )
        return true;

    start = terminator;
  }

  return false;
}

static bool ctxErrorOccurred = false;
static int ctxErrorHandler( Display *dpy, XErrorEvent *ev )
{
    ctxErrorOccurred = true;
    return 0;
}

int main(int argc, char* argv[])
{
  Display *display = XOpenDisplay(NULL);

  if (!display)
  {
    printf("Failed to open X display\n");
    exit(1);
  }

  // Get a matching FB config
  static int visual_attribs[] =
    {
      GLX_X_RENDERABLE    , True,
      GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
      GLX_RENDER_TYPE     , GLX_RGBA_BIT,
      GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
      GLX_RED_SIZE        , 8,
      GLX_GREEN_SIZE      , 8,
      GLX_BLUE_SIZE       , 8,
      GLX_ALPHA_SIZE      , 8,
      GLX_DEPTH_SIZE      , 24,
      GLX_STENCIL_SIZE    , 8,
      GLX_DOUBLEBUFFER    , True,
      //GLX_SAMPLE_BUFFERS  , 1,
      //GLX_SAMPLES         , 4,
      None
    };

  int glx_major, glx_minor;
 
  // FBConfigs were added in GLX version 1.3.
  if ( !glXQueryVersion( display, &glx_major, &glx_minor ) || 
       ( ( glx_major == 1 ) && ( glx_minor < 3 ) ) || ( glx_major < 1 ) )
  {
    printf("Invalid GLX version");
    exit(1);
  }

  printf( "Getting matching framebuffer configs\n" );
  int fbcount;
  GLXFBConfig* fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
  if (!fbc)
  {
    printf( "Failed to retrieve a framebuffer config\n" );
    exit(1);
  }
  printf( "Found %d matching FB configs.\n", fbcount );

  // Pick the FB config/visual with the most samples per pixel
  printf( "Getting XVisualInfos\n" );
  int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

  int i;
  for (i=0; i<fbcount; ++i)
  {
    XVisualInfo *vi = glXGetVisualFromFBConfig( display, fbc[i] );
    if ( vi )
    {
      int samp_buf, samples;
      glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
      glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLES       , &samples  );
      
      printf( "  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d,"
              " SAMPLES = %d\n", 
              i, vi -> visualid, samp_buf, samples );

      if ( best_fbc < 0 || samp_buf && samples > best_num_samp )
        best_fbc = i, best_num_samp = samples;
      if ( worst_fbc < 0 || !samp_buf || samples < worst_num_samp )
        worst_fbc = i, worst_num_samp = samples;
    }
    XFree( vi );
  }

  GLXFBConfig bestFbc = fbc[ best_fbc ];

  // Be sure to free the FBConfig list allocated by glXChooseFBConfig()
  XFree( fbc );

  // Get a visual
  XVisualInfo *vi = glXGetVisualFromFBConfig( display, bestFbc );
  printf( "Chosen visual ID = 0x%x\n", vi->visualid );

  printf( "Creating colormap\n" );
  XSetWindowAttributes swa;
  Colormap cmap;
  swa.colormap = cmap = XCreateColormap( display,
                                         RootWindow( display, vi->screen ), 
                                         vi->visual, AllocNone );
  swa.background_pixmap = None ;
  swa.border_pixel      = 0;
  swa.event_mask        = StructureNotifyMask;

  printf( "Creating window\n" );
  Window win = XCreateWindow( display, RootWindow( display, vi->screen ), 
                              0, 0, 1024, 768, 0, vi->depth, InputOutput, 
                              vi->visual, 
                              CWBorderPixel|CWColormap|CWEventMask, &swa );
  if ( !win )
  {
    printf( "Failed to create window.\n" );
    exit(1);
  }

  // Done with the visual info data
  XFree( vi );

  XStoreName( display, win, "GL 3.0 Window" );

  printf( "Mapping window\n" );
  XMapWindow( display, win );

  // Get the default screen's GLX extension list
  const char *glxExts = glXQueryExtensionsString( display,
                                                  DefaultScreen( display ) );

  // NOTE: It is not necessary to create or make current to a context before
  // calling glXGetProcAddressARB
  glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
  glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
           glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

  GLXContext ctx = 0;

  // Install an X error handler so the application won't exit if GL 3.0
  // context allocation fails.
  //
  // Note this error handler is global.  All display connections in all threads
  // of a process use the same error handler, so be sure to guard against other
  // threads issuing X commands while this code is running.
  ctxErrorOccurred = false;
  int (*oldHandler)(Display*, XErrorEvent*) =
      XSetErrorHandler(&ctxErrorHandler);

  // Check for the GLX_ARB_create_context extension string and the function.
  // If either is not present, use GLX 1.3 context creation method.
  if ( !isExtensionSupported( glxExts, "GLX_ARB_create_context" ) ||
       !glXCreateContextAttribsARB )
  {
    printf( "glXCreateContextAttribsARB() not found"
            " ... using old-style GLX context\n" );
    ctx = glXCreateNewContext( display, bestFbc, GLX_RGBA_TYPE, 0, True );
  }

  // If it does, try to get a GL 3.0 context!
  else
  {
    int context_attribs[] =
      {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
      };

    printf( "Creating context\n" );
    ctx = glXCreateContextAttribsARB( display, bestFbc, 0,
                                      True, context_attribs );

    // Sync to ensure any errors generated are processed.
    XSync( display, False );
    if ( !ctxErrorOccurred && ctx )
      printf( "Created GL 3.0 context\n" );
    else
    {
      // Couldn't create GL 3.0 context.  Fall back to old-style 2.x context.
      // When a context version below 3.0 is requested, implementations will
      // return the newest context version compatible with OpenGL versions less
      // than version 3.0.
      // GLX_CONTEXT_MAJOR_VERSION_ARB = 1
      context_attribs[1] = 1;
      // GLX_CONTEXT_MINOR_VERSION_ARB = 0
      context_attribs[3] = 0;

      ctxErrorOccurred = false;

      printf( "Failed to create GL 3.0 context"
              " ... using old-style GLX context\n" );
      ctx = glXCreateContextAttribsARB( display, bestFbc, 0, 
                                        True, context_attribs );
    }
  }

  // Sync to ensure any errors generated are processed.
  XSync( display, False );

  // Restore the original error handler
  XSetErrorHandler( oldHandler );

  if ( ctxErrorOccurred || !ctx )
  {
    printf( "Failed to create an OpenGL context\n" );
    exit(1);
  }

  // Verifying that context is a direct context
  if ( ! glXIsDirect ( display, ctx ) )
  {
    printf( "Indirect GLX rendering context obtained\n" );
  }
  else
  {
    printf( "Direct GLX rendering context obtained\n" );
  }

  printf( "Making context current\n" );
  glXMakeCurrent( display, win, ctx );

  glClearColor( 0, 0.5, 1, 1 );
  glClear( GL_COLOR_BUFFER_BIT );
  glXSwapBuffers ( display, win );

  sleep( 1 );

  glClearColor ( 1, 0.5, 0, 1 );
  glClear ( GL_COLOR_BUFFER_BIT );
  glXSwapBuffers ( display, win );

  sleep( 1 );

  glXMakeCurrent( display, 0, 0 );
  glXDestroyContext( display, ctx );

  XDestroyWindow( display, win );
  XFreeColormap( display, cmap );
  XCloseDisplay( display );

  return 0;
}
#endif 
#if 1
#include <memory>
#include <iomanip>

//#include "GL/glxew.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

// Undef symbols defined by XLib
#ifdef Bool
# undef Bool
#endif
#ifdef True
#   undef True
#endif
#ifdef False
#   undef False
#endif

#include "SampleBase.h"
#include "RenderDeviceFactoryOpenGL.h"
#include "Timer.h"

#define TW_STATIC
#include "AntTweakBar.h"
#include "Errors.h"

using namespace Diligent;
std::unique_ptr<SampleBase> g_pSample;

Colormap                cmap;
XSetWindowAttributes    swa;
GLXContext              glc;
XWindowAttributes       gwa;
XEvent                  xev;

// Create Direct3D device and swap chain
void InitDevice(Window Wnd, Display *pDisplay, IRenderDevice **ppRenderDevice, IDeviceContext **ppImmediateContext, ISwapChain **ppSwapChain)
{
    SwapChainDesc SCDesc;
    SCDesc.SamplesCount = 1;

    EngineCreationAttribs EngineCreationAttribs;
    GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(
        EngineCreationAttribs, ppRenderDevice, ppImmediateContext, SCDesc, reinterpret_cast<void*>(static_cast<size_t>(Wnd)), pDisplay, ppSwapChain );
}

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

static bool ctxErrorOccurred = false;
static int ctxErrorHandler( Display *dpy, XErrorEvent *ev )
{
    ctxErrorOccurred = true;
    return 0;
}

int main(int argc, char *argv[]) {
    Display *display = XOpenDisplay(NULL);

  if (!display)
  {
    printf("Failed to open X display\n");
    exit(1);
  }

  // Get a matching FB config
  static int visual_attribs[] =
    {
      GLX_X_RENDERABLE    , True,
      GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
      GLX_RENDER_TYPE     , GLX_RGBA_BIT,
      GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
      GLX_RED_SIZE        , 8,
      GLX_GREEN_SIZE      , 8,
      GLX_BLUE_SIZE       , 8,
      GLX_ALPHA_SIZE      , 8,
      GLX_DEPTH_SIZE      , 24,
      GLX_STENCIL_SIZE    , 8,
      GLX_DOUBLEBUFFER    , True,
      //GLX_SAMPLE_BUFFERS  , 1,
      //GLX_SAMPLES         , 4,
      None
    };

  int glx_major, glx_minor;
 
  // FBConfigs were added in GLX version 1.3.
  if ( !glXQueryVersion( display, &glx_major, &glx_minor ) || 
       ( ( glx_major == 1 ) && ( glx_minor < 3 ) ) || ( glx_major < 1 ) )
  {
    printf("Invalid GLX version");
    exit(1);
  }

  printf( "Getting matching framebuffer configs\n" );
  int fbcount;
  GLXFBConfig* fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
  if (!fbc)
  {
    printf( "Failed to retrieve a framebuffer config\n" );
    exit(1);
  }
  printf( "Found %d matching FB configs.\n", fbcount );

  // Pick the FB config/visual with the most samples per pixel
  printf( "Getting XVisualInfos\n" );
  int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

  int i;
  for (i=0; i<fbcount; ++i)
  {
    XVisualInfo *vi = glXGetVisualFromFBConfig( display, fbc[i] );
    if ( vi )
    {
      int samp_buf, samples;
      glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
      glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLES       , &samples  );
      
      printf( "  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d,"
              " SAMPLES = %d\n", 
              i, vi -> visualid, samp_buf, samples );

      if ( best_fbc < 0 || samp_buf && samples > best_num_samp )
        best_fbc = i, best_num_samp = samples;
      if ( worst_fbc < 0 || !samp_buf || samples < worst_num_samp )
        worst_fbc = i, worst_num_samp = samples;
    }
    XFree( vi );
  }

  GLXFBConfig bestFbc = fbc[ best_fbc ];

  // Be sure to free the FBConfig list allocated by glXChooseFBConfig()
  XFree( fbc );

  // Get a visual
  XVisualInfo *vi = glXGetVisualFromFBConfig( display, bestFbc );
  printf( "Chosen visual ID = 0x%x\n", vi->visualid );

  printf( "Creating colormap\n" );
  XSetWindowAttributes swa;
  Colormap cmap;
  swa.colormap = cmap = XCreateColormap( display,
                                         RootWindow( display, vi->screen ), 
                                         vi->visual, AllocNone );
  swa.background_pixmap = None ;
  swa.border_pixel      = 0;
  swa.event_mask        = StructureNotifyMask;

  printf( "Creating window\n" );
  Window win = XCreateWindow( display, RootWindow( display, vi->screen ), 
                              0, 0, 1024, 768, 0, vi->depth, InputOutput, 
                              vi->visual, 
                              CWBorderPixel|CWColormap|CWEventMask, &swa );
  auto wnd = win;
  if ( !win )
  {
    printf( "Failed to create window.\n" );
    exit(1);
  }

  // Done with the visual info data
  XFree( vi );

  XStoreName( display, win, "GL 3.0 Window" );

  printf( "Mapping window\n" );
  XMapWindow( display, win );

  // Get the default screen's GLX extension list
  const char *glxExts = glXQueryExtensionsString( display,
                                                  DefaultScreen( display ) );

  // NOTE: It is not necessary to create or make current to a context before
  // calling glXGetProcAddressARB
  glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
  glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
           glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

  GLXContext ctx = 0;

  // Install an X error handler so the application won't exit if GL 3.0
  // context allocation fails.
  //
  // Note this error handler is global.  All display connections in all threads
  // of a process use the same error handler, so be sure to guard against other
  // threads issuing X commands while this code is running.
  ctxErrorOccurred = false;
  int (*oldHandler)(Display*, XErrorEvent*) =
      XSetErrorHandler(&ctxErrorHandler);

  // Check for the GLX_ARB_create_context extension string and the function.
  // If either is not present, use GLX 1.3 context creation method.
//   if ( !isExtensionSupported( glxExts, "GLX_ARB_create_context" ) ||
//        !glXCreateContextAttribsARB )
//   {
//     printf( "glXCreateContextAttribsARB() not found"
//             " ... using old-style GLX context\n" );
//     ctx = glXCreateNewContext( display, bestFbc, GLX_RGBA_TYPE, 0, True );
//   }

//   // If it does, try to get a GL 3.0 context!
//   else
  {
    int context_attribs[] =
      {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
      };

    printf( "Creating context\n" );
    ctx = glXCreateContextAttribsARB( display, bestFbc, 0,
                                      True, context_attribs );

    // Sync to ensure any errors generated are processed.
    XSync( display, False );
    if ( !ctxErrorOccurred && ctx )
      printf( "Created GL 3.0 context\n" );
    else
    {
      // Couldn't create GL 3.0 context.  Fall back to old-style 2.x context.
      // When a context version below 3.0 is requested, implementations will
      // return the newest context version compatible with OpenGL versions less
      // than version 3.0.
      // GLX_CONTEXT_MAJOR_VERSION_ARB = 1
      context_attribs[1] = 1;
      // GLX_CONTEXT_MINOR_VERSION_ARB = 0
      context_attribs[3] = 0;

      ctxErrorOccurred = false;

      printf( "Failed to create GL 3.0 context"
              " ... using old-style GLX context\n" );
      ctx = glXCreateContextAttribsARB( display, bestFbc, 0, 
                                        True, context_attribs );
    }
  }

  // Sync to ensure any errors generated are processed.
  XSync( display, False );

  // Restore the original error handler
  XSetErrorHandler( oldHandler );

  if ( ctxErrorOccurred || !ctx )
  {
    printf( "Failed to create an OpenGL context\n" );
    exit(1);
  }

  // Verifying that context is a direct context
  if ( ! glXIsDirect ( display, ctx ) )
  {
    printf( "Indirect GLX rendering context obtained\n" );
  }
  else
  {
    printf( "Direct GLX rendering context obtained\n" );
  }

  printf( "Making context current\n" );
  glXMakeCurrent( display, win, ctx );
    
#if 0
    char *display_name = nullptr; //  if the display_name is NULL, it defaults to the value of the DISPLAY environment variable.
    Display* display = XOpenDisplay(display_name); 
    if(display == NULL) 
    {
        LOG_ERROR("\n\tcannot connect to X server\n\n");
        exit(1);
    }
    
    // The root window is the "desktop background" window.
    Window rootWnd = DefaultRootWindow(display);

    GLint attrList[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    // Find a visual that matches specified attributes
    XVisualInfo *xVisInfo = glXChooseVisual(display, 0, attrList);
    if(xVisInfo == NULL) 
    {
        LOG_ERROR("\n\tNo appropriate visual found\n\n");
        exit(1);
    } 

    XSetWindowAttributes SetWndAttrs;
    SetWndAttrs.colormap = XCreateColormap(display, rootWnd, xVisInfo->visual, AllocNone);
    SetWndAttrs.event_mask = ExposureMask | KeyPressMask;
    
    Window wnd = XCreateWindow(display, rootWnd, 0, 0, 1024, 768, 0, xVisInfo->depth, InputOutput, xVisInfo->visual, CWColormap | CWEventMask, &SetWndAttrs);

    XMapWindow(display, wnd);
    //XStoreName(dpy, win, "VERY SIMPLE APPLICATION");
    
    auto tmpCtx = glXCreateContext(display, xVisInfo, NULL, GL_TRUE);
    glXMakeCurrent(display, wnd, tmpCtx);

    glxewInit();
    int scrnum = DefaultScreen(display);
    int elemc = 0;
    GLXFBConfig *fbcfg = glXChooseFBConfig(display, scrnum, NULL, &elemc);
    if (!fbcfg)
    {
        LOG_ERROR("Couldn't get FB configs\n");
        exit(1);
    }

    int glattr[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    glc = glXCreateContextAttribsARB(display, fbcfg[0], NULL, true, glattr);

    //XFree(xVisInfo);        

    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, tmpCtx);

    glXMakeCurrent(display, wnd, glc);
#endif
#if 0
    // glewExperimental = GL_TRUE;
    //GLenum err = glewInit();
    //if( GLEW_OK != err ){
    //    LOG_ERROR( "Failed to initialize GLEW" );
    //    exit(1);
    //}

    RefCntAutoPtr<IRenderDevice> pRenderDevice;
    RefCntAutoPtr<IDeviceContext> pDeviceContext;
    Diligent::RefCntAutoPtr<ISwapChain> pSwapChain;
    InitDevice( wnd, display, &pRenderDevice, &pDeviceContext, &pSwapChain );
    //g_pSwapChain = pSwapChain;

    // Initialize AntTweakBar
    // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with 
    // very old GL specification. Using these modes results in applying some 
    // odd offsets which distorts everything
    // Latest OpenGL works very much like Direct3D11, and 
    // Tweak Bar will never know if D3D or OpenGL is actually used
    if (!TwInit(TW_DIRECT3D11, pRenderDevice.RawPtr(), pDeviceContext.RawPtr(), pSwapChain->GetDesc().ColorBufferFormat))
    {
        LOG_ERROR_MESSAGE("AntTweakBar initialization failed");
        return 0;
    }

    g_pSample.reset( CreateSample(pRenderDevice, pDeviceContext, pSwapChain) );
    g_pSample->WindowResize( pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height );
    std::string Title = g_pSample->GetSampleName();
#endif
    Timer Timer;
    auto PrevTime = Timer.GetElapsedTime();
    double filteredFrameTime = 0.0;

    while (1) {
        auto CurrTime = Timer.GetElapsedTime();
        auto ElapsedTime = CurrTime - PrevTime;
        PrevTime = CurrTime;

        XNextEvent(display, &xev);

        if (xev.type == GraphicsExpose) {
#if 0            
            pDeviceContext->InvalidateState();
            pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
            g_pSample->Update(CurrTime, ElapsedTime);
            g_pSample->Render();

            // Draw tweak bars
            // Restore default render target in case the sample has changed it
            pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
            TwDraw();
#endif
            XGetWindowAttributes(display, wnd, &gwa);
                glViewport(0, 0, gwa.width, gwa.height);
                glClearColor(0.2f, 0.5f, 0.6f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);

            glXSwapBuffers(display, wnd);
            //pSwapChain->Present();
        }
        else if (xev.type == KeyPress) {
            glXMakeCurrent(display, None, NULL);
            glXDestroyContext(display, glc);
            XDestroyWindow(display, wnd);
            XCloseDisplay(display);
            break;
        }

        double filterScale = 0.2;
        filteredFrameTime = filteredFrameTime * (1.0 - filterScale) + filterScale * ElapsedTime;
        std::stringstream fpsCounterSS;
        fpsCounterSS << " - " << std::fixed << std::setprecision(1) << filteredFrameTime * 1000;
        fpsCounterSS << " ms (" << 1.0 / filteredFrameTime << " fps)";
        //XStoreName(display, wnd, (Title + fpsCounterSS.str()).c_str());
    }

    g_pSample.reset();
#if 0    
    pSwapChain.Release();
    pDeviceContext.Release();
    pRenderDevice.Release();
#endif

    exit(0);
} 

#if 0


LRESULT CALLBACK MessageProc(HWND, UINT, WPARAM, LPARAM);

ISwapChain *g_pSwapChain;

// Main
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmdShow)
{
#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    DeviceType DevType = DeviceType::Undefined;
    std::wstring CmdLine = GetCommandLine();
    std::wstring Key = L"mode=";
    auto pos = CmdLine.find( Key );
    if( pos != std::string::npos )
    {
        pos += Key.length();
        auto Val = CmdLine.substr( pos );
        if(Val == L"D3D11")
        {
            DevType = DeviceType::D3D11;
            Title.append( L" (D3D11)" );
        }
        else if(Val == L"D3D12")
        {
            DevType = DeviceType::D3D12;
            Title.append( L" (D3D12)" );
        }
        else if(Val == L"GL")
        {
            DevType = DeviceType::OpenGL;
            Title.append( L" (OpenGL)" );
        }
        else
        {
            LOG_ERROR("Unknown device type. Only the following types are supported: D3D11, D3D12, GL")
            return -1;
        }
    }
    else
    {
        LOG_INFO_MESSAGE("Device type is not specified. Using D3D11 device");
        DevType = DeviceType::D3D11;
        Title.append( L" (D3D11)" );
    }
    // Register our window class
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW|CS_VREDRAW, MessageProc,
                        0L, 0L, instance, NULL, NULL, NULL, NULL, L"SampleApp", NULL };
    RegisterClassEx(&wcex);

    // Create a window
    RECT rc = { 0, 0, 1280, 1024 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(L"SampleApp", Title.c_str(), 
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
                            rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, instance, NULL);
    if (!wnd)
    {
        MessageBox(NULL, L"Cannot create window", L"Error", MB_OK|MB_ICONERROR);
        return 0;
    }
    ShowWindow(wnd, cmdShow);
    UpdateWindow(wnd);


    // Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {


            double filterScale = 0.2;
            filteredFrameTime = filteredFrameTime * (1.0 - filterScale) + filterScale * ElapsedTime;
            std::wstringstream fpsCounterSS;
            fpsCounterSS << " - " << std::fixed << std::setprecision(1) << filteredFrameTime * 1000;
            fpsCounterSS << " ms (" << 1.0 / filteredFrameTime << " fps)";
            SetWindowText(wnd, (Title + fpsCounterSS.str()).c_str());
        }
    }
    
    TwTerminate();

	//pDeviceContext->Flush();

    return (int)msg.wParam;
}

// Called every time the application receives a message
LRESULT CALLBACK MessageProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Send event message to AntTweakBar
    if (TwEventWin(wnd, message, wParam, lParam))
        return 0; // Event has been handled by AntTweakBar
    
    switch (message) 
    {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                BeginPaint(wnd, &ps);
                EndPaint(wnd, &ps);
                return 0;
            }
        case WM_SIZE: // Window size has been changed
            if( g_pSwapChain )
            {
                g_pSwapChain->Resize(LOWORD(lParam), HIWORD(lParam));
                g_pSample->WindowResize( g_pSwapChain->GetDesc().Width, g_pSwapChain->GetDesc().Height );
            }
            return 0;
        case WM_CHAR:
            if (wParam == VK_ESCAPE)
                PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
        {
            struct WindowMessageData
            {
                HWND hWnd;
                UINT message;
                WPARAM wParam;
                LPARAM lParam;
            }msg{wnd, message, wParam, lParam};
            if (g_pSample != nullptr && g_pSample->HandleNativeMessage(&msg) )
                return 0;
            else
                return DefWindowProc(wnd, message, wParam, lParam);
        }
    }
}
#endif
#endif
#endif