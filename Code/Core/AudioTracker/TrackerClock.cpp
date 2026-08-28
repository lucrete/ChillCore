#include "TrackerClock.h"

#include "CCAssert.h"

namespace CC
{
    TrackerClock::TrackerClock()
        : samplePosition(0)
        , sampleRate(48000)
        , bpm(120.0f)
        , timeSignatureNumerator(4)
        , timeSignatureDenominator(4)
    {
    }

    TrackerClock::~TrackerClock()
    {
    }

    void TrackerClock::Configure(int _sampleRate, float _bpm, int numerator, int denominator)
    {
        CC_ASSERT(_sampleRate > 0,                   "TrackerClock::Configure sampleRate must be positive");
        CC_ASSERT(_bpm > 0.0f,                       "TrackerClock::Configure bpm must be positive");
        CC_ASSERT(numerator > 0 && denominator > 0,  "TrackerClock::Configure time signature must be positive");

        sampleRate                 = _sampleRate;
        bpm                        = _bpm;
        timeSignatureNumerator     = numerator;
        timeSignatureDenominator   = denominator;
    }

    uint64_t TrackerClock::AdvanceFrames(uint64_t frameCount)
    {
        uint64_t startFrame = samplePosition.fetch_add(frameCount, std::memory_order_relaxed);
        return startFrame;
    }

    uint64_t TrackerClock::GetSamplePosition() const
    {
        return samplePosition.load(std::memory_order_relaxed);
    }

    void TrackerClock::ResetPosition()
    {
        samplePosition.store(0, std::memory_order_relaxed);
    }

    double TrackerClock::BeatAt(uint64_t sampleFrame) const
    {
        double secondsPerBeat = 60.0 / (double)bpm;
        double framesPerBeat = (double)sampleRate * secondsPerBeat;
        double result = (double)sampleFrame / framesPerBeat;
        return result;
    }

    uint64_t TrackerClock::SampleFrameAtBeat(double beat) const
    {
        double secondsPerBeat = 60.0 / (double)bpm;
        double framesPerBeat = (double)sampleRate * secondsPerBeat;
        double result = beat * framesPerBeat + 0.5;     // round to nearest frame
        return (uint64_t)result;
    }

    uint64_t TrackerClock::FramesPerBeat() const
    {
        double secondsPerBeat = 60.0 / (double)bpm;
        uint64_t result = (uint64_t)((double)sampleRate * secondsPerBeat + 0.5);
        return result;
    }

    uint64_t TrackerClock::FramesPerBar() const
    {
        return FramesPerBeat() * (uint64_t)timeSignatureNumerator;
    }
}
