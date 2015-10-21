/*     Copyright 2015 Egor Yusov
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

#pragma once

namespace DX
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
	interface IDeviceNotify
	{
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};

	// Controls all the DirectX device resources.
	class DeviceResources
	{
	public:
		DeviceResources();
		void SetWindow(Windows::UI::Core::CoreWindow^ window);
		void SetLogicalSize(Windows::Foundation::Size logicalSize);
		void SetCurrentOrientation(Windows::Graphics::Display::DisplayOrientations currentOrientation);
		void SetDpi(float dpi);
		void ValidateDevice();
		void HandleDeviceLost();
		void RegisterDeviceNotify(IDeviceNotify* deviceNotify);
		void Trim();
		void Present();

		// Device Accessors.
		Windows::Foundation::Size GetOutputSize() const					{ return m_outputSize; }
		Windows::Foundation::Size GetLogicalSize() const				{ return m_logicalSize; }

		// Device Accessors.
		DirectX::XMFLOAT4X4		GetOrientationTransform3D() const		{ return m_orientationTransform3D; }

        Diligent::IRenderDevice*          GetDevice()                             { return m_pRenderDevice; }
        Diligent::IDeviceContext*         GetDeviceContext()                      { return m_pDeviceContext; }
        Diligent::ISwapChain*             GetSwapChain()                          { return m_pSwapChain;  }

	private:
		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();
		DXGI_MODE_ROTATION ComputeDisplayRotation();

        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pRenderDevice;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pDeviceContext;
        Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;

		// Direct3D objects.
		Microsoft::WRL::ComPtr<ID3D11Device2>			m_d3dDevice;
		Microsoft::WRL::ComPtr<IDXGISwapChain1>			m_swapChain;

		// Cached reference to the Window.
		Platform::Agile<Windows::UI::Core::CoreWindow> m_window;

		// Cached device properties.
		Windows::Foundation::Size						m_d3dRenderTargetSize;
		Windows::Foundation::Size						m_outputSize;
		Windows::Foundation::Size						m_logicalSize;
		Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
		Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;
		float											m_dpi;

		// Transforms used for display orientation.
		DirectX::XMFLOAT4X4	m_orientationTransform3D;

		// The IDeviceNotify can be held directly as it owns the DeviceResources.
		IDeviceNotify* m_deviceNotify;
	};
}