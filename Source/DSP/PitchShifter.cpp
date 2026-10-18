#include "PitchShifter.h"
#include <algorithm>
#include <cmath>

namespace kitsune
{
static std::int64_t nextPow2 (std::int64_t v) { std::int64_t p = 1; while (p < v) p <<= 1; return p; }

int PitchShifter::latencyFor (double sampleRate, float minHz)
{
    const int tm = (int) std::ceil (sampleRate / minHz);
    const int r = (int) std::ceil (1.35 * tm) + 2;
    const int h = (int) std::ceil (1.25 * tm) + 2;
    return r + h + 2;
}

void PitchShifter::prepare (double sampleRate, float capacityMinHz)
{
    sr = sampleRate;
    const std::int64_t size = nextPow2 ((std::int64_t) (8.0 * sr / capacityMinHz) + 256);
    hist.assign ((std::size_t) size, 0.0f);
    ola.assign ((std::size_t) size, 0.0f);
    mask = size - 1;
    pCoef = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    sCoef = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
    vCoef = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));
    setMode (capacityMinHz);
}

void PitchShifter::setMode (float minHz)
{
    tmax = (float) std::ceil (sr / minHz);
    reach = (int) std::ceil (1.35 * tmax) + 2;
    delay = latencyFor (sr, minHz);
    reset();
}

void PitchShifter::reset()
{
    std::fill (hist.begin(), hist.end(), 0.0f);
    std::fill (ola.begin(), ola.end(), 0.0f);
    n = 0; nextCm = 0.0;
    shift = shiftTarget = form = formTarget = 0.0f;
    voiced = voicedTarget = 0.0f;
    periodInit = false; markValid = false; lastMark = 0;
}

void PitchShifter::setControl (float periodSamples, float shiftSemis, float formantSemis, bool keepFormants, bool isVoiced)
{
    if (isVoiced && periodSamples > 1.0f)
    {
        periodTarget = std::clamp (periodSamples, 8.0f, tmax);
        if (! periodInit) { period = periodTarget; periodInit = true; }
    }
    shiftTarget = std::clamp (shiftSemis, -12.0f, 12.0f);
    formTarget = std::clamp (formantSemis, -12.0f, 12.0f);
    keep = keepFormants;
    voicedTarget = isVoiced ? 1.0f : 0.0f;
}

void PitchShifter::renderGrain (std::int64_t cm)
{
    const float T = std::clamp (period, 8.0f, tmax);
    const float ratio = std::exp2 (std::clamp (shift, -12.0f, 12.0f) / 12.0f);
    const float Tout = T / ratio;
    const float H = std::min (std::max (T, Tout), 1.25f * T);
    const int Hi = (int) std::ceil (H);

    float fs = (keep ? 1.0f : ratio) * std::exp2 (form / 12.0f);
    fs = std::clamp (fs, 0.5f, 2.0f);

    // --- pitch-mark tracking: marks are consecutive positive waveform peaks ~T apart ---
    auto peakIn = [this] (std::int64_t a, std::int64_t b)
    {
        std::int64_t best = a; float bv = -1e30f;
        for (std::int64_t i = a; i <= b; ++i) { const float v = histAt (i); if (v > bv) { bv = v; best = i; } }
        return best;
    };
    const std::int64_t newest = n - 1;
    if (! markValid || cm - lastMark > (std::int64_t) (3.0f * T) || lastMark > cm + (std::int64_t) T)
    {
        lastMark = peakIn (cm - (std::int64_t) (0.5f * T), std::min (newest, cm + (std::int64_t) (0.5f * T)));
        markValid = true;
    }
    for (int guard = 0; guard < 8; ++guard)
    {
        const std::int64_t a = lastMark + (std::int64_t) (0.7f * T), b = lastMark + (std::int64_t) (1.3f * T);
        if (b > newest) break;
        const std::int64_t p = peakIn (a, b);
        if (p > cm) break;
        lastMark = p;
    }
    const std::int64_t ca = lastMark;   // latest pitch mark at or before the grain position
    fs = std::min (fs, ((float) reach - (float) std::max<std::int64_t> (0, ca - cm) - 2.0f) / H);

    const float g = Tout / H;
    for (int k = -Hi; k <= Hi; ++k)
    {
        if ((float) std::abs (k) > H) continue;
        const float w = 0.5f * (1.0f + std::cos (3.14159265f * (float) k / H));
        const double pos = (double) ca + (double) k * fs;
        const std::int64_t i0 = (std::int64_t) std::floor (pos);
        const float fr = (float) (pos - (double) i0);
        const float s = histAt (i0) * (1.0f - fr) + histAt (i0 + 1) * fr;
        ola[(std::size_t) ((cm + k) & mask)] += g * w * s;
    }
}

float PitchShifter::processSample (float x)
{
    hist[(std::size_t) (n & mask)] = x;
    ++n;

    period += pCoef * (periodTarget - period);
    shift  += sCoef * (shiftTarget - shift);
    form   += sCoef * (formTarget - form);
    voiced += vCoef * (voicedTarget - voiced);

    const double newest = (double) (n - 1);
    while (nextCm + reach <= newest)
    {
        renderGrain ((std::int64_t) std::llround (nextCm));
        const float ratio = std::exp2 (std::clamp (shift, -12.0f, 12.0f) / 12.0f);
        nextCm += (double) (std::max (period, 8.0f) / ratio);
    }

    const std::int64_t j = n - 1 - delay;
    const std::size_t idx = (std::size_t) (j & mask);
    const float wet = ola[idx];
    ola[idx] = 0.0f;
    const float dry = histAt (j);

    float w = std::clamp ((std::fabs (shift) - 0.02f) / 0.10f, 0.0f, 1.0f);
    if (std::fabs (form) > 0.05f) w = 1.0f;
    w *= voiced;
    return w * wet + (1.0f - w) * dry;
}
} // namespace kitsune
