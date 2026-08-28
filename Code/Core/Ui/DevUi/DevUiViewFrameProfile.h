#ifndef DEVUIVIEWFRAMEPROFILE_H
#define DEVUIVIEWFRAMEPROFILE_H

#include "DevUiView.h"
#include "FrameTimer.h"
#include "imgui.h"
#include <string>

namespace CC
{
    class DevUiViewFrameProfile : public DevUiView
    {
    public:
        static constexpr int PHASE_COUNT = 6;
        static constexpr int GPU_PHASE_COUNT = 7;

        void Draw() override;
        const char* GetName() const override;

        std::string GetFrameTimingSnapshot() const;

    private:
        static constexpr float TARGET_FRAME_TIME_MS = 16.667f;
        static constexpr float GRAPH_SCALE = 1.2f;

        void DrawFrameTimeGraph();
        void DrawGpuOverlayLine(ImVec2 origin, float graphWidth, float graphHeight, float scaleMax, int count, int oldest);
        void DrawPhaseBreakdownBar();
        void DrawStats();

        void DrawGpuPhaseGraph();
        void DrawGpuPhaseBreakdownBar();
        void DrawGpuPhaseStats();
    };
}

#endif // DEVUIVIEWFRAMEPROFILE_H
