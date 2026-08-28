#ifndef TRACKERCLOCK_H
#define TRACKERCLOCK_H

#include <atomic>
#include <cstdint>

namespace CC
{
    // Sample-accurate transport clock for the audio tracker. The audio
    // thread is the sole writer of samplePosition; the main thread reads
    // it for UI playhead. Position is stored in sample frames because that
    // is the unit miniaudio guarantees to be exact; beats and bars are
    // derived.
    class TrackerClock
    {
    public:
        TrackerClock();
        virtual ~TrackerClock();

        void Configure(int sampleRate, float bpm, int timeSignatureNumerator, int timeSignatureDenominator);

        int   GetSampleRate()             const { return sampleRate; }
        float GetBpm()                    const { return bpm; }
        int   GetTimeSignatureNumerator() const { return timeSignatureNumerator; }
        int   GetTimeSignatureDenominator() const { return timeSignatureDenominator; }

        // Audio thread: advances the clock by frameCount sample frames.
        // Returns the absolute frame at the start of the chunk that is
        // about to be rendered.
        uint64_t AdvanceFrames(uint64_t frameCount);

        // Main thread: read most recent position. Eventually consistent.
        uint64_t GetSamplePosition() const;

        void ResetPosition();

        // Pure conversions, independent of samplePosition. Safe on either thread.
        double   BeatAt(uint64_t sampleFrame) const;
        uint64_t SampleFrameAtBeat(double beat) const;
        uint64_t FramesPerBeat() const;
        uint64_t FramesPerBar() const;

    private:
        std::atomic<uint64_t> samplePosition;

        int   sampleRate;
        float bpm;
        int   timeSignatureNumerator;
        int   timeSignatureDenominator;
    };
}

#endif // TRACKERCLOCK_H
