#include "PitchDetector.h"
#include <algorithm>
#include <cmath>

namespace kitsune
{
static std::size_t nextPow2 (std::size_t v) { std::size_t p = 1; while (p < v) p <<= 1; return p; }

void PitchDetector::prepare (double sampleRate, float capacityMinHz, float maxHz)
{
    sr = sampleRate;
    decim = std::max (1, (int) std::lround (sr / 12000.0));
    srDec = sr / decim;
    capTauMax = (int) std::ceil (srDec / capacityMinHz) + 3;
    tauMin = std::max (2, (int) std::floor (srDec / maxHz));
    const std::size_t capN = (std::size_t) (capTauMax * 5 / 2 + 8);
    ring.assign (nextPow2 (capN + 64), 0.0f);
    ringMask = ring.size() - 1;
    linear.assign (capN + 8, 0.0f);
    cmnd.assign ((std::size_t) capTauMax + 4, 1.0f);
    configure (capacityMinHz, 1.0f);
}

void PitchDetector::configure (float minHz, float windowFactor)
{
    tauMax = std::min (capTauMax, (int) std::ceil (srDec / minHz) + 3);
    W = std::max (64, (int) (tauMax * windowFactor));
    W = std::min (W, (int) (tauMax * 3 / 2 + 8));
    N = W + tauMax;
    reset();
}

void PitchDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    wi = 0; sinceLast = 0; accCount = 0; accum = 0.0f;
    res = {}; lastValid = false; pendingCount = 0; unvoicedCount = 0;
}

bool PitchDetector::process (const float* in, int n)
{
    bool updated = false;
    for (int i = 0; i < n; ++i)
    {
        accum += in[i];
        if (++accCount >= decim)
        {
            ring[wi & ringMask] = accum / (float) decim;
            ++wi;
            accum = 0.0f; accCount = 0;
            if (++sinceLast >= hopDec)
            {
                sinceLast = 0;
                estimate();
                updated = true;
            }
        }
    }
    return updated;
}

void PitchDetector::estimate()
{
    auto setUnvoiced = [this] (float conf)
    {
        res.voiced = false; res.frequency = 0.0f; res.confidence = conf;
        if (++unvoicedCount > 6) { lastValid = false; pendingCount = 0; }
    };

    if (wi < (std::size_t) N) { setUnvoiced (0.0f); return; }

    for (int i = 0; i < N; ++i)
        linear[(std::size_t) i] = ring[(wi - (std::size_t) N + (std::size_t) i) & ringMask];

    double e = 0.0;
    for (int i = N - W; i < N; ++i) e += (double) linear[(std::size_t) i] * linear[(std::size_t) i];
    if (std::sqrt (e / W) < silenceRms) { setUnvoiced (0.0f); return; }

    // difference function + cumulative mean normalisation
    cmnd[0] = 1.0f;
    double run = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double d = 0.0;
        const float* a = linear.data();
        const float* b = linear.data() + tau;
        for (int j = 0; j < W; ++j) { const float t = a[j] - b[j]; d += (double) t * t; }
        run += d;
        cmnd[(std::size_t) tau] = run > 1e-12 ? (float) (d * tau / run) : 1.0f;
    }

    int tau = tauMin;
    bool found = false;
    float minVal = 1.0f;
    while (tau < tauMax)
    {
        minVal = std::min (minVal, cmnd[(std::size_t) tau]);
        if (cmnd[(std::size_t) tau] < threshold)
        {
            while (tau + 1 < tauMax && cmnd[(std::size_t) tau + 1] < cmnd[(std::size_t) tau]) ++tau;
            found = true;
            break;
        }
        ++tau;
    }
    if (! found) { setUnvoiced (1.0f - minVal); return; }

    float tauF = (float) tau;
    if (tau > 1 && tau < tauMax)
    {
        const float s0 = cmnd[(std::size_t) tau - 1], s1 = cmnd[(std::size_t) tau], s2 = cmnd[(std::size_t) tau + 1];
        const float den = s0 + s2 - 2.0f * s1;
        if (std::fabs (den) > 1e-9f) tauF += std::clamp (0.5f * (s0 - s2) / den, -1.0f, 1.0f);
    }
    float f = (float) (srDec / tauF);

    // octave / glitch guard: a large jump must be confirmed by 3 consecutive frames
    if (lastValid && std::fabs (12.0f * std::log2 (f / lastFreq)) > 6.0f)
    {
        if (pendingCount > 0 && std::fabs (12.0f * std::log2 (f / pendingFreq)) < 1.0f) ++pendingCount;
        else pendingCount = 1;
        pendingFreq = f;
        if (pendingCount < 3) f = lastFreq; else pendingCount = 0;
    }
    else pendingCount = 0;

    lastFreq = f; lastValid = true; unvoicedCount = 0;
    res.frequency = f;
    res.confidence = std::clamp (1.0f - cmnd[(std::size_t) tau], 0.0f, 1.0f);
    res.voiced = true;
}
} // namespace kitsune
