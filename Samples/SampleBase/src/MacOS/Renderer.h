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

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "RefCntAutoPtr.h"
#include "SampleBase.h"
#include "Timer.h"
#include <queue>

class Renderer
{
public:
    Renderer();
    
    ~Renderer();
    void Init(
#if PLATFORM_IOS
              void  *layer
#endif
    );
    void WindowResize(int width, int height);
    void Render();
    void OnMouseDown(int button);
    void OnMouseUp(int button);
    void OnMouseMove(int x, int y);
    void OnKeyPressed(int key);
    const char* GetSampleName()const{return pSample->GetSampleName();}
    
private:
    std::unique_ptr<SampleBase> pSample;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pRenderDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pDeviceContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> pSwapChain;
    Diligent::Timer timer;
    double PrevTime = 0.0;

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
    std::queue<TwEvent> TwBarEvents;
};

