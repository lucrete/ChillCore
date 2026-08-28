#include "FrameTimer.h"
#include <string.h>
#include "CCAssert.h"
#include "GfxRenderApi.h"
#include "PlatformFileSystem.h"
#include "PlatformWindow.h"
#include "PrintManager.h"

namespace CC
{
    FrameTimer* FrameTimer::instance = nullptr;

    FrameTimer::FrameTimer()
    {
        CC_ASSERT(instance == nullptr, "FrameTimer already created");
        instance = this;

        for (int i = 0; i < StandardTimestampCount; i++)
        {
            standardIndices[i] = -1;
            previousStandardIndices[i] = -1;
        }

        currentTime = GetCurrentTime();
        previousTime = currentTime;

        memset(profileTotalMs, 0, sizeof(profileTotalMs));
        memset(profileCpuPhases, 0, sizeof(profileCpuPhases));
        memset(profileGpuPhases, 0, sizeof(profileGpuPhases));
        memset(profileGpuDurationMs, 0, sizeof(profileGpuDurationMs));
        memset(profileGpuVsyncMs, 0, sizeof(profileGpuVsyncMs));
    }

    FrameTimer::~FrameTimer()
    {
        instance = nullptr;
    }

    FrameTimer* FrameTimer::Get()
    {
        CC_ASSERT(instance != nullptr, "FrameTimer not created yet");
        return instance;
    }

    void FrameTimer::FrameStart()
    {
        previousTime = currentTime;
        currentTime = GetCurrentTime();
        deltaTime = currentTime - previousTime;

        // Cache previous frame timestamps before reset
        memcpy(previousFrameTimestamps, timestamps, sizeof(Timestamp) * timestampCount);
        previousFrameTimestampCount = timestampCount;
        memcpy(previousStandardIndices, standardIndices, sizeof(previousStandardIndices));

        // Reset timestamps for this frame
        timestampCount = 0;

        AddTimestamp(StandardTimestamp::FrameStart, "FrameStart");

        // FPS averaging over 1-second intervals
        frameCount++;
        fpsAccumulator += deltaTime;
        if (fpsAccumulator >= 1.0f)
        {
            framesPerSecond = frameCount / fpsAccumulator;
            frameCount = 0;
            fpsAccumulator = 0.0f;
        }
    }

    float FrameTimer::DeltaTime() const
    {
        return deltaTime;
    }

    float FrameTimer::TimeSinceStartup() const
    {
        return currentTime;
    }

    float FrameTimer::GetFramesPerSecond() const
    {
        return framesPerSecond;
    }

    void FrameTimer::AddTimestamp(const char* label)
    {
        if (timestampCount >= MAX_TIMESTAMPS)
        {
            return;
        }

        timestamps[timestampCount].label = label;
        timestamps[timestampCount].time = GetCurrentTime();
        timestampCount++;
    }

    void FrameTimer::AddTimestamp(StandardTimestamp id, const char* label)
    {
        if (timestampCount >= MAX_TIMESTAMPS)
        {
            return;
        }

        standardIndices[id] = timestampCount;
        timestamps[timestampCount].label = label;
        timestamps[timestampCount].time = GetCurrentTime();
        timestampCount++;
    }

    float FrameTimer::GetDeltaTimestamp(StandardTimestamp idStart, StandardTimestamp idEnd) const
    {
        int startIndex = previousStandardIndices[idStart];
        int endIndex = previousStandardIndices[idEnd];
        if (startIndex < 0 || endIndex < 0)
        {
            return 0.0f;
        }
        return previousFrameTimestamps[endIndex].time - previousFrameTimestamps[startIndex].time;
    }

    const Timestamp* FrameTimer::GetPreviousFrameTimestamps() const
    {
        return previousFrameTimestamps;
    }

    int FrameTimer::GetPreviousFrameTimestampCount() const
    {
        return previousFrameTimestampCount;
    }

    float FrameTimer::GetCurrentTime() const
    {
        return static_cast<float>(PlatformWindow::GetTimeSeconds());
    }

    // ========================
    // Profile Data
    // ========================

    void FrameTimer::RecordProfileData()
    {
        float frameTimeMs = deltaTime * 1000.0f;

        if (frameTimeMs <= MAX_RECORDABLE_FRAME_TIME_MS)
        {
            // Phases 0..4 are bounded entirely within the previous frame's
            // data. Phase 5 (SwapBuffers) ends at the *next* frame's
            // FrameStart, which is the currentTime captured in FrameStart()
            // for this frame — handled as a special case below.
            static const StandardTimestamp mainPhaseStart[] = {
                CC::FrameStart, CC::AppMain, CC::UiScreen,
                CC::Render, CC::UiRender
            };
            static const StandardTimestamp mainPhaseEnd[] = {
                CC::AppMain, CC::UiScreen, CC::Render,
                CC::UiRender, CC::SwapBuffers
            };

            profileTotalMs[profileWriteIndex] = frameTimeMs;

            for (int i = 0; i < PROFILE_CPU_PHASES - 1; i++)
            {
                profileCpuPhases[i][profileWriteIndex] =
                    GetDeltaTimestamp(mainPhaseStart[i], mainPhaseEnd[i]) * 1000.0f;
            }

            int swapIdx = previousStandardIndices[CC::SwapBuffers];
            float swapPhaseMs = 0.0f;
            if (swapIdx >= 0)
            {
                swapPhaseMs = (currentTime - previousFrameTimestamps[swapIdx].time) * 1000.0f;
            }
            profileCpuPhases[PROFILE_CPU_PHASES - 1][profileWriteIndex] = swapPhaseMs;

            if (Gfx::RenderApi::Get()->IsGpuTimerSupported())
            {
                // The backend resolves the frame's scopes in the order they
                // were emitted by AddGpuTimestamp. The last scope is closed
                // when the ring advances after the next swap, so its duration
                // is the swap / vsync wait; everything before it is a
                // render-time phase.
                int scopeCount = Gfx::RenderApi::Get()->GetGpuScopeCount();
                int phaseCount = scopeCount - 1;
                if (phaseCount < 0)
                {
                    phaseCount = 0;
                }
                if (phaseCount > PROFILE_MAX_GPU_PHASES)
                {
                    phaseCount = PROFILE_MAX_GPU_PHASES;
                }

                profileGpuPhaseCount = phaseCount;
                profileGpuDurationMs[profileWriteIndex] = Gfx::RenderApi::Get()->GetGpuFrameDurationMs();

                for (int i = 0; i < phaseCount; i++)
                {
                    profileGpuPhases[i][profileWriteIndex] =
                        Gfx::RenderApi::Get()->GetGpuScopeDurationMsAt(i);
                }

                profileGpuVsyncMs[profileWriteIndex] = (scopeCount > 0)
                    ? Gfx::RenderApi::Get()->GetGpuScopeDurationMsAt(scopeCount - 1)
                    : 0.0f;
            }

            profileWriteIndex = (profileWriteIndex + 1) % PROFILE_HISTORY_SIZE;
            if (profileSampleCount < PROFILE_HISTORY_SIZE)
            {
                profileSampleCount++;
            }

            if (capturing)
            {
                captureRemainingSeconds -= deltaTime;
                captureFrameCount++;

                if (captureRemainingSeconds <= 0.0f || captureFrameCount >= PROFILE_HISTORY_SIZE)
                {
                    capturing = false;
                }
            }
        }
    }

    // ========================
    // Capture
    // ========================

    void FrameTimer::StartCapture(float durationSeconds)
    {
        captureFrameCount = 0;
        captureRemainingSeconds = durationSeconds;
        capturing = true;
    }

    bool FrameTimer::IsCapturing() const
    {
        return capturing;
    }

    int FrameTimer::GetCaptureFrameCount() const
    {
        return captureFrameCount;
    }

    std::string FrameTimer::WriteCaptureToFile(const char* filePath)
    {
        int count = captureFrameCount;
        if (count > profileSampleCount)
        {
            count = profileSampleCount;
        }

        bool hasGpu = Gfx::RenderApi::Get()->IsGpuTimerSupported();
        int  gpuScopeCount = hasGpu ? Gfx::RenderApi::Get()->GetGpuScopeCount() : 0;
        int  gpuPhaseCount = gpuScopeCount > 0 ? gpuScopeCount - 1 : 0;

        std::string csv;
        csv += "Frame,Total,FrameStart,Update,UiUpdate,Render,UiRender,SwapBuffers";
        if (hasGpu)
        {
            for (int phase = 0; phase < gpuPhaseCount; phase++)
            {
                csv += ",";
                csv += Gfx::RenderApi::Get()->GetGpuScopeNameAt(phase);
            }
            csv += ",GpuTotal";
            if (gpuScopeCount > 0)
            {
                csv += ",";
                csv += Gfx::RenderApi::Get()->GetGpuScopeNameAt(gpuScopeCount - 1);
            }
        }
        csv += "\n";

        char line[512];
        int oldest = (profileWriteIndex - count + PROFILE_HISTORY_SIZE) % PROFILE_HISTORY_SIZE;

        for (int frame = 0; frame < count; frame++)
        {
            int index = (oldest + frame) % PROFILE_HISTORY_SIZE;

            snprintf(line, sizeof(line), "%d,%.3f", frame, profileTotalMs[index]);
            csv += line;

            for (int phase = 0; phase < PROFILE_CPU_PHASES; phase++)
            {
                snprintf(line, sizeof(line), ",%.3f", profileCpuPhases[phase][index]);
                csv += line;
            }

            if (hasGpu)
            {
                for (int phase = 0; phase < gpuPhaseCount; phase++)
                {
                    snprintf(line, sizeof(line), ",%.3f", profileGpuPhases[phase][index]);
                    csv += line;
                }
                snprintf(line, sizeof(line), ",%.3f,%.3f",
                    profileGpuDurationMs[index], profileGpuVsyncMs[index]);
                csv += line;
            }

            csv += "\n";
        }

        if (PlatformFileSystem::Get()->WriteFileText(filePath, csv))
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "Capture complete: %d frames saved to %s", count, filePath);
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "Failed to write %s", filePath);
        }

        // Returned by value; NRVO (Named Return Value Optimization) 
        // constructs directly in caller's memory (no copy)
        return csv;
    }

    // ========================
    // Profile Accessors
    // ========================

    int FrameTimer::GetProfileWriteIndex() const
    {
        return profileWriteIndex;
    }

    int FrameTimer::GetProfileSampleCount() const
    {
        return profileSampleCount;
    }

    float FrameTimer::GetProfileTotalMs(int index) const
    {
        return profileTotalMs[index];
    }

    float FrameTimer::GetProfileCpuPhase(int phase, int index) const
    {
        return profileCpuPhases[phase][index];
    }

    float FrameTimer::GetProfileGpuPhase(int phase, int index) const
    {
        return profileGpuPhases[phase][index];
    }

    float FrameTimer::GetProfileGpuDurationMs(int index) const
    {
        return profileGpuDurationMs[index];
    }

    float FrameTimer::GetProfileGpuVsyncMs(int index) const
    {
        return profileGpuVsyncMs[index];
    }

    int FrameTimer::GetProfileGpuPhaseCount() const
    {
        return profileGpuPhaseCount;
    }
}
