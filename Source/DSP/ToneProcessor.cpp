#include "ToneProcessor.h"
#include <algorithm>
#include <cmath>

namespace kitsune
{
void ToneProcessor::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    chains.assign ((std::size_t) std::max (1, std::min (numChannels, 2)), {});
    last = { -1, -1, -1, -1 };
    reset();
}

void ToneProcessor::reset()
{
    for (auto& c : chains) { c.hp.reset(); c.lo.reset(); c.mid.reset(); c.hi.reset(); }
}

void ToneProcessor::process (float* const* ch, int numCh, int n, const ToneParams& p)
{
    if (p.hpfHz != last.hpfHz || p.lowDb != last.lowDb || p.midDb != last.midDb || p.highDb != last.highDb)
    {
        const auto hp = biquad::highPass (sr, std::max (20.0f, p.hpfHz), 0.707);
        const auto lo = biquad::lowShelf (sr, 200.0, p.lowDb);
        const auto mid = biquad::peaking (sr, 1500.0, 0.7, p.midDb);
        const auto hi = biquad::highShelf (sr, 6000.0, p.highDb);
        for (auto& c : chains) { c.hp.c = hp; c.lo.c = lo; c.mid.c = mid; c.hi.c = hi; }
        last = p;
    }
    const bool hpOn = p.hpfHz > 20.5f;
    const bool eqOn = std::fabs (p.lowDb) > 0.01f || std::fabs (p.midDb) > 0.01f || std::fabs (p.highDb) > 0.01f;
    if (! hpOn && ! eqOn) return;

    numCh = std::min (numCh, (int) chains.size());
    for (int c = 0; c < numCh; ++c)
    {
        auto& k = chains[(std::size_t) c];
        for (int i = 0; i < n; ++i)
        {
            float x = ch[c][i];
            if (hpOn) x = k.hp.process (x);
            if (eqOn) x = k.hi.process (k.mid.process (k.lo.process (x)));
            ch[c][i] = x;
        }
    }
}
} // namespace kitsune
