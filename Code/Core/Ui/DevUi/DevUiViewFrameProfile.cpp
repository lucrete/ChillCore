#include "DevUiViewFrameProfile.h"
#include <string.h>
#include "imgui.h"
#include "FrameTimer.h"
#include "GfxRenderApi.h"

namespace CC
{
    static const char* PHASE_NAMES[DevUiViewFrameProfile::PHASE_COUNT] =
    {
        "FrameStart",
        "Update",
        "UiUpdate",
        "Render",
        "UiRender",
        "SwapBuffers"
    };

    static const ImU32 PHASE_COLORS[DevUiViewFrameProfile::PHASE_COUNT] =
    {
        IM_COL32(180, 180, 80, 255),
        IM_COL32(80, 200, 80, 255),
        IM_COL32(80, 200, 160, 255),
        IM_COL32(80, 130, 230, 255),
        IM_COL32(180, 100, 220, 255),
        IM_COL32(150, 150, 150, 255)
    };

    static const ImVec4 PHASE_TEXT_COLORS[DevUiViewFrameProfile::PHASE_COUNT] =
    {
        ImVec4(0.7f, 0.7f, 0.3f, 1.0f),
        ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
        ImVec4(0.3f, 0.8f, 0.6f, 1.0f),
        ImVec4(0.3f, 0.5f, 0.9f, 1.0f),
        ImVec4(0.7f, 0.4f, 0.9f, 1.0f),
        ImVec4(0.6f, 0.6f, 0.6f, 1.0f)
    };

    // Phase names are owned by the backend and read through FrameTimer, so
    // only the palette lives here. It is indexed with a wrap because the
    // number of phases is not fixed — bloom adds three of its own.
    static const ImU32 GPU_PHASE_COLORS[DevUiViewFrameProfile::GPU_PHASE_COUNT] =
    {
        IM_COL32(100, 100, 100, 255),
        IM_COL32(80, 130, 230, 255),
        IM_COL32(80, 200, 180, 255),
        IM_COL32(220, 80, 60, 255),
        IM_COL32(230, 150, 60, 255),
        IM_COL32(180, 100, 220, 255),
        IM_COL32(170, 170, 170, 255)
    };

    static const ImVec4 GPU_PHASE_TEXT_COLORS[DevUiViewFrameProfile::GPU_PHASE_COUNT] =
    {
        ImVec4(0.4f, 0.4f, 0.4f, 1.0f),
        ImVec4(0.3f, 0.5f, 0.9f, 1.0f),
        ImVec4(0.3f, 0.8f, 0.7f, 1.0f),
        ImVec4(0.9f, 0.3f, 0.2f, 1.0f),
        ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
        ImVec4(0.7f, 0.4f, 0.9f, 1.0f),
        ImVec4(0.7f, 0.7f, 0.7f, 1.0f)
    };

    const char* DevUiViewFrameProfile::GetName() const
    {
        return "Frame Profile";
    }

    // ========================
    // Frame Timing Snapshot
    // ========================

    std::string DevUiViewFrameProfile::GetFrameTimingSnapshot() const
    {
        const Timestamp* timestamps = FrameTimer::Get()->GetPreviousFrameTimestamps();
        int count = FrameTimer::Get()->GetPreviousFrameTimestampCount();

        if (count < 2)
        {
            return "No frame data recorded yet.";
        }

        char line[256];
        std::string result;

        float totalMs = FrameTimer::Get()->DeltaTimeUnclamped() * 1000.0f;
        result += "=== Frame Timing Snapshot ===\n";
        snprintf(line, sizeof(line), "Total Frame Time: %.3f ms\n", totalMs);
        result += line;
        snprintf(line, sizeof(line), "FPS: %.1f\n", FrameTimer::Get()->GetFramesPerSecond());
        result += line;

        result += "\n--- CPU Timestamps ---\n";
        // Each timestamp is emitted *before* the work it labels, so the
        // duration on each row is the wall time from that timestamp to the
        // next one. The final timestamp (SwapBuffers) bounds work that ends
        // at the next frame's FrameStart — use currentTime for that.
        for (int i = 0; i < count - 1; i++)
        {
            float deltaMs = (timestamps[i + 1].time - timestamps[i].time) * 1000.0f;
            const char* label = timestamps[i].label ? timestamps[i].label : "?";
            snprintf(line, sizeof(line), "  %-22s %7.3f ms\n", label, deltaMs);
            result += line;
        }

        const char* lastLabel = timestamps[count - 1].label ? timestamps[count - 1].label : "?";
        float lastDeltaMs = (FrameTimer::Get()->TimeSinceStartup() - timestamps[count - 1].time) * 1000.0f;
        snprintf(line, sizeof(line), "  %-22s %7.3f ms\n", lastLabel, lastDeltaMs);
        result += line;

        snprintf(line, sizeof(line), "  %-22s %7.3f ms\n", "CPU Total", totalMs);
        result += line;

        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        int currentIndex = (FrameTimer::Get()->GetProfileWriteIndex() - 1 + historySize) % historySize;

        if (Gfx::RenderApi::Get()->IsGpuTimerSupported())
        {
            float gpuMs = FrameTimer::Get()->GetProfileGpuDurationMs(currentIndex);
            result += "\n--- GPU Phases ---\n";
            for (int i = 0; i < FrameTimer::Get()->GetProfileGpuPhaseCount(); i++)
            {
                float phaseMs = FrameTimer::Get()->GetProfileGpuPhase(i, currentIndex);
                snprintf(line, sizeof(line), "  %-14s %7.3f ms\n", FrameTimer::Get()->GetProfileGpuPhaseName(i), phaseMs);
                result += line;
            }
            float vsyncMs = FrameTimer::Get()->GetProfileGpuVsyncMs(currentIndex);
            snprintf(line, sizeof(line), "  %-14s %7.3f ms\n", "VSync", vsyncMs);
            result += line;
            snprintf(line, sizeof(line), "  %-14s %7.3f ms\n", "GPU Total", gpuMs);
            result += line;
        }
        else
        {
            result += "\nGPU timing: not supported\n";
        }

        result += "=============================";

        return result;
    }

    void DevUiViewFrameProfile::Draw()
    {
        ImGui::SetNextWindowPos(ImVec2(10, 80), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 560), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Frame Profile", &isVisible, ImGuiWindowFlags_NoFocusOnAppearing))
        {
            ImGui::SeparatorText("CPU (ms)");
            DrawFrameTimeGraph();
            DrawPhaseBreakdownBar();
            DrawStats();

            if (Gfx::RenderApi::Get()->IsGpuTimerSupported())
            {
                ImGui::SeparatorText("GPU (ms)");
                DrawGpuPhaseGraph();
                DrawGpuPhaseBreakdownBar();
                DrawGpuPhaseStats();
            }
        }
        ImGui::End();
    }

    // ========================
    // Graph
    // ========================

    void DevUiViewFrameProfile::DrawFrameTimeGraph()
    {
        int count = FrameTimer::Get()->GetProfileSampleCount();
        if (count < 2)
        {
            return;
        }

        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        int writeIndex = FrameTimer::Get()->GetProfileWriteIndex();        

        float graphWidth = ImGui::GetContentRegionAvail().x;
        float graphHeight = 80.0f;
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float scaleMax = TARGET_FRAME_TIME_MS * GRAPH_SCALE;
        float bottom = origin.y + graphHeight;
        int oldest = (count < historySize) ? 0 : writeIndex;

        // Background
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + graphWidth, bottom),
            IM_COL32(30, 30, 30, 255));

        // Stacked area: all phases
        for (int i = 0; i < count - 1; i++)
        {
            int idx0 = (oldest + i) % historySize;
            int idx1 = (oldest + i + 1) % historySize;

            float x0 = origin.x + (graphWidth * i) / (count - 1);
            float x1 = origin.x + (graphWidth * (i + 1)) / (count - 1);

            float cumulative0 = 0.0f;
            float cumulative1 = 0.0f;

            for (int p = 0; p < PHASE_COUNT; p++)
            {
                float phase0 = FrameTimer::Get()->GetProfileCpuPhase(p, idx0);
                float phase1 = FrameTimer::Get()->GetProfileCpuPhase(p, idx1);

                float layerBottom0 = bottom - (cumulative0 / scaleMax) * graphHeight;
                float layerBottom1 = bottom - (cumulative1 / scaleMax) * graphHeight;

                cumulative0 += phase0;
                cumulative1 += phase1;

                float layerTop0 = bottom - (cumulative0 / scaleMax) * graphHeight;
                float layerTop1 = bottom - (cumulative1 / scaleMax) * graphHeight;

                drawList->AddQuadFilled(
                    ImVec2(x0, layerBottom0), ImVec2(x1, layerBottom1),
                    ImVec2(x1, layerTop1), ImVec2(x0, layerTop0),
                    PHASE_COLORS[p]);
            }
        }

        // Target frame time line
        float targetY = bottom - (TARGET_FRAME_TIME_MS / scaleMax) * graphHeight;
        drawList->AddLine(
            ImVec2(origin.x, targetY),
            ImVec2(origin.x + graphWidth, targetY),
            IM_COL32(255, 200, 0, 180), 1.0f);

        if (Gfx::RenderApi::Get()->IsGpuTimerSupported())
        {
            DrawGpuOverlayLine(origin, graphWidth, graphHeight, scaleMax, count, oldest);
        }

        ImGui::Dummy(ImVec2(graphWidth, graphHeight));
    }

    // ========================
    // GPU Overlay Line
    // ========================

    void DevUiViewFrameProfile::DrawGpuOverlayLine(ImVec2 origin, float graphWidth, float graphHeight, float scaleMax, int count, int oldest)
    {
        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float bottom = origin.y + graphHeight;
        ImU32 gpuColor = IM_COL32(255, 100, 100, 220);

        for (int i = 0; i < count - 1; i++)
        {
            int idx0 = (oldest + i) % historySize;
            int idx1 = (oldest + i + 1) % historySize;

            float x0 = origin.x + (graphWidth * i) / (count - 1);
            float x1 = origin.x + (graphWidth * (i + 1)) / (count - 1);

            float y0 = bottom - (FrameTimer::Get()->GetProfileGpuDurationMs(idx0) / scaleMax) * graphHeight;
            float y1 = bottom - (FrameTimer::Get()->GetProfileGpuDurationMs(idx1) / scaleMax) * graphHeight;

            drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), gpuColor, 2.0f);
        }
    }

    // ========================
    // Phase Breakdown Bar
    // ========================

    void DevUiViewFrameProfile::DrawPhaseBreakdownBar()
    {
        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        int currentIndex = (FrameTimer::Get()->GetProfileWriteIndex() - 1 + historySize) % historySize;
        float totalMs = FrameTimer::Get()->GetProfileTotalMs(currentIndex);
        if (totalMs <= 0.0f)
        {
            return;
        }

        ImGui::Separator();

        float barWidth = ImGui::GetContentRegionAvail().x;
        float barHeight = 20.0f;
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        float x = cursorPos.x;
        float y = cursorPos.y;

        for (int p = 0; p < PHASE_COUNT; p++)
        {
            float phaseMs = FrameTimer::Get()->GetProfileCpuPhase(p, currentIndex);
            float fraction = phaseMs / totalMs;
            float segmentWidth = barWidth * fraction;

            drawList->AddRectFilled(
                ImVec2(x, y),
                ImVec2(x + segmentWidth, y + barHeight),
                PHASE_COLORS[p]);

            x += segmentWidth;
        }

        ImGui::Dummy(ImVec2(barWidth, barHeight));

        // Phase labels: 3 per row, 2 rows
        for (int p = 0; p < PHASE_COUNT; p++)
        {
            float phaseMs = FrameTimer::Get()->GetProfileCpuPhase(p, currentIndex);
            ImGui::TextColored(PHASE_TEXT_COLORS[p], "%s: %.2f", PHASE_NAMES[p], phaseMs);

            if (p % 3 != 2 && p < PHASE_COUNT - 1)
            {
                ImGui::SameLine();
            }
        }
    }

    // ========================
    // Stats
    // ========================

    void DevUiViewFrameProfile::DrawStats()
    {
        ImGui::Separator();

        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        int writeIndex = FrameTimer::Get()->GetProfileWriteIndex();
        int count = FrameTimer::Get()->GetProfileSampleCount();
        float currentMs = FrameTimer::Get()->GetProfileTotalMs((writeIndex - 1 + historySize) % historySize);

        float minMs = currentMs;
        float maxMs = currentMs;
        float sumMs = 0.0f;

        for (int i = 0; i < count; i++)
        {
            int index = (writeIndex - count + i + historySize) % historySize;
            float value = FrameTimer::Get()->GetProfileTotalMs(index);
            if (value < minMs)
            {
                minMs = value;
            }
            if (value > maxMs)
            {
                maxMs = value;
            }
            sumMs += value;
        }

        float avgMs = (count > 0) ? sumMs / count : 0.0f;

        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            "CPU: %.2f  |  Min: %.2f  |  Max: %.2f  |  Avg: %.2f\n",
            currentMs, minMs, maxMs, avgMs);
    }

    // ========================
    // GPU Phase Graph
    // ========================

    void DevUiViewFrameProfile::DrawGpuPhaseGraph()
    {
        int count = FrameTimer::Get()->GetProfileSampleCount();
        if (count < 2)
        {
            return;
        }

        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        int writeIndex = FrameTimer::Get()->GetProfileWriteIndex();

        float graphWidth = ImGui::GetContentRegionAvail().x;
        float graphHeight = 80.0f;
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float scaleMax = TARGET_FRAME_TIME_MS * GRAPH_SCALE;
        float bottom = origin.y + graphHeight;
        int oldest = (count < historySize) ? 0 : writeIndex;

        // Background
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + graphWidth, bottom),
            IM_COL32(30, 30, 30, 255));

        // Stacked area: all GPU phases
        for (int i = 0; i < count - 1; i++)
        {
            int idx0 = (oldest + i) % historySize;
            int idx1 = (oldest + i + 1) % historySize;

            float x0 = origin.x + (graphWidth * i) / (count - 1);
            float x1 = origin.x + (graphWidth * (i + 1)) / (count - 1);

            float cumulative0 = 0.0f;
            float cumulative1 = 0.0f;

            for (int p = 0; p < FrameTimer::Get()->GetProfileGpuPhaseCount(); p++)
            {
                float phase0 = FrameTimer::Get()->GetProfileGpuPhase(p, idx0);
                float phase1 = FrameTimer::Get()->GetProfileGpuPhase(p, idx1);

                float layerBottom0 = bottom - (cumulative0 / scaleMax) * graphHeight;
                float layerBottom1 = bottom - (cumulative1 / scaleMax) * graphHeight;

                cumulative0 += phase0;
                cumulative1 += phase1;

                float layerTop0 = bottom - (cumulative0 / scaleMax) * graphHeight;
                float layerTop1 = bottom - (cumulative1 / scaleMax) * graphHeight;

                drawList->AddQuadFilled(
                    ImVec2(x0, layerBottom0), ImVec2(x1, layerBottom1),
                    ImVec2(x1, layerTop1), ImVec2(x0, layerTop0),
                    GPU_PHASE_COLORS[p % DevUiViewFrameProfile::GPU_PHASE_COUNT]);
            }

            // VSync layer on top
            float layerBottom0 = bottom - (cumulative0 / scaleMax) * graphHeight;
            float layerBottom1 = bottom - (cumulative1 / scaleMax) * graphHeight;

            cumulative0 += FrameTimer::Get()->GetProfileGpuVsyncMs(idx0);
            cumulative1 += FrameTimer::Get()->GetProfileGpuVsyncMs(idx1);

            float layerTop0 = bottom - (cumulative0 / scaleMax) * graphHeight;
            float layerTop1 = bottom - (cumulative1 / scaleMax) * graphHeight;

            drawList->AddQuadFilled(
                ImVec2(x0, layerBottom0), ImVec2(x1, layerBottom1),
                ImVec2(x1, layerTop1), ImVec2(x0, layerTop0),
                IM_COL32(60, 60, 60, 255));
        }

        // Target frame time line
        float targetY = bottom - (TARGET_FRAME_TIME_MS / scaleMax) * graphHeight;
        drawList->AddLine(
            ImVec2(origin.x, targetY),
            ImVec2(origin.x + graphWidth, targetY),
            IM_COL32(255, 200, 0, 180), 1.0f);

        ImGui::Dummy(ImVec2(graphWidth, graphHeight));
    }

    // ========================
    // GPU Phase Breakdown Bar
    // ========================

    void DevUiViewFrameProfile::DrawGpuPhaseBreakdownBar()
    {
        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        int currentIndex = (FrameTimer::Get()->GetProfileWriteIndex() - 1 + historySize) % historySize;

        float totalGpuMs = FrameTimer::Get()->GetProfileGpuVsyncMs(currentIndex);
        for (int p = 0; p < FrameTimer::Get()->GetProfileGpuPhaseCount(); p++)
        {
            totalGpuMs += FrameTimer::Get()->GetProfileGpuPhase(p, currentIndex);
        }

        if (totalGpuMs <= 0.0f)
        {
            return;
        }

        ImGui::Separator();

        float barWidth = ImGui::GetContentRegionAvail().x;
        float barHeight = 20.0f;
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        float x = cursorPos.x;
        float y = cursorPos.y;

        for (int p = 0; p < FrameTimer::Get()->GetProfileGpuPhaseCount(); p++)
        {
            float phaseMs = FrameTimer::Get()->GetProfileGpuPhase(p, currentIndex);
            float fraction = phaseMs / totalGpuMs;
            float segmentWidth = barWidth * fraction;

            drawList->AddRectFilled(
                ImVec2(x, y),
                ImVec2(x + segmentWidth, y + barHeight),
                GPU_PHASE_COLORS[p % DevUiViewFrameProfile::GPU_PHASE_COUNT]);

            x += segmentWidth;
        }

        // VSync segment
        float vsyncFraction = FrameTimer::Get()->GetProfileGpuVsyncMs(currentIndex) / totalGpuMs;
        float vsyncWidth = barWidth * vsyncFraction;
        drawList->AddRectFilled(
            ImVec2(x, y),
            ImVec2(x + vsyncWidth, y + barHeight),
            IM_COL32(60, 60, 60, 255));

        ImGui::Dummy(ImVec2(barWidth, barHeight));

        // Phase labels: 3 per row
        for (int p = 0; p < FrameTimer::Get()->GetProfileGpuPhaseCount(); p++)
        {
            float phaseMs = FrameTimer::Get()->GetProfileGpuPhase(p, currentIndex);
            ImGui::TextColored(GPU_PHASE_TEXT_COLORS[p % DevUiViewFrameProfile::GPU_PHASE_COUNT], "%s: %.2f", FrameTimer::Get()->GetProfileGpuPhaseName(p), phaseMs);
            ImGui::SameLine();

            if (p % 3 == 2)
            {
                ImGui::NewLine();
            }
        }
        ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "VSync: %.2f",
            FrameTimer::Get()->GetProfileGpuVsyncMs(currentIndex));
    }

    // ========================
    // GPU Phase Stats
    // ========================

    void DevUiViewFrameProfile::DrawGpuPhaseStats()
    {
        ImGui::Separator();

        int historySize = FrameTimer::PROFILE_HISTORY_SIZE;
        int writeIndex = FrameTimer::Get()->GetProfileWriteIndex();
        int count = FrameTimer::Get()->GetProfileSampleCount();
        int currentIndex = (writeIndex - 1 + historySize) % historySize;
        float currentGpuMs = FrameTimer::Get()->GetProfileGpuDurationMs(currentIndex);

        float minMs = currentGpuMs;
        float maxMs = currentGpuMs;
        float sumMs = 0.0f;

        for (int i = 0; i < count; i++)
        {
            int index = (writeIndex - count + i + historySize) % historySize;
            float value = FrameTimer::Get()->GetProfileGpuDurationMs(index);
            if (value < minMs)
            {
                minMs = value;
            }
            if (value > maxMs)
            {
                maxMs = value;
            }
            sumMs += value;
        }

        float avgMs = (count > 0) ? sumMs / count : 0.0f;

        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            "GPU: %.2f  |  Min: %.2f  |  Max: %.2f  |  Avg: %.2f",
            currentGpuMs, minMs, maxMs, avgMs);
    }
}
