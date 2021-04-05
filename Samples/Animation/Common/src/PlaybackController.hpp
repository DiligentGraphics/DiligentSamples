/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#pragma once

#include "ozz/animation/runtime/animation.h"
#include "imgui.h"

namespace Diligent
{
// Utility class that helps with controlling animation playback time. Time is
// computed every update according to the dt given by the caller, playback speed
// and "play" state.
// Internally time is stored as a ratio in unit interval [0,1], as expected by
// ozz runtime animation jobs.
// OnGui function allows to tweak controller parameters through the application
// Gui.
class PlaybackController
{
public:
    // Constructor.
    PlaybackController();

    // Sets animation current time.
    void SetTimeRatio(float Ratio);

    // Gets animation current time.
    float TimeRatio() const { return m_TimeRatio; }

    // Gets animation time ratio of last update. Useful when the range between
    // previous and current frame needs to pe processed.
    float PreviousTimeRatio() const { return m_PreviousTimeRatio; }

    // Sets playback speed.
    void SetPlaybackSpeed(float Speed) { m_PlaybackSpeed = Speed; }

    // Gets playback speed.
    float PlaybackSpeed() const { return m_PlaybackSpeed; }

    // Sets loop modes. If true, animation time is always clamped between 0 and 1.
    void SetLoop(bool _loop) { m_bLoop = _loop; }

    // Gets loop mode.
    bool Loop() const { return m_bLoop; }

    // Updates animation time if in "play" state, according to playback speed and
    // given frame time _dt.
    // Returns true if animation has looped during update
    void Update(const ozz::animation::Animation& Animation, float Dt);

    // Resets all parameters to their default value.
    void Reset();

    // Do controller Gui.
    // Returns true if animation time has been changed.
    bool UpdateUI(const ozz::animation::Animation& Animation);

    private:
    // Current animation time ratio, in the unit interval [0,1], where 0 is the
    // beginning of the animation, 1 is the end.
    float m_TimeRatio;

    // Time ratio of the previous update.
    float m_PreviousTimeRatio;

    // Playback speed, can be negative in order to play the animation backward.
    float m_PlaybackSpeed;

    // Animation play mode state: play/pause.
    bool m_bPlay;

    // Animation loop mode.
    bool m_bLoop;
    };
} // namespace name
