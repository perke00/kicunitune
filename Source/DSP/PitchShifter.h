#pragma once
#include <vector>
#include <cstdint>

namespace kitsune
{
/** Pitch-synchronous overlap-add (pseudo-PSOLA) shifter, one instance per channel.
    Grains are copied at their ORIGINAL speed and re-spaced, so timing and formants are preserved.
    The wet path is aligned with a delayed dry path (same fixed latency). No allocation after prepare(). */
class PitchShifter
{
public:
    static int latencyFor (double sampleRate, float minHz);

    void prepare (double sampleRate, float capacityMinHz);   // allocates
    void setMode (float minHz);                              // no allocation, resets state
    void reset();
    int  latencySamples() const noexcept { return delay; }

    /** periodSamples: input period in samples (ignored when !voiced). */
    void setControl (float periodSamples, float shiftSemitones, float formantSemitones,
                     bool keepFormants, bool voiced);

    float processSample (float x);

private:
    void renderGrain (std::int64_t centre);
    inline float histAt (std::int64_t i) const noexcept { return i < 0 ? 0.0f : hist[(std::size_t) (i & mask)]; }

    double sr = 48000.0;
    std::vector<float> hist, ola;
    std::int64_t mask = 0, n = 0;
    double nextCm = 0.0;
    float tmax = 480.0f;
    int reach = 0, delay = 0;

    float periodTarget = 200, period = 200, shiftTarget = 0, shift = 0, formTarget = 0, form = 0;
    float voicedTarget = 0, voiced = 0;
    bool keep = true, periodInit = false, markValid = false;
    std::int64_t lastMark = 0;
    float pCoef = 0.01f, sCoef = 0.01f, vCoef = 0.01f;
};
} // namespace kitsune
