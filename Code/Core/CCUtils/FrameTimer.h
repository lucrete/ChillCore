#ifndef FRAMETIMER_H
#define FRAMETIMER_H

#include <string>

namespace CC
{
    struct Timestamp
    {
        const char* label = nullptr;
        float time = 0.0f;
    };

    enum StandardTimestamp
    {
        FrameStart,
        UpdateInput,
        UpdateStartFrame,
        AppMain,
        SceneUpdate,
        UiScreen,
        UiUpdate,
        DevUiUpdate,
        Render,
        UiRender,
        SwapBuffers,
        StandardTimestampCount
    };

    class FrameTimer
    {
    public:
        FrameTimer();
        virtual ~FrameTimer();

        static FrameTimer* Get();

        void FrameStart();

        // Hands the frame clock to something that owns frame pacing itself.
        // An XR runtime predicts when a frame will actually be displayed, and
        // animation advanced against any other clock lands at the wrong
        // moment on screen.
        //
        // Affects FrameStart only. The intra-frame profiling timestamps stay
        // on the platform clock, which is a running measure rather than a
        // single predicted instant.
        //
        // seconds must be relative to the caller's own start, not an absolute
        // runtime timestamp: a float holds too few digits for an epoch-based
        // value to resolve a frame.
        void SetExternalFrameTime(float seconds);
        void ClearExternalFrameTime();

        float DeltaTime() const;
        float DeltaTimeUnclamped() const;
        float TimeSinceStartup() const;
        float GetFramesPerSecond() const;

        // ========================
        // Simulation clock
        // ========================
        // The world's own time. It advances with real time scaled by the time
        // scale, and stands still while paused. Components that pause read
        // this clock; everything that runs through a pause reads real time,
        // so a paused component resumes from where it stopped rather than
        // jumping to where it would have been.
        float SimulationDeltaTime() const { return simulationDeltaTime; }
        float SimulationTime() const { return simulationTime; }

        void SetSimulationPaused(bool isPaused) { isSimulationPaused = isPaused; }
        bool IsSimulationPaused() const { return isSimulationPaused; }

        void SetSimulationTimeScale(float timeScale);
        float GetSimulationTimeScale() const { return simulationTimeScale; }

        // Simulation time wrapped to a fixed period for shaders. A float of
        // seconds loses precision as it grows, and fast periodic animation
        // visibly steps after a long session.
        float ShaderSimulationTime() const;

        // The frame start and frame interval on the platform clock, which is
        // the clock every profiling timestamp is taken on. Under an external
        // frame clock these differ from TimeSinceStartup and DeltaTime, and a
        // profile mixing the two subtracts one clock from another.
        float PlatformFrameStartTime() const { return platformFrameStartTime; }
        float PlatformDeltaTime() const { return platformDeltaTime; }

        void AddTimestamp(const char* label);
        void AddTimestamp(StandardTimestamp id, const char* label);
        float GetDeltaTimestamp(StandardTimestamp idStart, StandardTimestamp idEnd) const;

        const Timestamp* GetPreviousFrameTimestamps() const;
        int GetPreviousFrameTimestampCount() const;

        // Profile data
        static const int PROFILE_HISTORY_SIZE = 300;
        static const int PROFILE_CPU_PHASES = 6;
        // Headroom over the phases a frame emits. Passes group into far
        // fewer phases than there are passes, but the raw scope count is what
        // has to fit while they are being grouped.
        static const int PROFILE_MAX_GPU_PHASES = 16;
        static const int PROFILE_GPU_PHASE_NAME_LENGTH = 32;
        static constexpr float MAX_RECORDABLE_FRAME_TIME_MS = 100.0f;

        void RecordProfileData();

        void StartCapture(float durationSeconds);
        bool IsCapturing() const;
        int GetCaptureFrameCount() const;
        std::string WriteCaptureToFile(const char* filePath);

        int GetProfileWriteIndex() const;
        int GetProfileSampleCount() const;
        float GetProfileTotalMs(int index) const;
        float GetProfileCpuPhase(int phase, int index) const;
        float GetProfileGpuPhase(int phase, int index) const;
        float GetProfileGpuDurationMs(int index) const;
        float GetProfileGpuVsyncMs(int index) const;
        int GetProfileGpuPhaseCount() const;

        // Phase names come from the backend's resolved scopes, so a caller
        // displaying or recording them never writes the names down itself.
        const char* GetProfileGpuPhaseName(int phase) const;

    private:
        static FrameTimer* instance;

        static constexpr float MAX_DELTA_TIME_SECONDS = 0.1f;

        // A power of two, so the wrapped value keeps sub-millisecond
        // resolution across the whole period.
        static constexpr float SHADER_TIME_WRAP_SECONDS = 4096.0f;

        float simulationTime = 0.0f;
        float simulationDeltaTime = 0.0f;
        float simulationTimeScale = 1.0f;
        bool  isSimulationPaused = false;

        float currentTime = 0.0f;
        float previousTime = 0.0f;
        float platformFrameStartTime = 0.0f;
        float platformDeltaTime = 0.0f;
        float deltaTime = 0.0f;
        float deltaTimeClamped = 0.0f;
        float framesPerSecond = 0.0f;

        int frameCount = 0;
        float fpsAccumulator = 0.0f;

        static const int MAX_TIMESTAMPS = 64;
        Timestamp timestamps[MAX_TIMESTAMPS];
        int timestampCount = 0;

        int standardIndices[StandardTimestampCount];

        Timestamp previousFrameTimestamps[MAX_TIMESTAMPS];
        int previousFrameTimestampCount = 0;
        int previousStandardIndices[StandardTimestampCount];

        float GetCurrentTime() const;

        float externalFrameTime = 0.0f;
        bool  hasExternalFrameTime = false;

        // Profile ring buffer
        float profileTotalMs[PROFILE_HISTORY_SIZE];
        float profileCpuPhases[PROFILE_CPU_PHASES][PROFILE_HISTORY_SIZE];
        float profileGpuPhases[PROFILE_MAX_GPU_PHASES][PROFILE_HISTORY_SIZE];
        float profileGpuDurationMs[PROFILE_HISTORY_SIZE];
        float profileGpuVsyncMs[PROFILE_HISTORY_SIZE];
        int profileWriteIndex = 0;
        int profileSampleCount = 0;
        int profileGpuPhaseCount = 0;
        char profileGpuPhaseNames[PROFILE_MAX_GPU_PHASES][PROFILE_GPU_PHASE_NAME_LENGTH] = {};

        // A frame whose phase set differs from the stored one cannot be
        // compared against the history, because phase i would mean two
        // different things in the same graph. Adopting the new set discards
        // the history rather than blending them.
        bool AdoptGpuPhaseNames(const char names[][PROFILE_GPU_PHASE_NAME_LENGTH], int phaseCount);

        // Capture state
        bool capturing = false;
        float captureRemainingSeconds = 0.0f;
        int captureFrameCount = 0;
    };
}

#endif // FRAMETIMER_H
