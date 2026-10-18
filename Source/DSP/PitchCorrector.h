#pragma once
#include <cstdint>
#include "PitchDetector.h"

namespace kitsune
{
struct CorrectorParams
{
    float refA = 440.0f;
    std::uint16_t mask = 0xFFF;  // allowed pitch classes, bit0 = C
    float speedMs = 25.0f;       // 0 = instant (hard tune)
    float strength = 1.0f;       // 0..1
    float humanize = 0.0f;       // 0..1, slows correction on sustained notes
    float vibrato = 0.7f;        // 0..1, how much natural vibrato is kept
};

class PitchCorrector
{
public:
    static int numScales();
    static const char* scaleName (int index);
    static std::uint16_t maskForScale (int scaleIndex, int root);

    void reset();
    /** Called once per detector hop. Returns the pitch shift in semitones. */
    float update (const PitchResult& r, const CorrectorParams& p, float dtSeconds);

    bool  isVoicedHold() const noexcept { return voicedHold; }
    float detectedMidi() const noexcept { return detMidi; }
    float targetMidi()   const noexcept { return targetNote < 0 ? detMidi : (float) targetNote; }
    float shiftSemitones() const noexcept { return shift; }

private:
    float shift = 0, slowMean = 60, detMidi = 60, noteAge = 0, unvoicedTime = 1;
    int targetNote = -1;
    bool wasVoiced = false, voicedHold = false;
};
} // namespace kitsune
