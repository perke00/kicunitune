#pragma once
#include <array>
#include <atomic>
#include <vector>
#include "PitchDetector.h"
#include "PitchCorrector.h"
#include "PitchShifter.h"

namespace kitsune
{
struct EngineParams
{
    CorrectorParams corrector;
    float formantSemitones = 0.0f;
    bool  keepFormants = true;
};

/** Detector + corrector + shifter(s). JUCE-free so it can be unit-tested. */
class PitchEngine
{
public:
    static constexpr float kLowLatencyMinHz = 100.0f;
    static constexpr float kStudioMinHz = 65.0f;
    static constexpr int   kHistory = 1024;

    void prepare (double sampleRate, int numChannels);
    void setMode (int mode);                       // 0 = Low Latency, 1 = Studio. No allocation.
    int  getMode() const noexcept { return mode; }
    void reset();
    int  latencySamples() const noexcept { return shifters.empty() ? 0 : shifters[0].latencySamples(); }
    static int latencyFor (double sr, int mode) { return PitchShifter::latencyFor (sr, mode == 0 ? kLowLatencyMinHz : kStudioMinHz); }

    void process (float* const* channels, int numChannels, int numSamples, const EngineParams& p);

    // telemetry for the UI (lock-free)
    std::array<std::atomic<float>, kHistory> detectedHistory, correctedHistory;   // MIDI note or -1
    std::atomic<int>   historyWrite { 0 };
    std::atomic<float> lastDetectedMidi { -1.0f }, lastCorrectedMidi { -1.0f };

private:
    double sr = 48000.0;
    int mode = 0;
    PitchDetector detector;
    PitchCorrector corrector;
    std::vector<PitchShifter> shifters;
};
} // namespace kitsune
