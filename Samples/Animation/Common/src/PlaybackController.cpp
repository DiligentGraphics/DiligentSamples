#include "PlaybackController.hpp"
#include "BasicMath.hpp"

namespace Diligent
{

PlaybackController::PlaybackController() :
    m_TimeRatio(0.f),
    m_PreviousTimeRatio(0.f),
    m_PlaybackSpeed(1.f),
    m_bPlay(true),
    m_bLoop(true)
{
}

void PlaybackController::SetTimeRatio(float Ratio)
{
    m_PreviousTimeRatio = m_TimeRatio;
    if (m_bLoop)
    {
        // Wraps in the unit interval [0:1], even for negative values (the reason
        // for using floorf).
        m_TimeRatio = Ratio - floorf(Ratio);
    }
    else
    {
        // Clamps in the unit interval [0:1].
        m_TimeRatio = clamp(Ratio, 0.f, 1.f);
    }
}

void PlaybackController::Update(const class ozz::animation::Animation& Animation, float Dt)
{
    float NewTimeRatio = m_TimeRatio;

    if (m_bPlay)
    {
        NewTimeRatio = m_TimeRatio + Dt * m_PlaybackSpeed / Animation.duration();
    }

    SetTimeRatio(NewTimeRatio);
}

void PlaybackController::Reset()
{
    m_PreviousTimeRatio = m_TimeRatio = 0.f;
    m_PlaybackSpeed                   = 1.f;
    m_bPlay                           = true;
}

bool PlaybackController::UpdateUI(const ozz::animation::Animation& Animation)
{
    bool time_changed = false;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Playback", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::Button(m_bPlay ? "Pause" : "Play"))
        {
            m_bPlay = !m_bPlay;
        }

        ImGui::Checkbox("Loop", &m_bLoop);

        char szLabel[64];

        // Uses a local copy of time_ so that set_time is used to actually apply
        // changes. Otherwise previous time would be incorrect.
        sprintf_s(szLabel, "Animation time: %.2f", m_TimeRatio * Animation.duration());
        if (ImGui::SliderFloat(szLabel, &m_TimeRatio, 0.f, 1.f))
        {
            m_bPlay      = false;
            time_changed = true;
        }
        sprintf_s(szLabel, "Playback speed: %.2f", m_PlaybackSpeed);
        ImGui::SliderFloat(szLabel, &m_PlaybackSpeed, -5.f, 5.f);

        // Allow to reset speed if it is not the default value.
        if (ImGui::Button("Reset playback speed") && m_PlaybackSpeed != 1.f)
        {
            m_PlaybackSpeed = 1.f;
        }
        
    }
    ImGui::End();

    return time_changed;
}

}


