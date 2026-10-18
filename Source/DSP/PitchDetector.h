#pragma once
#include <vector>
#include <cstddef>

namespace kitsune
{
struct PitchResult
{
    float frequency = 0.0f;   // Hz, 0 when unvoiced
    float confidence = 0.0f;  // 0..1
    bool  voiced = false;
};

/** Monophonic YIN pitch detector working on a decimated (~12 kHz) signal.
    No allocation after prepare(). */
class PitchDetector
{
public:
    void prepare (double sampleRate, float capacityMinHz, float maxHz);
    /** Non-allocating: sets lowest detectable pitch and analysis window (x tauMax). */
    void configure (float minHz, float windowFactor);
    void reset();

    /** Feed samples. Returns true if at least one new estimate was produced. */
    bool process (const float* in, int numSamples);

    const PitchResult& result() const noexcept { return res; }
    int hopSamples() const noexcept { return hopDec * decim; }

    float threshold = 0.15f;
    float silenceRms = 0.003f;

private:
    void estimate();

    double sr = 48000.0, srDec = 12000.0;
    int decim = 4, hopDec = 32;
    int capTauMax = 0, tauMin = 2, tauMax = 0, W = 0, N = 0;
    std::vector<float> ring, linear, cmnd;
    std::size_t ringMask = 0, wi = 0;
    int sinceLast = 0, accCount = 0;
    float accum = 0.0f;

    PitchResult res;
    float lastFreq = 0.0f, pendingFreq = 0.0f;
    int pendingCount = 0, unvoicedCount = 0;
    bool lastValid = false;
};
} // namespace kitsune
