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

#include <queue>
#include "SampleApp.h"
#include "AntTweakBar.h"

namespace Diligent
{

class SampleAppIOS final : public SampleApp
{
public:
    SampleAppIOS()
    {
        m_DeviceType = DeviceType::OpenGLES;
    }

    virtual void OnGLContextCreated(void *eaglLayer)override final
    {
        InitializeDiligentEngine(eaglLayer);
        InitializeSample();
    }
    
    virtual void Render()override
    {
        /*m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr);
        // Handle all TwBar events here as the event handlers call draw commands
        // and thus cannot be used in the UI thread
        while(!TwBarEvents.empty())
        {
            const auto& event = TwBarEvents.front();
            switch (event.type)
            {
                case TwEvent::LMB_PRESSED:
                case TwEvent::RMB_PRESSED:
                    TwMouseButton(TW_MOUSE_PRESSED, event.type == TwEvent::LMB_PRESSED ? TW_MOUSE_LEFT : TW_MOUSE_RIGHT);
                    break;
                    
                case TwEvent::LMB_RELEASED:
                case TwEvent::RMB_RELEASED:
                    TwMouseButton(TW_MOUSE_RELEASED, event.type == TwEvent::LMB_RELEASED ? TW_MOUSE_LEFT : TW_MOUSE_RIGHT);
                    break;
                    
                case TwEvent::MOUSE_MOVE:
                    TwMouseMotion(event.mouseX, event.mouseY);
                    break;
                    
                case TwEvent::KEY_PRESSED:
                    TwKeyPressed(event.key, 0);
                    break;
            }
            TwBarEvents.pop();
        }*/
        
        SampleApp::Render();
    }

    virtual void OnTouchBegan(float x, float y)override final
    {
        TwMouseMotion(static_cast<int>(x), static_cast<int>(y));
        TwMouseButton(TW_MOUSE_PRESSED, TW_MOUSE_LEFT);
    }
    
    virtual void OnTouchMoved(float x, float y)override final
    {
        TwMouseMotion(static_cast<int>(x), static_cast<int>(y));
    }
    
    virtual void OnTouchEnded(float x, float y)override final
    {
        TwMouseMotion(static_cast<int>(x), static_cast<int>(y));
        TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_LEFT);
    }
    
private:
    /*
    // Unfortunately TwBar library calls rendering
    // functions from event handlers, which does not work on MacOS
    // as UI events and rendering are handled by separate threads
    struct TwEvent
    {
        enum EVENT_TYPE
        {
            LMB_PRESSED,
            LMB_RELEASED,
            RMB_PRESSED,
            RMB_RELEASED,
            MOUSE_MOVE,
            KEY_PRESSED
        }type;
        int mouseX = 0;
        int mouseY = 0;
        int key = 0;
        
        TwEvent(EVENT_TYPE _type) : type(_type){}
        TwEvent(int x, int y) : type(MOUSE_MOVE), mouseX(x), mouseY(y){}
        TwEvent(int k) : type(KEY_PRESSED), key(k){}
    };
    std::queue<TwEvent> TwBarEvents;*/
};

NativeAppBase* CreateApplication()
{
    return new SampleAppIOS;
}

}
