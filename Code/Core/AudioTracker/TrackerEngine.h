#ifndef TRACKERENGINE_H
#define TRACKERENGINE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct ma_engine;

namespace CC
{
    class TrackerClock;
    class SampleLibrary;
    class Synthesis;
    class TrackerCommandHistory;
    struct TrackSource;
    struct Pattern;
    struct TrackerProject;

    // Identifies a sample slot in the engine's pre-decoded sample table.
    // Phase 1 hardcodes three prototype sounds; Phase 2 replaces this with
    // a SampleLibrary-driven runtime registration.
    enum class TrackerSampleId
    {
        Kick,
        Snare,
        HiHat,
        Count
    };

    // Pre-decoded interleaved float PCM at the engine's native sample
    // rate and channel count. Held by both the synth-preset table and
    // the library disk-sample map. Made a namespace-level type rather
    // than nested-private so TrackerScheduleEvent can carry a typed
    // pointer into either store.
    struct TrackerSample
    {
        std::vector<float> pcm;
        uint64_t           frameCount   = 0;
        int                channelCount = 0;
    };

    struct TrackerScheduleEvent
    {
        uint64_t             sampleFrame = 0;       // position within one loop, in sample frames
        const TrackerSample* sample      = nullptr; // resolved at schedule build time; null = event no-ops
        float                gainLinear  = 1.0f;
    };

    // Immutable, audio-thread-readable schedule. Built on the main thread,
    // published via atomic pointer swap. Audio thread never writes.
    struct TrackerSchedule
    {
        std::vector<TrackerScheduleEvent> events;                   // sorted by sampleFrame
        uint64_t                          loopLengthFrames = 0;     // 0 = one-shot, else loops
    };

    class TrackerEngine
    {
    public:
        TrackerEngine();
        virtual ~TrackerEngine();

        static TrackerEngine* Get();

        // Initialise after AudioManager. Reads the engine's sample rate
        // and channel count, synthesises the prototype sample set, and
        // attaches a custom node to the engine's endpoint so playback
        // mixes into the same output stream as UI SFX.
        void Init();
        void Shutdown();

        TrackerClock& GetClock() { return *clock; }

        // Phase 1 prototype: hardcoded three-sample loop. Builds and
        // publishes a fresh schedule, resets the clock, and starts
        // looping playback.
        void StartPrototypeLoop();
        void Stop();
        bool IsPlaying() const;

        // Builds a schedule from a Pattern's tracks + velocities at the
        // current BPM/time-signature, publishes it, and starts looping
        // playback. Sources that don't resolve (e.g. tracks with no
        // assigned sample, or assigned to a still-decoding library id)
        // produce no events; the rest play.
        void AuditionPattern(const Pattern& pattern);

        // Builds a flat schedule covering the entire song (songTracks ×
        // bars × pattern events), publishes it, and loops at the song
        // length. Configures the clock from project.bpm and time
        // signature so song playback respects user-set tempo. Same
        // live-swap semantics as AuditionPattern: if a song is already
        // playing with the same loop length, the schedule is swapped
        // without restarting transport.
        void PlaySong(const TrackerProject& project);

        // Walks SampleLibrary entries and decodes any Ok-status entries
        // whose audio is not yet cached. Idempotent — already-decoded
        // ids are skipped. Called from Init and after each successful
        // import so audition is available without restart.
        void SyncWithLibrary();

        // Audition: triggers a single playback of a previously decoded
        // disk sample through the voice pool. Returns false if the id
        // is not in the cache (typically because the sample is not
        // yet decoded, or has a non-Ok library status).
        bool PlayOneShotSample(const std::string& sampleId);

        // Total bytes occupied by decoded library samples (interleaved
        // float PCM). Excludes the synth-preset prototype samples,
        // which are a small fixed overhead. Used by the panel UI to
        // surface the memory cost of all-in-memory decoding.
        size_t GetDiskSamplesByteCount() const;

        // Audio thread entry. Called from the miniaudio node vtable. Writes
        // frameCount frames of interleaved float into output (engineChannelCount
        // samples per frame).
        void OnProcessAudio(float* output, uint32_t frameCount);

    private:
        struct Voice
        {
            const float* pcmData       = nullptr;       // interleaved float, channelCount channels
            uint64_t     pcmFrameCount = 0;
            uint64_t     cursor        = 0;
            int          channelCount  = 0;
            float        gainLinear    = 0.0f;
            bool         isActive      = false;
        };

        void SynthesizePrototypeSamples();
        void AttachNode();
        void DetachNode();
        static bool DecodeFileToSample(const std::string& filePath, int sampleRate, int channelCount, TrackerSample& outSample);

        // Maps a TrackSource to a stable PCM pointer. Returns nullptr
        // for sources that have no resolution (no assignment, missing
        // library id, synth preset not yet rendered to PCM). Schedule
        // builder uses this once at build time; audio thread sees the
        // resolved pointer.
        const TrackerSample* ResolveSource(const TrackSource& source) const;

        void PublishSchedule(std::unique_ptr<TrackerSchedule> schedule);
        int  AllocateVoice();

        static TrackerEngine* instance;
        static const int VOICE_POOL_SIZE = 32;

        TrackerClock*          clock;
        SampleLibrary*         sampleLibrary;       // owned; created in Init, destroyed in Shutdown
        Synthesis*             synthesis;           // owned; created in Init, destroyed in Shutdown
        TrackerCommandHistory* commandHistory;      // owned; created in Init, destroyed in Shutdown

        ma_engine* engine;                          // not owned (AudioManager owns)
        void*      nodeHandle;                      // owned: TrackerEngineNode (defined in .cpp)
        int        engineSampleRate;
        int        engineChannelCount;

        std::vector<TrackerSample> samples;         // indexed by TrackerSampleId (synth presets)

        // Library-loaded samples decoded once from disk. Lookups are
        // by SampleLibrary id. The map is append-only within a session
        // (only Import grows it) so PCM-buffer addresses handed to
        // active voices stay stable.
        std::unordered_map<std::string, TrackerSample> diskSamples;

        Voice voicePool[VOICE_POOL_SIZE];

        std::atomic<TrackerSchedule*> activeSchedule;
        std::atomic<bool>             isPlaying;

        // Schedules retired from the audio thread are kept alive until
        // engine shutdown so the audio callback never reads freed memory.
        // Phase 1 swaps schedules at most a handful of times so the
        // memory growth is trivial; Phase 4 will introduce a proper
        // retirement strategy when edits become high-frequency.
        std::vector<std::unique_ptr<TrackerSchedule>> retiredSchedules;
    };
}

#endif // TRACKERENGINE_H
