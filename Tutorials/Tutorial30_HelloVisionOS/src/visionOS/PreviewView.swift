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

import SwiftUI
import UIKit
import QuartzCore

final class PreviewMetalView: UIView
{
    override class var layerClass: AnyClass { CAMetalLayer.self }

    var metalLayer: CAMetalLayer
    {
        layer as! CAMetalLayer
    }
}

struct PreviewView: UIViewRepresentable
{
    var isPaused: Bool = false

    func makeCoordinator() -> Coordinator
    {
        Coordinator(isPaused: isPaused)
    }

    func makeUIView(context: Context) -> PreviewMetalView
    {
        let view = PreviewMetalView(frame: .zero)
        context.coordinator.isPaused = isPaused
        context.coordinator.attach(to: view)
        return view
    }

    func updateUIView(_ uiView: PreviewMetalView, context: Context)
    {
        context.coordinator.isPaused = isPaused
        context.coordinator.resizeIfNeeded()
    }

    static func dismantleUIView(_ uiView: PreviewMetalView, coordinator: Coordinator)
    {
        coordinator.detach()
    }

    final class Coordinator: NSObject
    {
        var isPaused: Bool
        {
            didSet { displayLink?.isPaused = isPaused }
        }

        private var bridge: VisionOSPreviewBridge?
        private var displayLink: CADisplayLink?
        private weak var view: PreviewMetalView?
        private var drawableWidth  = 0
        private var drawableHeight = 0

        init(isPaused: Bool)
        {
            self.isPaused = isPaused
        }

        func attach(to view: PreviewMetalView)
        {
            self.view = view
            startDisplayLink()
        }

        func detach()
        {
            stopDisplayLink()
            bridge         = nil
            view           = nil
            drawableWidth  = 0
            drawableHeight = 0
        }

        func resizeIfNeeded()
        {
            guard let view, view.window != nil else { return }

            let scale  = max(view.traitCollection.displayScale, 1.0)
            let width  = Int(max(1.0, view.bounds.width  * scale))
            let height = Int(max(1.0, view.bounds.height * scale))

            view.metalLayer.contentsScale = scale

            guard bridge == nil || width != drawableWidth || height != drawableHeight else { return }

            drawableWidth  = width
            drawableHeight = height

            if let bridge
            {
                bridge.resize(toWidth: width, height: height)
            }
            else
            {
                bridge = VisionOSPreviewBridge(metalLayer: view.metalLayer, widthPx: width, heightPx: height)
            }
        }

        private func startDisplayLink()
        {
            guard displayLink == nil else { return }

            let link = CADisplayLink(target: self, selector: #selector(renderFrame))
            link.add(to: .main, forMode: .common)
            link.isPaused = isPaused
            displayLink = link
        }

        private func stopDisplayLink()
        {
            displayLink?.invalidate()
            displayLink = nil
        }

        @objc private func renderFrame()
        {
            resizeIfNeeded()
            bridge?.renderFrame()
        }
    }
}
