/*
 *  Copyright 2026 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#import "VisionOSAppBridge.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "Tutorial30_Immersive.hpp"
#include "Tutorial30_Preview.hpp"
#include "DebugUtilities.hpp"

using namespace Diligent;

namespace
{

dispatch_queue_t GetRenderQueue()
{
    static dispatch_queue_t RenderQueue =
        dispatch_queue_create("com.diligentengine.tutorial30.render",
                              dispatch_queue_attr_make_with_qos_class(
                                  DISPATCH_QUEUE_SERIAL,
                                  QOS_CLASS_USER_INTERACTIVE, 0));
    return RenderQueue;
}

} // namespace


@implementation VisionOSImmersiveBridge
{
    cp_layer_renderer_t _layerRenderer;
}

- (instancetype)initWithLayerRenderer:(cp_layer_renderer_t)layerRenderer
{
    self = [super init];
    _layerRenderer = layerRenderer;
    return self;
}

- (void)run
{
    cp_layer_renderer_t capturedLayerRenderer = [(id)_layerRenderer retain];

    dispatch_async(GetRenderQueue(), ^{
        auto tutorialView = std::make_unique<Tutorial30_Immersive>((__bridge void*)capturedLayerRenderer);

        bool isRendering = true;
        while (isRendering) @autoreleasepool
        {
            switch (cp_layer_renderer_get_state(capturedLayerRenderer))
            {
                case cp_layer_renderer_state_paused:
                    cp_layer_renderer_wait_until_running(capturedLayerRenderer);
                    break;

                case cp_layer_renderer_state_running:
                    tutorialView->RenderFrame();
                    break;

                case cp_layer_renderer_state_invalidated:
                    isRendering = false;
                    break;
            }
        }
        // Destructor releases all Diligent resources.
        [(id)capturedLayerRenderer release];
    });
}

@end


@implementation VisionOSPreviewBridge
{
    std::unique_ptr<Tutorial30_Preview> _tutorialView;
    BOOL                                _frameInFlight; // Touched only from main thread
}

- (instancetype)initWithMetalLayer:(CAMetalLayer*)layer
                           widthPx:(NSInteger)widthPx
                          heightPx:(NSInteger)heightPx
{
    self = [super init];
    dispatch_sync(GetRenderQueue(), ^{
        _tutorialView = std::make_unique<Tutorial30_Preview>((__bridge void*)layer,
                                                             static_cast<Uint32>(widthPx),
                                                             static_cast<Uint32>(heightPx));
    });
    return self;
}

- (void)dealloc
{
    Tutorial30_Preview* pTutorialView = _tutorialView.release();
    if (pTutorialView != nullptr)
    {
        dispatch_async(GetRenderQueue(), ^{
            delete pTutorialView;
        });
    }
    [super dealloc];
}

- (void)resizeToWidth:(NSInteger)width height:(NSInteger)height
{
    VERIFY_EXPR([NSThread isMainThread]);

    if (_tutorialView == nullptr || width <= 0 || height <= 0)
        return;

    VisionOSPreviewBridge* bridge = [self retain];
    dispatch_async(GetRenderQueue(), ^{
        if (bridge->_tutorialView != nullptr)
            bridge->_tutorialView->Resize(static_cast<Uint32>(width), static_cast<Uint32>(height));
        [bridge release];
    });
}

- (void)renderFrame
{
    VERIFY_EXPR([NSThread isMainThread]);

    if (_tutorialView == nullptr || _frameInFlight)
        return;

    _frameInFlight = YES;

    VisionOSPreviewBridge* bridge = [self retain];
    dispatch_async(GetRenderQueue(), ^{
        if (bridge->_tutorialView != nullptr)
            bridge->_tutorialView->RenderFrame();

        dispatch_async(dispatch_get_main_queue(), ^{
            bridge->_frameInFlight = NO;
            [bridge release];
        });
    });
}

@end
