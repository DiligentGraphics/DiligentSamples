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

#ifndef DILIGENT_TUTORIAL30_VISIONOS_APP_BRIDGE_H
#define DILIGENT_TUTORIAL30_VISIONOS_APP_BRIDGE_H

#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CompositorServices/CompositorServices.h>

NS_ASSUME_NONNULL_BEGIN

/// Drives the immersive (stereo) Diligent render loop bound to a
/// CompositorServices layer renderer.
@interface VisionOSImmersiveBridge : NSObject

/// Captures the layer renderer.
- (instancetype)initWithLayerRenderer:(cp_layer_renderer_t)layerRenderer;

/// Starts the immersive Diligent render loop on the shared serial render queue.
- (void)run;

@end


/// Drives the preview shown in the launcher window.
@interface VisionOSPreviewBridge : NSObject

/// Creates the swap chain bound to `layer`.
- (instancetype)initWithMetalLayer:(CAMetalLayer*)layer
                           widthPx:(NSInteger)widthPx
                          heightPx:(NSInteger)heightPx;

/// Queues a preview resize on the shared serial render queue.
- (void)resizeToWidth:(NSInteger)width height:(NSInteger)height;

/// Queues a single preview frame on the shared serial render queue.
- (void)renderFrame;

@end

NS_ASSUME_NONNULL_END

#endif // DILIGENT_TUTORIAL30_VISIONOS_APP_BRIDGE_H
