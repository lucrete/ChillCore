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
        float DeltaTime() const;
        float TimeSinceStartup() const;
        float GetFramesPerSecond() const;

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

        float currentTime = 0.0f;
        float previousTime = 0.0f;
        float deltaTime = 0.0f;
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
