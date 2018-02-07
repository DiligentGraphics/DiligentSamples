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

class SampleAppWin32 final : public SampleApp
{
public:
    virtual void Render()override final;
    virtual LRESULT HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)override final;
};

NativeAppBase* CreateApplication()
{
    return new SampleAppWin32;
}

void SampleAppWin32::Render()
{
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr);
    m_TheSample->Render();

    // Draw tweak bars
    // Restore default render target in case the sample has changed it
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr);
    TwDraw();

    m_pSwapChain->Present();
}

LRESULT SampleAppWin32::HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Send event message to AntTweakBar
    return TwEventWin(hWnd, message, wParam, lParam);
}
