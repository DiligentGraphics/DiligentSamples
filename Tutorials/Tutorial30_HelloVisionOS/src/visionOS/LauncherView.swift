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

struct LauncherView: View
{
    @Environment(ImmersiveModel.self) private var model

    @Environment(\.openImmersiveSpace)    private var openImmersiveSpace
    @Environment(\.dismissImmersiveSpace) private var dismissImmersiveSpace

    var body: some View
    {
        let toggle: () -> Void = { Task { await toggleImmersive() } }

        Group
        {
            switch model.state
            {
                case .open, .closing:
                    ImmersivePanel(action: toggle,
                                   isBusy: model.isTransitioning)
                case .closed, .opening:
                    PreviewPanel(action: toggle,
                                 isBusy: model.isTransitioning,
                                 isPreviewPaused: model.shouldPausePreview)
            }
        }
        .padding(24)
    }

    @MainActor
    private func toggleImmersive() async
    {
        switch model.state
        {
            case .opening, .closing:
                return

            case .open:
                model.state = .closing
                await dismissImmersiveSpace()
                model.state = .closed

            case .closed:
                model.state = .opening
                switch await openImmersiveSpace(id: AppSceneID.immersiveSpace)
                {
                    case .opened:
                        model.state = .open
                    case .userCancelled, .error:
                        model.state = .closed
                    @unknown default:
                        model.state = .closed
                }
        }
    }
}

private struct PreviewPanel: View
{
    let action: () -> Void
    let isBusy: Bool
    let isPreviewPaused: Bool

    var body: some View
    {
        VStack(spacing: 16)
        {
            Text("Tutorial 30 — Hello visionOS")
                .font(.title2)
                .foregroundStyle(.secondary)

            PreviewView(isPaused: isPreviewPaused)
                .frame(width: 720, height: 480)
                .clipShape(RoundedRectangle(cornerRadius: 20, style: .continuous))
                .overlay(
                    RoundedRectangle(cornerRadius: 20, style: .continuous)
                        .stroke(.white.opacity(0.15), lineWidth: 1)
                )

            Button(action: action)
            {
                Label("Enter Immersive", systemImage: "visionpro")
            }
            .buttonStyle(.borderedProminent)
            .buttonBorderShape(.capsule)
            .controlSize(.large)
            .disabled(isBusy)
        }
    }
}

private struct ImmersivePanel: View
{
    let action: () -> Void
    let isBusy: Bool

    var body: some View
    {
        VStack(spacing: 12)
        {
            Text("Immersive Space Active")
                .font(.headline)

            Button(action: action)
            {
                Label("Exit", systemImage: "xmark.circle.fill")
            }
            .buttonStyle(.borderedProminent)
            .buttonBorderShape(.capsule)
            .controlSize(.large)
            .tint(.red)
            .disabled(isBusy)
        }
        .padding(.horizontal, 12)
    }
}
