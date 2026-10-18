#pragma once
#include <vector>
#include "Biquad.h"

namespace kitsune
{
struct ToneParams { float hpfHz = 20.0f, lowDb = 0, midDb = 0, highDb = 0; };

/** High-pass + low shelf (200 Hz) + mid bell (1.5 kHz) + high shelf (6 kHz). Coefficients update per block. */
class ToneProcessor
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void process (float* const* ch, int numCh, int n, const ToneParams& p);

private:
    struct Chain { Biquad hp, lo, mid, hi; };
    double sr = 48000.0;
    std::vector<Chain> chains;
    ToneParams last { -1, -1, -1, -1 };
};
} // namespace kitsune
