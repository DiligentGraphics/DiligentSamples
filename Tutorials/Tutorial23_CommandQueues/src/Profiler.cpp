/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "Profiler.hpp"
#include "imgui.h"

namespace Diligent
{

static constexpr float GraphWidth  = 500.f;
static constexpr float GraphHeight = 100.f;

void Profiler::Initialize(IRenderDevice* pDevice)
{
    m_Device  = pDevice;
    m_FrameId = 0;

    QueryDesc queryDesc;
    queryDesc.Name = "Timestamp query";
    queryDesc.Type = QUERY_TYPE_TIMESTAMP;

    m_SupportsTransferQueueProfiling = pDevice->GetDeviceInfo().Features.TransferQueueTimestampQueries;

    for (auto& Frame : m_FrameHistory)
    {
        m_Device->CreateQuery(queryDesc, &Frame.Graphics1.GpuTimeQueryBegin);
        m_Device->CreateQuery(queryDesc, &Frame.Graphics1.GpuTimeQueryEnd);

        m_Device->CreateQuery(queryDesc, &Frame.Graphics2.GpuTimeQueryBegin);
        m_Device->CreateQuery(queryDesc, &Frame.Graphics2.GpuTimeQueryEnd);

        m_Device->CreateQuery(queryDesc, &Frame.Compute.GpuTimeQueryBegin);
        m_Device->CreateQuery(queryDesc, &Frame.Compute.GpuTimeQueryEnd);

        m_Device->CreateQuery(queryDesc, &Frame.Transfer.GpuTimeQueryBegin);
        m_Device->CreateQuery(queryDesc, &Frame.Transfer.GpuTimeQueryEnd);
    }
}

void Profiler::Begin(IDeviceContext* pContext, PASS_TYPE PassType)
{
    if (m_Device == nullptr)
        return;

    const auto BeginCounters = [&](auto& Pass) //
    {
        Pass.QuerySupported = (pContext != nullptr && ((pContext->GetDesc().QueueType & COMMAND_QUEUE_TYPE_PRIMARY_MASK) > COMMAND_QUEUE_TYPE_TRANSFER || m_SupportsTransferQueueProfiling));
        if (Pass.QuerySupported)
        {
            pContext->EndQuery(Pass.GpuTimeQueryBegin);
        }
        Pass.CpuTImeBegin = TimePoint::clock::now();
    };

    auto& Frame = m_FrameHistory[m_FrameId];
    switch (PassType)
    {
        case GRAPHICS_1:
            BeginCounters(Frame.Graphics1);
            break;
        case GRAPHICS_2:
            BeginCounters(Frame.Graphics2);
            break;
        case COMPUTE:
            BeginCounters(Frame.Compute);
            break;
        case TRANSFER:
            BeginCounters(Frame.Transfer);
            break;
        case FRAME:
            BeginCounters(Frame.Frame);
            break;
        default:
            UNEXPECTED("Unknown pass type");
    }
}

void Profiler::End(IDeviceContext* pContext, PASS_TYPE PassType)
{
    if (m_Device == nullptr)
        return;

    const auto EndCounters = [pContext](auto& Pass) //
    {
        if (Pass.QuerySupported)
        {
            pContext->EndQuery(Pass.GpuTimeQueryEnd);
            Pass.Queried = true;
        }
        Pass.CpuTImeEnd = TimePoint::clock::now();
    };

    auto& Frame = m_FrameHistory[m_FrameId];
    switch (PassType)
    {
        case GRAPHICS_1:
            EndCounters(Frame.Graphics1);
            break;
        case GRAPHICS_2:
            EndCounters(Frame.Graphics2);
            break;
        case COMPUTE:
            EndCounters(Frame.Compute);
            break;
        case TRANSFER:
            EndCounters(Frame.Transfer);
            break;
        case FRAME:
            EndCounters(Frame.Frame);
            break;
        default:
            UNEXPECTED("Unknown pass type");
    }
}

void Profiler::SetCpuToGpuTransferRate(Uint32 RateInMb)
{
    m_TempCpuToGpuTransferRateMb = RateInMb;
}

void Profiler::Update(double ElapsedTime)
{
    if (m_Device == nullptr)
        return;

    ++m_FrameId;

    // Read query data
    {
        auto& Frame = m_FrameHistory[m_FrameId];

        const auto ReadTime = [](IQuery* pQuery, double& Time) //
        {
            Time = 0.0;
            QueryDataTimestamp TimeData;
            if (pQuery->GetData(&TimeData, sizeof(TimeData), true))
                Time = static_cast<double>(TimeData.Counter) / static_cast<double>(TimeData.Frequency);
        };

        const auto UpdatePass = [ReadTime](PassCounters& Pass) //
        {
            if (Pass.QuerySupported && Pass.Queried)
            {
                ReadTime(Pass.GpuTimeQueryBegin, Pass.GpuTimeBegin);
                ReadTime(Pass.GpuTimeQueryEnd, Pass.GpuTimeEnd);
                VERIFY_EXPR(Pass.GpuTimeEnd >= Pass.GpuTimeBegin);
                VERIFY_EXPR(Pass.CpuTImeEnd > Pass.CpuTImeBegin);
            }
        };

        UpdatePass(Frame.Graphics1);
        UpdatePass(Frame.Graphics2);
        UpdatePass(Frame.Compute);
        UpdatePass(Frame.Transfer);
    }

    // Update UI
    m_AccumTime += ElapsedTime;
    if (m_AccumTime > UpdateInterval)
    {
        m_AccumTime = 0.0;

        auto&       Curr = m_FrameHistory[m_FrameId];
        const auto& Prev = m_FrameHistory[(m_FrameId - 1) % m_FrameHistory.size()];

        const auto CalcFrameTimes = [](const Frame& f, double& Begin, double& End) //
        {
            const auto CalcPassTimes = [&Begin, &End](const PassCounters& p) //
            {
                if (p.Queried)
                {
                    Begin = std::min(Begin, p.GpuTimeBegin);
                    End   = std::max(End, p.GpuTimeEnd);
                }
            };
            CalcPassTimes(f.Graphics1);
            CalcPassTimes(f.Graphics2);
            CalcPassTimes(f.Compute);
            CalcPassTimes(f.Transfer);
        };
        double CurrFrameBegin = 1.0e+100;
        double CurrFrameEnd   = 0.0;
        CalcFrameTimes(Curr, CurrFrameBegin, CurrFrameEnd);

        double PrevFrameBegin = 1.0e+100;
        double PrevFrameEnd   = 0.0;
        CalcFrameTimes(Prev, PrevFrameBegin, PrevFrameEnd);

        const auto  StartTime = PrevFrameBegin;
        const auto  EndTime   = CurrFrameEnd;
        const float Scale     = GraphWidth / static_cast<float>(EndTime - StartTime);
        const auto  CalcGraph = [StartTime, Scale](Graph& g, const Frame& f) //
        {
            const float MinW = 2.f;

            g.Gfx1X   = f.Graphics1.Queried ? static_cast<float>(f.Graphics1.GpuTimeBegin - StartTime) * Scale : 0.f;
            g.Gfx1W   = f.Graphics1.Queried ? std::max(MinW, static_cast<float>(f.Graphics1.GpuTimeEnd - f.Graphics1.GpuTimeBegin) * Scale) : 0.1f;
            g.Gfx2X   = f.Graphics2.Queried ? static_cast<float>(f.Graphics2.GpuTimeBegin - StartTime) * Scale : 0.f;
            g.Gfx2W   = f.Graphics2.Queried ? std::max(MinW, static_cast<float>(f.Graphics2.GpuTimeEnd - f.Graphics2.GpuTimeBegin) * Scale) : 0.1f;
            g.CompX   = f.Compute.Queried ? static_cast<float>(f.Compute.GpuTimeBegin - StartTime) * Scale : 0.f;
            g.CompW   = f.Compute.Queried ? std::max(MinW, static_cast<float>(f.Compute.GpuTimeEnd - f.Compute.GpuTimeBegin) * Scale) : 0.1f;
            g.TransfX = f.Transfer.Queried ? static_cast<float>(f.Transfer.GpuTimeBegin - StartTime) * Scale : 0.f;
            g.TransfW = f.Transfer.Queried ? std::max(MinW, static_cast<float>(f.Transfer.GpuTimeEnd - f.Transfer.GpuTimeBegin) * Scale) : 0.1f;
        };
        CalcGraph(m_Graph1, Prev);
        CalcGraph(m_Graph2, Curr);

        const auto TimeToStr = [](std::stringstream& stream, double dt) //
        {
            if (dt <= 0.0)
                stream << "-";
            else if (dt > 1.0e-1)
                stream << (dt) << " s";
            else if (dt > 1.0e-4)
                stream << (dt * 1.0e+3) << " ms"; // milliseconds
            else if (dt > 1.0e-7)
                stream << (dt * 1.0e+6) << " mus"; // microseconds
            else
                stream << (dt * 1.0e+9) << " ns"; // nanoseconds
            stream << std::endl;
        };
        const auto ByteSizeToStr = [](std::stringstream& stream, double SizeInMb) //
        {
            if (SizeInMb <= 0.0)
                stream << "-";
            else if (SizeInMb < 0.1)
                stream << (SizeInMb * 1024.0) << " Kb/s";
            else if (SizeInMb < 1024.0)
                stream << (SizeInMb) << " Mb/s";
            else
                stream << (SizeInMb / 1024.0) << " Gb/s";
            stream << std::endl;
        };

        const auto Gfx1Time   = Curr.Graphics1.GpuTimeEnd - Curr.Graphics1.GpuTimeBegin;
        const auto Gfx2Time   = Curr.Graphics2.GpuTimeEnd - Curr.Graphics2.GpuTimeBegin;
        const auto CompTime   = Curr.Compute.GpuTimeEnd - Curr.Compute.GpuTimeBegin;
        const auto TransfTime = Curr.Transfer.GpuTimeEnd - Curr.Transfer.GpuTimeBegin;

        std::stringstream values1_ss;
        values1_ss.precision(1);
        values1_ss.flags(std::ios_base::fixed);
        values1_ss << "GPU" << std::endl;
        TimeToStr(values1_ss, CurrFrameEnd - CurrFrameBegin);
        TimeToStr(values1_ss, Curr.Graphics1.GpuTimeBegin - Prev.Graphics1.GpuTimeBegin);
        TimeToStr(values1_ss, Gfx1Time + Gfx2Time);
        TimeToStr(values1_ss, CompTime);
        TimeToStr(values1_ss, TransfTime);
        ByteSizeToStr(values1_ss, m_TempCpuToGpuTransferRateMb / ElapsedTime);
        m_GpuCountersStr = values1_ss.str();

        const auto CpuGfx1Time   = std::chrono::duration_cast<SecondsD>(Curr.Graphics1.CpuTImeEnd - Curr.Graphics1.CpuTImeBegin).count();
        const auto CpuGfx2Time   = std::chrono::duration_cast<SecondsD>(Curr.Graphics2.CpuTImeEnd - Curr.Graphics2.CpuTImeBegin).count();
        const auto CpuCompTime   = std::chrono::duration_cast<SecondsD>(Curr.Compute.CpuTImeEnd - Curr.Compute.CpuTImeBegin).count();
        const auto CpuTransfTime = std::chrono::duration_cast<SecondsD>(Curr.Transfer.CpuTImeEnd - Curr.Transfer.CpuTImeBegin).count();
        const auto CpuFrameTime  = std::chrono::duration_cast<SecondsD>(Curr.Frame.CpuTImeEnd - Curr.Frame.CpuTImeBegin).count();

        std::stringstream values2_ss;
        values2_ss.precision(1);
        values2_ss.flags(std::ios_base::fixed);
        values2_ss << "CPU" << std::endl;
        TimeToStr(values2_ss, CpuFrameTime);
        values2_ss << "-" << std::endl;
        TimeToStr(values2_ss, CpuGfx1Time + CpuGfx2Time);
        TimeToStr(values2_ss, CpuCompTime);
        TimeToStr(values2_ss, CpuTransfTime);
        m_CpuCountersStr = values2_ss.str();
    }

    // Clear state
    {
        auto& Curr = m_FrameHistory[m_FrameId];

        Curr.Graphics1.Queried = false;
        Curr.Graphics2.Queried = false;
        Curr.Compute.Queried   = false;
        Curr.Transfer.Queried  = false;
        Curr.Frame.Queried     = false;

        m_TempCpuToGpuTransferRateMb = 0;
    }
}

void Profiler::UpdateUI()
{
    if (m_Device == nullptr)
        return;

    ImGui::SetNextWindowPos(ImVec2(240, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::BeginChild("##PassOverlapping", ImVec2{GraphWidth, GraphHeight}, false, ImGuiWindowFlags_None);
        {
            const float  BtnH        = GraphHeight / 4.f;
            const ImVec4 Gfx1Color   = {1.0f, 0.f, 0.f, 1.0f};
            const ImVec4 Gfx2Color   = {1.0f, 0.5f, 0.f, 1.0f};
            const ImVec4 CompColor   = {0.0f, 0.8f, 0.f, 1.0f};
            const ImVec4 TransfColor = {0.0f, 0.4f, 1.f, 1.0f};

            ImGui::NewLine();
            ImGui::PushStyleColor(ImGuiCol_Button, Gfx1Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Gfx1Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Gfx1Color);
            ImGui::SameLine(m_Graph1.Gfx1X);
            ImGui::Button("Gfx1##G1F1", ImVec2(m_Graph1.Gfx1W, BtnH));
            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_Button, Gfx2Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Gfx2Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Gfx2Color);
            ImGui::SameLine(m_Graph1.Gfx2X);
            ImGui::Button("Gfx2##G2F1", ImVec2(m_Graph1.Gfx2W, BtnH));
            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_Button, Gfx1Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Gfx1Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Gfx1Color);
            ImGui::SameLine(m_Graph2.Gfx1X);
            ImGui::Button("Gfx1##G1F2", ImVec2(m_Graph2.Gfx1W, BtnH));
            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_Button, Gfx2Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Gfx2Color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Gfx2Color);
            ImGui::SameLine(m_Graph2.Gfx2X);
            ImGui::Button("Gfx2##G2F2", ImVec2(m_Graph2.Gfx2W, BtnH));
            ImGui::PopStyleColor(3);

            ImGui::NewLine();

            ImGui::PushStyleColor(ImGuiCol_Button, CompColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CompColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, CompColor);
            ImGui::SameLine(m_Graph1.CompX);
            ImGui::Button("Compute##CF1", ImVec2(m_Graph1.CompW, BtnH));
            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_Button, CompColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CompColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, CompColor);
            ImGui::SameLine(m_Graph2.CompX);
            ImGui::Button("Compute##CF2", ImVec2(m_Graph2.CompW, BtnH));
            ImGui::PopStyleColor(3);

            ImGui::NewLine();

            ImGui::PushStyleColor(ImGuiCol_Button, TransfColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, TransfColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, TransfColor);
            ImGui::SameLine(m_Graph1.TransfX);
            ImGui::Button("Upload##TF1", ImVec2(m_Graph1.TransfW, BtnH));
            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_Button, TransfColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, TransfColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, TransfColor);
            ImGui::SameLine(m_Graph2.TransfX);
            ImGui::Button("Upload##TF2", ImVec2(m_Graph2.TransfW, BtnH));
            ImGui::PopStyleColor(3);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(1);

        {
            std::stringstream params_ss;
            params_ss << std::endl;
            params_ss << "Frame:" << std::endl;
            params_ss << "Between frames:" << std::endl;
            params_ss << "Graphics pass:" << std::endl;
            params_ss << "Compute pass:" << std::endl;
            params_ss << "Upload pass:" << std::endl;
            params_ss << "Transfer rate:" << std::endl;

            ImGui::TextDisabled("%s", params_ss.str().c_str());
            ImGui::SameLine(0.f, 20.f);
            ImGui::TextDisabled("%s", m_GpuCountersStr.c_str());
            ImGui::SameLine(0.f, 20.f);
            ImGui::TextDisabled("%s", m_CpuCountersStr.c_str());
        }
    }
    ImGui::End();
}

} // namespace Diligent
