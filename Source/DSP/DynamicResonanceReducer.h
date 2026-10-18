#pragma once
#include <array>
#include <atomic>
#include <vector>
#include "Biquad.h"

namespace kitsune
{
struct ResonanceParams
{
    bool  enabled = false;
    float sensitivity = 0.5f;  // 0..1  (higher = reacts to smaller peaks)
    float amount = 0.5f;       // 0..1  (maximum cut = 18 dB * amount)
    float attackMs = 5.0f;
    float releaseMs = 80.0f;
};

/** Adaptive harshness reducer: 10 log-spaced bands (1.5-11 kHz). A band is cut only when its level exceeds
    the average level of all bands by more than a sensitivity-dependent margin. Detection is channel-linked. */
class DynamicResonanceReducer
{
public:
    static constexpr int kBands = 10;
    static constexpr int kSub = 32;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void process (float* const* ch, int numCh, int n, const ResonanceParams& p);

    std::array<std::atomic<float>, kBands> reductionDb;   // for UI
    static float bandFrequency (int b);

private:
    void updateGains (const ResonanceParams& p);

    double sr = 48000.0;
    int channels = 2;
    std::array<Biquad, kBands> detector;
    std::array<float, kBands> env {}, gain {}, lastApplied {};
    std::vector<std::array<Biquad, kBands>> cut;
    float envA = 0, envR = 0;
    int counter = 0;
};
} // namespace kitsune
