/*     Copyright 2015-2018 Egor Yusov
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

#include "SampleApp.h"
#include "AntTweakBar.h"
#include "resources/Win32AppResource.h"

namespace
{

Diligent::DeviceType g_DeviceType = Diligent::DeviceType::Undefined;

void SetButtonImage(HWND hwndDlg, int buttonId, int imageId, BOOL Enable)
{
    HBITMAP hBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(imageId));
    auto hButton = GetDlgItem(hwndDlg, buttonId);
    SendMessage(hButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hBitmap);
    EnableWindow(hButton, Enable);
}

INT_PTR CALLBACK SelectDeviceTypeDialogProc(HWND hwndDlg, 
                                            UINT message, 
                                            WPARAM wParam, 
                                            LPARAM lParam) 
{ 
    switch (message) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case ID_DIRECT3D11: 
                    g_DeviceType = Diligent::DeviceType::D3D11;
                    EndDialog(hwndDlg, wParam); 
                    return TRUE;

                case ID_DIRECT3D12:
                    g_DeviceType = Diligent::DeviceType::D3D12;
                    EndDialog(hwndDlg, wParam); 
                    return TRUE;

                case ID_OPENGL:
                    g_DeviceType = Diligent::DeviceType::OpenGL;
                    EndDialog(hwndDlg, wParam); 
                    return TRUE;

                case ID_VULKAN:
                    g_DeviceType = Diligent::DeviceType::Vulkan;
                    EndDialog(hwndDlg, wParam); 
                    return TRUE;
            } 
        break;

        case WM_INITDIALOG:
        {
#if D3D11_SUPPORTED
            BOOL D3D11Supported = TRUE;
#else
            BOOL D3D11Supported = FALSE;
#endif
            SetButtonImage(hwndDlg, ID_DIRECT3D11, IDB_DIRECTX11_LOGO, D3D11Supported);

#if D3D12_SUPPORTED
            BOOL D3D12Supported = TRUE;
#else
            BOOL D3D12Supported = FALSE;
#endif
            SetButtonImage(hwndDlg, ID_DIRECT3D12, IDB_DIRECTX12_LOGO, D3D12Supported);

#if GL_SUPPORTED
            BOOL OpenGLSupported = TRUE;
#else
            BOOL OpenGLSupported = FALSE;
#endif
            SetButtonImage(hwndDlg, ID_OPENGL, IDB_OPENGL_LOGO, OpenGLSupported);

#if VULKAN_SUPPORTED
            BOOL VulkanSupported = TRUE;
#else
            BOOL VulkanSupported = FALSE;
#endif
            SetButtonImage(hwndDlg, ID_VULKAN, IDB_VULKAN_LOGO, VulkanSupported);
        }
        break;
    } 
    return FALSE; 
} 

}

class SampleAppWin32 final : public SampleApp
{
public:
    virtual LRESULT HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)override final
    {
        switch (message)
        {
            case WM_SYSKEYDOWN:
                // Handle ALT+ENTER:
                if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
                {
                    //if (pSample && pSample->GetTearingSupport())
                    {
                        ToggleFullscreenWindow();
                        return 0;
                    }
                }
            // Send all other WM_SYSKEYDOWN messages to the default WndProc.
            break;
        }

        // Send event message to AntTweakBar
        return TwEventWin(hWnd, message, wParam, lParam);
    }

    virtual void OnWindowCreated(HWND hWnd, LONG WindowWidth, LONG WindowHeight)override final
    {
        m_hWnd = hWnd;
        InitializeDiligentEngine(hWnd);
        InitializeSample();
    }


protected:
    void ToggleFullscreenWindow()
    {
        // Ignore if we are in exclusive fullscreen mode
        if(m_bFullScreenMode)
            return;

        m_bFullScreenWindow = !m_bFullScreenWindow;

        if (m_bFullScreenWindow)
        {
            // Save the old window rect so we can restore it when exiting fullscreen mode.
            GetWindowRect(m_hWnd, &m_WindowRect);
            // Save the original window style
            m_WindowStyle = GetWindowLong(m_hWnd, GWL_STYLE);

            // Make the window borderless so that the client area can fill the screen.
            SetWindowLong(m_hWnd, GWL_STYLE, m_WindowStyle & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

            // Get the settings of the primary display. We want the app to go into
            // fullscreen mode on the display that supports Independent Flip.
            DEVMODE devMode = {};
            devMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

            SetWindowPos(
                m_hWnd,
                HWND_TOPMOST,
                devMode.dmPosition.x,
                devMode.dmPosition.y,
                devMode.dmPosition.x + devMode.dmPelsWidth,
                devMode.dmPosition.y + devMode.dmPelsHeight,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_hWnd, SW_MAXIMIZE);
        }
        else
        {
            // Restore the window's attributes and size.
            SetWindowLong(m_hWnd, GWL_STYLE, m_WindowStyle);

            SetWindowPos(
                m_hWnd,
                HWND_NOTOPMOST,
                m_WindowRect.left,
                m_WindowRect.top,
                m_WindowRect.right - m_WindowRect.left,
                m_WindowRect.bottom - m_WindowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_hWnd, SW_NORMAL);
        }
    }
    
    virtual void SetFullscreenMode(const Diligent::DisplayModeAttribs &DisplayMode)override 
    { 
        if(m_bFullScreenWindow)
        {
            // We must exit full screen window first.
            ToggleFullscreenWindow();
        }
        SampleApp::SetFullscreenMode(DisplayMode);
    }

    virtual void SetWindowedMode()override
    { 
        if (m_bFullScreenWindow)
        {
            // Exit full screen window
            ToggleFullscreenWindow();
        }
        SampleApp::SetWindowedMode();
    }

    virtual void SelectDeviceType()override final
    {
        DialogBox(NULL, MAKEINTRESOURCE(IDD_DEVICE_TYPE_SELECTION_DIALOG), NULL, SelectDeviceTypeDialogProc);
        m_DeviceType = g_DeviceType;
    }

    bool m_bFullScreenWindow = false;
    HWND m_hWnd = 0;

private:
    RECT m_WindowRect = {};
    LONG m_WindowStyle = 0;
};

NativeAppBase* CreateApplication()
{
    return new SampleAppWin32;
}
