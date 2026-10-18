#include "PitchCorrector.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace kitsune
{
namespace
{
constexpr std::uint16_t bits (std::initializer_list<int> l)
{
    std::uint16_t m = 0;
    for (int i : l) m = (std::uint16_t) (m | (1u << i));
    return m;
}
struct ScaleDef { const char* name; std::uint16_t intervals; };
const ScaleDef kScales[] = {
    { "Chromatic",        0xFFF },
    { "Major",            bits ({ 0, 2, 4, 5, 7, 9, 11 }) },
    { "Natural Minor",    bits ({ 0, 2, 3, 5, 7, 8, 10 }) },
    { "Harmonic Minor",   bits ({ 0, 2, 3, 5, 7, 8, 11 }) },
    { "Melodic Minor",    bits ({ 0, 2, 3, 5, 7, 9, 11 }) },
    { "Dorian",           bits ({ 0, 2, 3, 5, 7, 9, 10 }) },
    { "Phrygian",         bits ({ 0, 1, 3, 5, 7, 8, 10 }) },
    { "Lydian",           bits ({ 0, 2, 4, 6, 7, 9, 11 }) },
    { "Mixolydian",       bits ({ 0, 2, 4, 5, 7, 9, 10 }) },
    { "Locrian",          bits ({ 0, 1, 3, 5, 6, 8, 10 }) },
    { "Major Pentatonic", bits ({ 0, 2, 4, 7, 9 }) },
    { "Minor Pentatonic", bits ({ 0, 3, 5, 7, 10 }) },
    { "Blues",            bits ({ 0, 3, 5, 6, 7, 10 }) },
    { "Whole Tone",       bits ({ 0, 2, 4, 6, 8, 10 }) },
};

int nearestAllowed (float midi, std::uint16_t mask, int current, float hyst)
{
    if (mask == 0) return -1;
    const int base = (int) std::floor (midi);
    int best = -1; float bd = 1e9f;
    for (int k = -7; k <= 7; ++k)
    {
        const int n = base + k, pc = ((n % 12) + 12) % 12;
        if ((mask >> pc) & 1)
        {
            const float d = std::fabs (midi - (float) n);
            if (d < bd) { bd = d; best = n; }
        }
    }
    if (current >= 0 && best != current)
    {
        const int pc = ((current % 12) + 12) % 12;
        if (((mask >> pc) & 1) && std::fabs (midi - (float) current) <= bd + hyst) return current;
    }
    return best;
}
} // namespace

int PitchCorrector::numScales() { return (int) (sizeof (kScales) / sizeof (kScales[0])); }
const char* PitchCorrector::scaleName (int i) { return kScales[std::clamp (i, 0, numScales() - 1)].name; }

std::uint16_t PitchCorrector::maskForScale (int scaleIndex, int root)
{
    const auto iv = kScales[std::clamp (scaleIndex, 0, numScales() - 1)].intervals;
    std::uint16_t m = 0;
    for (int i = 0; i < 12; ++i)
        if ((iv >> i) & 1) m = (std::uint16_t) (m | (1u << ((i + root) % 12)));
    return m;
}

void PitchCorrector::reset()
{
    shift = 0; targetNote = -1; wasVoiced = false; voicedHold = false; unvoicedTime = 1; noteAge = 0;
}

float PitchCorrector::update (const PitchResult& r, const CorrectorParams& p, float dt)
{
    if (r.voiced && r.frequency > 0.0f)
    {
        unvoicedTime = 0.0f;
        voicedHold = true;
        const float midi = 69.0f + 12.0f * std::log2 (r.frequency / p.refA);
        detMidi = midi;

        if (! wasVoiced || std::fabs (midi - slowMean) > 0.8f) slowMean = midi;
        else slowMean += (1.0f - std::exp (-dt / 0.08f)) * (midi - slowMean);

        const int tgt = nearestAllowed (slowMean, p.mask, targetNote, 0.15f);
        if (tgt != targetNote) { targetNote = tgt; noteAge = 0; } else noteAge += dt;

        const float desired = tgt < 0 ? midi : (float) tgt + p.vibrato * (midi - slowMean);
        const float raw = (desired - midi) * p.strength;
        const float steady = noteAge > 0.1f ? 1.0f : 0.0f;
        const float tau = p.speedMs * 0.001f * (1.0f + 4.0f * p.humanize * steady);

        if (tau < 0.0005f) shift = raw;
        else shift += (1.0f - std::exp (-dt / tau)) * (raw - shift);
        shift = std::clamp (shift, -12.0f, 12.0f);
        wasVoiced = true;
    }
    else
    {
        unvoicedTime += dt;
        voicedHold = unvoicedTime < 0.03f;
        if (unvoicedTime > 0.03f) shift *= std::exp (-dt / 0.03f);
        if (unvoicedTime > 0.1f) { wasVoiced = false; targetNote = -1; }
    }
    return shift;
}
} // namespace kitsune
