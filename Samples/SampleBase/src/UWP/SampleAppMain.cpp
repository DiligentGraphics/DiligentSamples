/*     Copyright 2015-2016 Egor Yusov
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

#include "pch.h"
#include "pch2.h"
#include "SampleAppMain.h"
#include "Common\DirectXHelper.h"

#include "FileSystem.h"
#include "Timer.h"

#include "AntTweakBar.h"

using namespace SampleApp;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;
using namespace Diligent;

// The DirectX 12 Application template is documented at http://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
SampleAppMain::SampleAppMain()
{
    m_pSample.reset( CreateSample() );

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

// Creates and initializes the renderers.
void SampleAppMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
    m_deviceResources = deviceResources;
	// TODO: Replace this with your app's content initialization.

    // Initialize AntTweakBar
    // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with 
    // very old GL specification. Using these modes results in applying some 
    // odd offsets which distorts everything
    // Latest OpenGL works very much like Direct3D11, and 
    // Tweak Bar will never know if D3D or OpenGL is actually used
    auto pDevice = m_deviceResources->GetDevice();
    auto ppContexts = m_deviceResources->GetDeviceContexts();
    auto pSwapChain = m_deviceResources->GetSwapChain();
    if (!TwInit(TW_DIRECT3D11, pDevice, m_deviceResources->GetImmediateContext(), pSwapChain->GetDesc().ColorBufferFormat))
    {
        //MessageBoxA(wnd, TwGetLastError(), "AntTweakBar initialization failed", MB_OK|MB_ICONERROR);
        //return 0;
    }
    TwDefine(" TW_HELP visible=false ");

    m_pSample->Initialize(pDevice, ppContexts, m_deviceResources->GetNumDeferredContexts(), pSwapChain) ;
    m_pSample->WindowResize( pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height );

    TwWindowSize(pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height);


	OnWindowSizeChanged();
}

// Updates the application state once per frame.
void SampleAppMain::Update()
{
	// Update scene objects.
	m_timer.Tick([&]()
	{
        static Timer timer;
        static double PrevTime = timer.GetElapsedTime();
        auto CurrTime = timer.GetElapsedTime();
        auto ElapsedTime = CurrTime - PrevTime;
        PrevTime = CurrTime;
        m_pSample->Update(CurrTime, ElapsedTime);
	});
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool SampleAppMain::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	auto context = m_deviceResources->GetImmediateContext();

	// Reset the viewport to target the whole screen.
    context->SetViewports( 1, nullptr, 0, 0 );

	// Reset render targets to the screen.
    context->SetRenderTargets( 0, nullptr, nullptr );

    m_pSample->Render();

	// Draw tweak bars
    TwDraw();
    
    return true;
}

// Updates application state when the window's size changes (e.g. device orientation change)
void SampleAppMain::OnWindowSizeChanged()
{
	// TODO: Replace this with the size-dependent initialization of your app's content.
	//m_sceneRenderer->CreateWindowSizeDependentResources();
    auto pSwapChain = m_deviceResources->GetSwapChain();
    m_pSample->WindowResize( pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height );
    
        TwWindowSize(pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height);

}

// Notifies the app that it is being suspended.
void SampleAppMain::OnSuspending()
{
	// TODO: Replace this with your app's suspending logic.

	// Process lifetime management may terminate suspended apps at any time, so it is
	// good practice to save any state that will allow the app to restart where it left off.

	//m_sceneRenderer->SaveState();

	// If your application uses video memory allocations that are easy to re-create,
	// consider releasing that memory to make it available to other applications.
}

// Notifes the app that it is no longer suspended.
void SampleAppMain::OnResuming()
{
	// TODO: Replace this with your app's resuming logic.
}

// Notifies renderers that device resources need to be released.
void SampleAppMain::OnDeviceRemoved()
{
	// TODO: Save any necessary application or renderer state and release the renderer
	// and its resources which are no longer valid.
	//m_sceneRenderer->SaveState();
	m_pSample = nullptr;
}
