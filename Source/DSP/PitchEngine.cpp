#include "PitchEngine.h"
#include <algorithm>
#include <cmath>

namespace kitsune
{
void PitchEngine::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    detector.prepare (sr, kStudioMinHz, 1000.0f);
    shifters.assign ((std::size_t) std::max (1, std::min (numChannels, 2)), PitchShifter());
    for (auto& s : shifters) s.prepare (sr, kStudioMinHz);
    for (auto& a : detectedHistory) a.store (-1.0f);
    for (auto& a : correctedHistory) a.store (-1.0f);
    setMode (0);
}

void PitchEngine::setMode (int m)
{
    mode = m == 0 ? 0 : 1;
    const float minHz = mode == 0 ? kLowLatencyMinHz : kStudioMinHz;
    detector.configure (minHz, mode == 0 ? 1.0f : 1.5f);
    for (auto& s : shifters) s.setMode (minHz);
    corrector.reset();
}

void PitchEngine::reset()
{
    setMode (mode);
}

void PitchEngine::process (float* const* ch, int numCh, int n, const EngineParams& p)
{
    numCh = std::min (numCh, (int) shifters.size());
    constexpr int kSeg = 64;
    float mono[kSeg];
    const float dt = (float) detector.hopSamples() / (float) sr;

    for (int off = 0; off < n; off += kSeg)
    {
        const int len = std::min (kSeg, n - off);
        for (int i = 0; i < len; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < numCh; ++c) s += ch[c][off + i];
            mono[i] = s / (float) numCh;
        }

        if (detector.process (mono, len))
        {
            const auto& r = detector.result();
            const float shift = corrector.update (r, p.corrector, dt);
            const float period = r.frequency > 0.0f ? (float) sr / r.frequency : 0.0f;
            for (int c = 0; c < numCh; ++c)
                shifters[(std::size_t) c].setControl (period, shift, p.formantSemitones, p.keepFormants, corrector.isVoicedHold());

            const bool v = r.voiced;
            const float det = v ? corrector.detectedMidi() : -1.0f;
            const float cor = v ? corrector.detectedMidi() + shift : -1.0f;
            const int w = historyWrite.load (std::memory_order_relaxed);
            detectedHistory[(std::size_t) (w % kHistory)].store (det, std::memory_order_relaxed);
            correctedHistory[(std::size_t) (w % kHistory)].store (cor, std::memory_order_relaxed);
            historyWrite.store ((w + 1) % (kHistory * 1024), std::memory_order_release);
            lastDetectedMidi.store (det, std::memory_order_relaxed);
            lastCorrectedMidi.store (cor, std::memory_order_relaxed);
        }

        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < len; ++i)
                ch[c][off + i] = shifters[(std::size_t) c].processSample (ch[c][off + i]);
    }
}
} // namespace kitsune
