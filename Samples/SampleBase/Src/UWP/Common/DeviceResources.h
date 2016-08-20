#pragma once

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "RefCntAutoPtr.h"

namespace DX
{
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
		void Present();


		// The size of the render target, in pixels.
		Windows::Foundation::Size	GetOutputSize() const				{ return m_outputSize; }

		// The size of the render target, in dips.
		Windows::Foundation::Size	GetLogicalSize() const				{ return m_logicalSize; }

		float						GetDpi() const						{ return m_effectiveDpi; }
		bool						IsDeviceRemoved() const				{ return m_deviceRemoved; }

		// D3D Accessors.
		DirectX::XMFLOAT4X4			GetOrientationTransform3D() const	{ return m_orientationTransform3D; }

        Diligent::IRenderDevice*          GetDevice()                             { return m_pRenderDevice; }
        Diligent::IDeviceContext*         GetDeviceContext()                      { return m_pDeviceContext; }
        Diligent::ISwapChain*             GetSwapChain()                          { return m_pSwapChain;  }

	private:
		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();
		void UpdateRenderTargetSize();
		DXGI_MODE_ROTATION ComputeDisplayRotation();

        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pRenderDevice;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pDeviceContext;
        Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;
        bool											m_deviceRemoved;

		// Direct3D objects.
		Microsoft::WRL::ComPtr<ID3D12Device>			m_d3d12Device;
        Microsoft::WRL::ComPtr<ID3D11Device>			m_d3d11Device;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>			m_swapChain;

		// Cached reference to the Window.
		Platform::Agile<Windows::UI::Core::CoreWindow>	m_window;

		// Cached device properties.
		Windows::Foundation::Size						m_d3dRenderTargetSize;
		Windows::Foundation::Size						m_outputSize;
		Windows::Foundation::Size						m_logicalSize;
		Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
		Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;
		float											m_dpi;

		// This is the DPI that will be reported back to the app. It takes into account whether the app supports high resolution screens or not.
		float											m_effectiveDpi;

		// Transforms used for display orientation.
		DirectX::XMFLOAT4X4								m_orientationTransform3D;

        bool m_UseD3D12 = true;
	};
}
