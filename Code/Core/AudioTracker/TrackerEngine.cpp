#include "TrackerEngine.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "TrackerClock.h"
#include "SampleLibrary.h"
#include "TrackerCommandHistory.h"
#include "TrackerPaths.h"
#include "TrackSource.h"
#include "Pattern.h"
#include "TrackerProject.h"
#include "Synthesis/Synthesis.h"
#include "Synthesis/SynthPresets.h"
#include "AudioManager.h"
#include "PrintManager.h"
#include "CCAssert.h"
#include "miniaudio.h"

namespace CC
{
    // ========================
    // Custom miniaudio node
    // ========================

    // miniaudio allocates the node's storage in-place via ma_node_init. The
    // ma_node_base member must be the first field; ma_node APIs cast back
    // and forth between ma_node_base* and our wrapper. The engine pointer
    // gives the audio callback a path back to this object's mixing state.
    struct TrackerEngineNode
    {
        ma_node_base   base;
        TrackerEngine* engine;
    };

    static void TrackerEngineNodeOnProcess(
        ma_node*       node,
        const float**  inputFrames,
        ma_uint32*     inputFrameCount,
        float**        outputFrames,
        ma_uint32*     outputFrameCount)
    {
        (void)inputFrames;
        (void)inputFrameCount;

        TrackerEngineNode* trackerNode = (TrackerEngineNode*)node;
        if (trackerNode != nullptr && trackerNode->engine != nullptr && outputFrames != nullptr)
        {
            trackerNode->engine->OnProcessAudio(outputFrames[0], *outputFrameCount);
        }
    }

    static ma_node_vtable trackerEngineNodeVTable =
    {
        TrackerEngineNodeOnProcess,
        NULL,                                       // onGetRequiredInputFrameCount
        0,                                          // 0 input buses (source node)
        1,                                          // 1 output bus
        MA_NODE_FLAG_CONTINUOUS_PROCESSING          // process even with no input data
    };

    // ========================
    // TrackerEngine
    // ========================

    TrackerEngine* TrackerEngine::instance = nullptr;

    TrackerEngine::TrackerEngine()
        : clock(nullptr)
        , sampleLibrary(nullptr)
        , synthesis(nullptr)
        , commandHistory(nullptr)
        , engine(nullptr)
        , nodeHandle(nullptr)
        , engineSampleRate(0)
        , engineChannelCount(0)
        , activeSchedule(nullptr)
        , isPlaying(false)
    {
        CC_ASSERT(instance == nullptr, "TrackerEngine already created");
        instance = this;
    }

    TrackerEngine::~TrackerEngine()
    {
        instance = nullptr;
    }

    TrackerEngine* TrackerEngine::Get()
    {
        CC_ASSERT(instance != nullptr, "TrackerEngine not created yet");
        return instance;
    }

    void TrackerEngine::Init()
    {
        // Tracker-internal sub-singletons. AppState constructs only
        // TrackerEngine; everything else used by the tracker is
        // created here so AppState code does not need to know the
        // dependency order.
        //
        // Order matters: Synthesis must exist before
        // SynthesizePrototypeSamples below (which calls into it), and
        // SampleLibrary's LoadAndVerify is independent of audio
        // setup so it runs first to surface any disk issues at the
        // start of init rather than mid-way.
        commandHistory = new TrackerCommandHistory();
        synthesis      = new Synthesis();

        sampleLibrary = new SampleLibrary();
        sampleLibrary->LoadAndVerify();

        AudioManager* audioManager = AudioManager::Get();
        engine = audioManager->GetEngine();
        if (engine == nullptr)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerEngine::Init engine is null; AudioManager not initialised");
        }
        else
        {
            engineSampleRate   = (int)ma_engine_get_sample_rate(engine);
            engineChannelCount = (int)ma_engine_get_channels(engine);

            clock = new TrackerClock();
            clock->Configure(engineSampleRate, 90.0f, 4, 4);

            SynthesizePrototypeSamples();
            AttachNode();

            // Decode any library samples loaded by SampleLibrary so
            // audition is available immediately on first launch.
            SyncWithLibrary();
        }
    }

    void TrackerEngine::Shutdown()
    {
        // Stop the audio thread from triggering new events first, then tear
        // the node down (ma_node_uninit waits for the audio callback to
        // finish), then free the schedules the audio thread might have been
        // reading.
        isPlaying.store(false, std::memory_order_release);
        DetachNode();

        TrackerSchedule* current = activeSchedule.exchange(nullptr, std::memory_order_acq_rel);
        (void)current;      // current was already pushed into retiredSchedules on its publish

        retiredSchedules.clear();
        samples.clear();
        diskSamples.clear();

        delete clock;
        clock = nullptr;

        delete sampleLibrary;
        sampleLibrary = nullptr;

        delete synthesis;
        synthesis = nullptr;

        delete commandHistory;
        commandHistory = nullptr;

        engine = nullptr;
        engineSampleRate   = 0;
        engineChannelCount = 0;
    }

    bool TrackerEngine::IsPlaying() const
    {
        return isPlaying.load(std::memory_order_relaxed);
    }

    // ========================
    // Public playback control
    // ========================

    void TrackerEngine::StartPrototypeLoop()
    {
        if (clock == nullptr || samples.empty())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerEngine::StartPrototypeLoop called before Init");
        }
        else
        {
            std::unique_ptr<TrackerSchedule> schedule = std::unique_ptr<TrackerSchedule>(new TrackerSchedule());

            // 90 BPM, 4/4, two-bar loop.
            uint64_t framesPerBeat = clock->FramesPerBeat();
            uint64_t framesPerBar  = clock->FramesPerBar();
            schedule->loopLengthFrames = framesPerBar * 2;

            // Resolve preset pointers once. samples[] is append-only
            // after Init populates it, so these stay stable for the
            // schedule's lifetime.
            const TrackerSample* kickSample  = (samples.size() > (size_t)TrackerSampleId::Kick)  ? &samples[(size_t)TrackerSampleId::Kick]  : nullptr;
            const TrackerSample* snareSample = (samples.size() > (size_t)TrackerSampleId::Snare) ? &samples[(size_t)TrackerSampleId::Snare] : nullptr;
            const TrackerSample* hiHatSample = (samples.size() > (size_t)TrackerSampleId::HiHat) ? &samples[(size_t)TrackerSampleId::HiHat] : nullptr;

            // Kick on beats 1 and 3 of each bar (zero-indexed: beats 0, 2, 4, 6).
            for (int beat = 0; beat < 8; beat += 2)
            {
                TrackerScheduleEvent evt;
                evt.sampleFrame = (uint64_t)beat * framesPerBeat;
                evt.sample      = kickSample;
                evt.gainLinear  = 1.0f;
                schedule->events.push_back(evt);
            }

            // Snare on beats 2 and 4 of each bar (beats 1, 3, 5, 7).
            for (int beat = 1; beat < 8; beat += 2)
            {
                TrackerScheduleEvent evt;
                evt.sampleFrame = (uint64_t)beat * framesPerBeat;
                evt.sample      = snareSample;
                evt.gainLinear  = 0.85f;
                schedule->events.push_back(evt);
            }

            // Closed hi-hat on every eighth note across the two bars.
            for (int eighth = 0; eighth < 16; eighth++)
            {
                TrackerScheduleEvent evt;
                evt.sampleFrame = (uint64_t)eighth * (framesPerBeat / 2);
                evt.sample      = hiHatSample;
                evt.gainLinear  = (eighth % 2 == 0) ? 0.7f : 0.45f;        // accent on quarter notes
                schedule->events.push_back(evt);
            }

            // Sort by sampleFrame so the audio callback can scan in order.
            std::sort(schedule->events.begin(), schedule->events.end(),
                [](const TrackerScheduleEvent& a, const TrackerScheduleEvent& b)
                {
                    return a.sampleFrame < b.sampleFrame;
                });

            // Pause the audio thread, swap schedule + clock, then resume.
            // isPlaying gates everything in OnProcessAudio so the audio
            // thread observes a coherent (schedule, position) pair.
            isPlaying.store(false, std::memory_order_release);

            for (int i = 0; i < VOICE_POOL_SIZE; i++)
            {
                voicePool[i].isActive = false;
            }

            PublishSchedule(std::move(schedule));
            clock->ResetPosition();

            isPlaying.store(true, std::memory_order_release);
        }
    }

    void TrackerEngine::Stop()
    {
        isPlaying.store(false, std::memory_order_release);

        // Voices are written by the audio thread but flagging isPlaying
        // false above means OnProcessAudio takes the inactive path and
        // does not touch them. Resetting their flags here gives a clean
        // slate for the next StartPrototypeLoop.
        for (int i = 0; i < VOICE_POOL_SIZE; i++)
        {
            voicePool[i].isActive = false;
        }
    }

    // ========================
    // Library sync + audition
    // ========================

    // miniaudio decoder. Decodes an arbitrary file (WAV PCM 16/24/32/float
    // and OGG Vorbis are the formats we currently expect, all natively
    // supported by miniaudio + linked stb_vorbis) into interleaved float
    // PCM at the engine's sample rate and channel count. Static private
    // member so it can construct a TrackerSample (which is a private
    // nested type).
    bool TrackerEngine::DecodeFileToSample(const std::string& filePath, int sampleRate, int channelCount, TrackerSample& outSample)
    {
        bool succeeded = false;

        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, (ma_uint32)channelCount, (ma_uint32)sampleRate);

        ma_decoder decoder;
        if (ma_decoder_init_file(filePath.c_str(), &config, &decoder) == MA_SUCCESS)
        {
            ma_uint64 totalFrames = 0;
            ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

            if (totalFrames > 0)
            {
                outSample.pcm.assign((size_t)totalFrames * (size_t)channelCount, 0.0f);
                outSample.channelCount = channelCount;

                ma_uint64 framesRead = 0;
                ma_decoder_read_pcm_frames(&decoder, outSample.pcm.data(), totalFrames, &framesRead);
                outSample.frameCount = (uint64_t)framesRead;

                if (framesRead == totalFrames)
                {
                    succeeded = true;
                }
            }

            ma_decoder_uninit(&decoder);
        }

        return succeeded;
    }

    void TrackerEngine::SyncWithLibrary()
    {
        if (sampleLibrary != nullptr && engineSampleRate > 0)
        {
            const std::vector<SampleEntry>& entries = sampleLibrary->GetEntries();
            const std::string&              samplesPath = TrackerPaths::GetSamplesPath();

            for (size_t i = 0; i < entries.size(); i++)
            {
                const SampleEntry& entry = entries[i];

                bool isDecodable = (entry.status == SampleStatus::Ok)
                    && !entry.id.empty()
                    && (diskSamples.find(entry.id) == diskSamples.end());

                if (isDecodable)
                {
                    std::string fullPath = samplesPath + "/" + entry.fileName;
                    TrackerSample decoded;
                    if (DecodeFileToSample(fullPath, engineSampleRate, engineChannelCount, decoded))
                    {
                        diskSamples[entry.id] = std::move(decoded);
                        CCPrint(PrintManager::CHANNEL_ALWAYS,
                            "TrackerEngine: decoded '%s' (%llu frames)",
                            entry.id.c_str(), (unsigned long long)diskSamples[entry.id].frameCount);
                    }
                    else
                    {
                        CCPrint(PrintManager::CHANNEL_ALWAYS,
                            "TrackerEngine: failed to decode '%s' from %s",
                            entry.id.c_str(), fullPath.c_str());
                    }
                }
            }
        }
    }

    size_t TrackerEngine::GetDiskSamplesByteCount() const
    {
        size_t totalBytes = 0;
        for (std::unordered_map<std::string, TrackerSample>::const_iterator iter = diskSamples.begin();
             iter != diskSamples.end();
             ++iter)
        {
            totalBytes += iter->second.pcm.size() * sizeof(float);
        }
        return totalBytes;
    }

    bool TrackerEngine::PlayOneShotSample(const std::string& sampleId)
    {
        bool succeeded = false;

        std::unordered_map<std::string, TrackerSample>::iterator iter = diskSamples.find(sampleId);
        if (iter == diskSamples.end())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "TrackerEngine::PlayOneShotSample: '%s' is not decoded", sampleId.c_str());
        }
        else
        {
            int voiceIndex = AllocateVoice();
            if (voiceIndex >= 0)
            {
                Voice&               voice  = voicePool[voiceIndex];
                const TrackerSample& sample = iter->second;
                voice.pcmData       = sample.pcm.data();
                voice.pcmFrameCount = sample.frameCount;
                voice.channelCount  = sample.channelCount;
                voice.cursor        = 0;
                voice.gainLinear    = 1.0f;
                voice.isActive      = true;
                succeeded = true;

                // Voice mixing runs independently of the isPlaying
                // schedule gate (see OnProcessAudio), so audition does
                // not need to interfere with prototype-loop transport
                // state.
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerEngine::PlayOneShotSample: voice pool exhausted");
            }
        }

        return succeeded;
    }

    void TrackerEngine::PlaySong(const TrackerProject& project)
    {
        if (clock == nullptr || project.songTracks.empty() || project.songLengthBars <= 0)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerEngine::PlaySong: nothing to play");
            return;
        }

        // Pull tempo + time signature from the project so user-set BPM
        // takes effect at the start of playback. Mid-song tempo changes
        // are explicitly out of scope.
        clock->Configure(engineSampleRate, (float)project.bpm,
            project.timeSignatureNumerator, project.timeSignatureDenominator);

        uint64_t framesPerBeat = clock->FramesPerBeat();
        uint64_t framesPerBar  = framesPerBeat * (uint64_t)project.timeSignatureNumerator;
        uint64_t loopFrames    = framesPerBar * (uint64_t)project.songLengthBars;

        std::unique_ptr<TrackerSchedule> schedule = std::unique_ptr<TrackerSchedule>(new TrackerSchedule());
        schedule->loopLengthFrames = loopFrames;

        for (size_t songTrackIndex = 0; songTrackIndex < project.songTracks.size(); songTrackIndex++)
        {
            const SongTrack& songTrack = project.songTracks[songTrackIndex];
            if (songTrack.isMuted)
            {
                continue;
            }

            for (size_t barIndex = 0; barIndex < songTrack.patternIdPerBar.size(); barIndex++)
            {
                const std::string& placedId = songTrack.patternIdPerBar[barIndex];
                if (placedId.empty())
                {
                    continue;
                }

                // Locate the placed pattern. Linear scan is fine —
                // pattern counts are small. If the user removed the
                // referenced pattern, the placement is silently
                // ignored; the song view's editor will still show the
                // stale id and let the user clean up.
                const Pattern* placedPattern = nullptr;
                for (size_t p = 0; p < project.patterns.size(); p++)
                {
                    if (project.patterns[p].id == placedId)
                    {
                        placedPattern = &project.patterns[p];
                        break;
                    }
                }
                if (placedPattern == nullptr)
                {
                    continue;
                }

                // First-cut multi-bar handling: a 1-bar pattern fires
                // each bar it appears in. Multi-bar patterns (barCount
                // > 1, repeated id across consecutive song bars) need
                // a "skip if previous bar was same id" rule to avoid
                // double-firing — deferred until pattern barCount > 1
                // becomes editable.
                uint64_t framesPerStep = (uint64_t)((double)framesPerBeat * 4.0 / (double)placedPattern->stepsPerBar);
                uint64_t barOffset     = (uint64_t)barIndex * framesPerBar;

                for (size_t patternTrackIndex = 0; patternTrackIndex < placedPattern->tracks.size(); patternTrackIndex++)
                {
                    const PatternTrack&  patternTrack = placedPattern->tracks[patternTrackIndex];
                    const TrackerSample* sample       = ResolveSource(patternTrack.source);
                    if (sample == nullptr || patternTrack.isMuted)
                    {
                        continue;
                    }

                    for (size_t stepIndex = 0; stepIndex < patternTrack.velocities.size(); stepIndex++)
                    {
                        float velocity = patternTrack.velocities[stepIndex];
                        if (velocity > 0.0f)
                        {
                            TrackerScheduleEvent evt;
                            evt.sampleFrame = barOffset + (uint64_t)stepIndex * framesPerStep;
                            evt.sample      = sample;
                            evt.gainLinear  = songTrack.gainLinear * patternTrack.gainLinear * velocity;
                            schedule->events.push_back(evt);
                        }
                    }
                }
            }
        }

        std::sort(schedule->events.begin(), schedule->events.end(),
            [](const TrackerScheduleEvent& a, const TrackerScheduleEvent& b)
            {
                return a.sampleFrame < b.sampleFrame;
            });

        // Same live-swap protocol as AuditionPattern: if a song with
        // matching loop length is already playing, swap silently;
        // otherwise full restart.
        TrackerSchedule* currentSchedule = activeSchedule.load(std::memory_order_acquire);
        bool isLiveSwap = isPlaying.load(std::memory_order_acquire)
            && currentSchedule != nullptr
            && currentSchedule->loopLengthFrames == loopFrames;

        if (isLiveSwap)
        {
            PublishSchedule(std::move(schedule));
        }
        else
        {
            isPlaying.store(false, std::memory_order_release);
            for (int i = 0; i < VOICE_POOL_SIZE; i++)
            {
                voicePool[i].isActive = false;
            }
            PublishSchedule(std::move(schedule));
            clock->ResetPosition();
            isPlaying.store(true, std::memory_order_release);
        }
    }

    const TrackerSample* TrackerEngine::ResolveSource(const TrackSource& source) const
    {
        const TrackerSample* result = nullptr;
        if (source.kind == TrackSourceKind::Sample && !source.sampleId.empty())
        {
            std::unordered_map<std::string, TrackerSample>::const_iterator iter = diskSamples.find(source.sampleId);
            if (iter != diskSamples.end())
            {
                result = &iter->second;
            }
        }
        // Synth-source resolution caches a rendered TrackerSample per
        // unique parameter set; deferred to a follow-up. For now synth
        // tracks resolve to nullptr and produce no schedule events.
        return result;
    }

    void TrackerEngine::AuditionPattern(const Pattern& pattern)
    {
        if (clock == nullptr || pattern.tracks.empty() || pattern.GetTotalStepCount() <= 0)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerEngine::AuditionPattern: pattern has nothing to play");
            return;
        }

        // Sixteenth-note grid (stepsPerBar = 16 in 4/4) maps four steps
        // to a beat. framesPerStep keeps the step grid sample-accurate.
        uint64_t framesPerBeat = clock->FramesPerBeat();
        uint64_t framesPerStep = (uint64_t)((double)framesPerBeat * 4.0 / (double)pattern.stepsPerBar);
        uint64_t loopFrames    = framesPerStep * (uint64_t)pattern.GetTotalStepCount();

        std::unique_ptr<TrackerSchedule> schedule = std::unique_ptr<TrackerSchedule>(new TrackerSchedule());
        schedule->loopLengthFrames = loopFrames;

        for (size_t trackIndex = 0; trackIndex < pattern.tracks.size(); trackIndex++)
        {
            const PatternTrack&  track  = pattern.tracks[trackIndex];
            const TrackerSample* sample = ResolveSource(track.source);
            if (sample == nullptr || track.isMuted)
            {
                continue;
            }

            for (size_t stepIndex = 0; stepIndex < track.velocities.size(); stepIndex++)
            {
                float velocity = track.velocities[stepIndex];
                if (velocity > 0.0f)
                {
                    TrackerScheduleEvent evt;
                    evt.sampleFrame = (uint64_t)stepIndex * framesPerStep;
                    evt.sample      = sample;
                    evt.gainLinear  = track.gainLinear * velocity;
                    schedule->events.push_back(evt);
                }
            }
        }

        std::sort(schedule->events.begin(), schedule->events.end(),
            [](const TrackerScheduleEvent& a, const TrackerScheduleEvent& b)
            {
                return a.sampleFrame < b.sampleFrame;
            });

        // Live-edit support: if audition is already running with a
        // compatible loop length, swap the schedule without resetting
        // the clock or clearing voices. The user's current loop phase
        // is preserved, in-flight one-shots play out, and the next
        // events fired against the new schedule land at the upcoming
        // loop positions. This is the standard tracker behaviour
        // ("paint while it's playing").
        TrackerSchedule* currentSchedule = activeSchedule.load(std::memory_order_acquire);
        bool isLiveSwap = isPlaying.load(std::memory_order_acquire)
            && currentSchedule != nullptr
            && currentSchedule->loopLengthFrames == loopFrames;

        if (isLiveSwap)
        {
            PublishSchedule(std::move(schedule));
        }
        else
        {
            isPlaying.store(false, std::memory_order_release);
            for (int i = 0; i < VOICE_POOL_SIZE; i++)
            {
                voicePool[i].isActive = false;
            }
            PublishSchedule(std::move(schedule));
            clock->ResetPosition();
            isPlaying.store(true, std::memory_order_release);
        }
    }

    // ========================
    // Audio thread
    // ========================

    // Trigger any events in the schedule whose loop-position equals the
    // exact frame about to be rendered, then mix every active voice into
    // the interleaved output buffer at this frame index. Voices are
    // tracked by cursor, not absolute time, so wrapping the loop position
    // is naturally handled by re-triggering events at the wrap point.
    void TrackerEngine::OnProcessAudio(float* output, uint32_t frameCount)
    {
        if (output != nullptr && engineChannelCount > 0)
        {
            std::memset(output, 0, sizeof(float) * (size_t)frameCount * (size_t)engineChannelCount);

            bool                   active   = isPlaying.load(std::memory_order_acquire);
            const TrackerSchedule* schedule = activeSchedule.load(std::memory_order_acquire);

            // Schedule triggering is gated on (isPlaying && schedule)
            // because that path drives the prototype loop's transport.
            // Voice mixing runs unconditionally so audition one-shots
            // play even when no scheduled playback is active.
            bool     hasScheduledPlayback = (active && schedule != nullptr && clock != nullptr);
            uint64_t absStart             = 0;
            uint64_t loopLengthFrames     = 0;
            size_t   eventCount           = 0;
            if (hasScheduledPlayback)
            {
                absStart         = clock->AdvanceFrames(frameCount);
                loopLengthFrames = schedule->loopLengthFrames;
                eventCount       = schedule->events.size();
            }

            for (uint32_t i = 0; i < frameCount; i++)
            {
                if (hasScheduledPlayback)
                {
                    uint64_t absoluteFrame = absStart + (uint64_t)i;
                    uint64_t loopPosition  = (loopLengthFrames > 0) ? (absoluteFrame % loopLengthFrames) : absoluteFrame;

                    for (size_t e = 0; e < eventCount; e++)
                    {
                        const TrackerScheduleEvent& event = schedule->events[e];
                        if (event.sampleFrame == loopPosition && event.sample != nullptr)
                        {
                            int voiceIndex = AllocateVoice();
                            if (voiceIndex >= 0)
                            {
                                Voice& voice = voicePool[voiceIndex];
                                voice.pcmData       = event.sample->pcm.data();
                                voice.pcmFrameCount = event.sample->frameCount;
                                voice.channelCount  = event.sample->channelCount;
                                voice.cursor        = 0;
                                voice.gainLinear    = event.gainLinear;
                                voice.isActive      = true;
                            }
                        }
                    }
                }

                for (int voiceIndex = 0; voiceIndex < VOICE_POOL_SIZE; voiceIndex++)
                {
                    Voice& voice = voicePool[voiceIndex];
                    if (voice.isActive)
                    {
                        for (int channelIndex = 0; channelIndex < engineChannelCount; channelIndex++)
                        {
                            int sourceChannelIndex = (voice.channelCount == 1) ? 0 : channelIndex;
                            if (sourceChannelIndex >= voice.channelCount)
                            {
                                sourceChannelIndex = 0;
                            }
                            float sampleValue = voice.pcmData[voice.cursor * (uint64_t)voice.channelCount + (uint64_t)sourceChannelIndex];
                            output[(size_t)i * (size_t)engineChannelCount + (size_t)channelIndex] += sampleValue * voice.gainLinear;
                        }

                        voice.cursor++;
                        if (voice.cursor >= voice.pcmFrameCount)
                        {
                            voice.isActive = false;
                        }
                    }
                }
            }
        }
    }

    // ========================
    // Private helpers
    // ========================

    void TrackerEngine::SynthesizePrototypeSamples()
    {
        samples.clear();
        samples.resize((size_t)TrackerSampleId::Count);

        Synthesis* synthesis = Synthesis::Get();

        // The defaults of each *Params struct reproduce the Phase 1
        // prototype sound, so the audible loop after extraction is
        // identical to before.
        SynthParams kickRequest;
        kickRequest.presetId = SynthPresetId::Kick;
        synthesis->RenderPreset(kickRequest, engineSampleRate, engineChannelCount,
            samples[(size_t)TrackerSampleId::Kick].pcm);

        SynthParams snareRequest;
        snareRequest.presetId = SynthPresetId::Snare;
        synthesis->RenderPreset(snareRequest, engineSampleRate, engineChannelCount,
            samples[(size_t)TrackerSampleId::Snare].pcm);

        SynthParams hiHatRequest;
        hiHatRequest.presetId = SynthPresetId::HiHat;
        synthesis->RenderPreset(hiHatRequest, engineSampleRate, engineChannelCount,
            samples[(size_t)TrackerSampleId::HiHat].pcm);

        for (size_t i = 0; i < samples.size(); i++)
        {
            samples[i].channelCount = engineChannelCount;
            samples[i].frameCount   = (uint64_t)(samples[i].pcm.size() / (size_t)engineChannelCount);
        }
    }

    void TrackerEngine::AttachNode()
    {
        if (engine != nullptr && nodeHandle == nullptr)
        {
            ma_uint32       outputChannels[1] = { (ma_uint32)engineChannelCount };
            ma_node_config  nodeConfig        = ma_node_config_init();
            nodeConfig.vtable          = &trackerEngineNodeVTable;
            nodeConfig.pInputChannels  = NULL;
            nodeConfig.pOutputChannels = outputChannels;

            TrackerEngineNode* trackerNode = new TrackerEngineNode();
            trackerNode->engine = this;

            ma_node_graph* graph  = ma_engine_get_node_graph(engine);
            ma_result      result = ma_node_init(graph, &nodeConfig, NULL, &trackerNode->base);
            if (result == MA_SUCCESS)
            {
                ma_node_attach_output_bus(&trackerNode->base, 0, ma_engine_get_endpoint(engine), 0);
                nodeHandle = trackerNode;
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerEngine::AttachNode ma_node_init failed");
                delete trackerNode;
            }
        }
    }

    void TrackerEngine::DetachNode()
    {
        if (nodeHandle != nullptr)
        {
            TrackerEngineNode* trackerNode = (TrackerEngineNode*)nodeHandle;
            ma_node_detach_output_bus(&trackerNode->base, 0);
            ma_node_uninit(&trackerNode->base, NULL);
            delete trackerNode;
            nodeHandle = nullptr;
        }
    }

    void TrackerEngine::PublishSchedule(std::unique_ptr<TrackerSchedule> schedule)
    {
        TrackerSchedule* raw = schedule.release();
        retiredSchedules.push_back(std::unique_ptr<TrackerSchedule>(raw));

        // The audio callback may still hold the previous pointer briefly.
        // Both old and new are owned by retiredSchedules, so the swap can
        // happen without an explicit grace period.
        activeSchedule.store(raw, std::memory_order_release);
    }

    int TrackerEngine::AllocateVoice()
    {
        int result = -1;
        for (int i = 0; i < VOICE_POOL_SIZE; i++)
        {
            if (!voicePool[i].isActive)
            {
                result = i;
                break;
            }
        }
        return result;
    }
}
