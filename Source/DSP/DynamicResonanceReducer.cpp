#include "DynamicResonanceReducer.h"
#include <algorithm>
#include <cmath>

namespace kitsune
{
float DynamicResonanceReducer::bandFrequency (int b)
{
    return 1500.0f * std::pow (11000.0f / 1500.0f, (float) b / (float) (kBands - 1));
}

void DynamicResonanceReducer::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    channels = std::max (1, std::min (numChannels, 2));
    cut.assign ((std::size_t) channels, {});
    for (int b = 0; b < kBands; ++b)
        detector[(std::size_t) b].c = biquad::bandPass (sr, bandFrequency (b), 5.0);
    reset();
}

void DynamicResonanceReducer::reset()
{
    for (auto& d : detector) d.reset();
    for (auto& c : cut) for (auto& b : c) { b.reset(); b.c = BiquadCoeffs(); }
    env.fill (0.0f); gain.fill (0.0f); lastApplied.fill (0.0f);
    for (auto& r : reductionDb) r.store (0.0f);
    counter = 0;
}

void DynamicResonanceReducer::updateGains (const ResonanceParams& p)
{
    float db[kBands], mean = 0.0f;
    for (int b = 0; b < kBands; ++b) { db[b] = 20.0f * std::log10 (env[(std::size_t) b] + 1e-9f); mean += db[b]; }
    mean /= (float) kBands;

    const float thr = 9.0f - 8.0f * p.sensitivity;
    const float maxCut = 18.0f * p.amount;
    const bool active = p.enabled && mean > -70.0f;
    const float smooth = 1.0f - std::exp (-(float) kSub / (0.005f * (float) sr));

    for (int b = 0; b < kBands; ++b)
    {
        float target = 0.0f;
        if (active) target = -std::min (std::max (0.0f, db[b] - mean - thr), maxCut);
        gain[(std::size_t) b] += smooth * (target - gain[(std::size_t) b]);
        if (std::fabs (gain[(std::size_t) b] - lastApplied[(std::size_t) b]) > 0.02f)
        {
            const auto c = biquad::peaking (sr, bandFrequency (b), 4.0, gain[(std::size_t) b]);
            for (auto& ch : cut) ch[(std::size_t) b].c = c;
            lastApplied[(std::size_t) b] = gain[(std::size_t) b];
        }
        reductionDb[(std::size_t) b].store (gain[(std::size_t) b], std::memory_order_relaxed);
    }
}

void DynamicResonanceReducer::process (float* const* ch, int numCh, int n, const ResonanceParams& p)
{
    numCh = std::min (numCh, channels);
    envA = 1.0f - std::exp (-1.0f / (std::max (0.1f, p.attackMs) * 0.001f * (float) sr));
    envR = 1.0f - std::exp (-1.0f / (std::max (1.0f, p.releaseMs) * 0.001f * (float) sr));

    if (! p.enabled)
    {
        // let existing reduction fade out smoothly
        bool any = false;
        for (float g : gain) any |= std::fabs (g) > 0.02f;
        if (! any) return;
    }

    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int c = 0; c < numCh; ++c) mono += ch[c][i];
        mono /= (float) numCh;

        for (int b = 0; b < kBands; ++b)
        {
            const float e = std::fabs (detector[(std::size_t) b].process (mono));
            float& v = env[(std::size_t) b];
            v += (e > v ? envA : envR) * (e - v);
        }
        if (++counter >= kSub) { counter = 0; updateGains (p); }

        for (int c = 0; c < numCh; ++c)
        {
            float x = ch[c][i];
            for (int b = 0; b < kBands; ++b)
                if (lastApplied[(std::size_t) b] != 0.0f || cut[(std::size_t) c][(std::size_t) b].z1 != 0.0)
                    x = cut[(std::size_t) c][(std::size_t) b].process (x);
            ch[c][i] = x;
        }
    }
}
} // namespace kitsune
