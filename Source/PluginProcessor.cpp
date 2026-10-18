#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace kitsune;

juce::AudioProcessorValueTreeState::ParameterLayout KitsuneProcessor::createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using FA = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const char* s) { return ParameterID { s, 1 }; };

    StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    StringArray scales;
    for (int i = 0; i < PitchCorrector::numScales(); ++i) scales.add (PitchCorrector::scaleName (i));

    l.add (std::make_unique<AudioParameterChoice> (id ("key"), "Key", keys, 0));
    l.add (std::make_unique<AudioParameterChoice> (id ("scale"), "Scale", scales, 1));
    l.add (std::make_unique<AudioParameterChoice> (id ("mode"), "Mode", StringArray { "Low Latency", "Studio Quality" }, 0));
    l.add (std::make_unique<F> (id ("speed"), "Retune Speed", NormalisableRange<float> (0.0f, 400.0f, 0.1f, 0.5f), 25.0f, FA().withLabel ("ms")));
    l.add (std::make_unique<F> (id ("strength"), "Strength", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, FA().withLabel ("%")));
    l.add (std::make_unique<F> (id ("humanize"), "Humanize", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, FA().withLabel ("%")));
    l.add (std::make_unique<F> (id ("vibrato"), "Vibrato Keep", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, FA().withLabel ("%")));
    l.add (std::make_unique<F> (id ("formantShift"), "Formant Shift", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, FA().withLabel ("st")));
    l.add (std::make_unique<AudioParameterBool> (id ("formantKeep"), "Formant Preserve", true));
    l.add (std::make_unique<F> (id ("refA"), "Reference A4", NormalisableRange<float> (420.0f, 460.0f, 0.1f), 440.0f, FA().withLabel ("Hz")));
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "note" + String (i), 1 }, "Note " + keys[i], true));

    l.add (std::make_unique<F> (id ("inGain"), "Input Gain", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, FA().withLabel ("dB")));
    l.add (std::make_unique<F> (id ("outGain"), "Output Gain", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, FA().withLabel ("dB")));
    l.add (std::make_unique<F> (id ("mix"), "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, FA().withLabel ("%")));

    l.add (std::make_unique<F> (id ("hpf"), "High-Pass", NormalisableRange<float> (20.0f, 400.0f, 1.0f, 0.5f), 20.0f, FA().withLabel ("Hz")));
    l.add (std::make_unique<F> (id ("toneLow"), "Tone Low", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, FA().withLabel ("dB")));
    l.add (std::make_unique<F> (id ("toneMid"), "Tone Mid", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, FA().withLabel ("dB")));
    l.add (std::make_unique<F> (id ("toneHigh"), "Tone High", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, FA().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterBool> (id ("harshOn"), "Harsh Reduction", false));
    l.add (std::make_unique<F> (id ("harshSens"), "Harsh Sensitivity", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, FA().withLabel ("%")));
    l.add (std::make_unique<F> (id ("harshAmount"), "Harsh Reduction Amount", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, FA().withLabel ("%")));
    l.add (std::make_unique<F> (id ("harshAttack"), "Harsh Attack", NormalisableRange<float> (0.5f, 50.0f, 0.1f, 0.5f), 5.0f, FA().withLabel ("ms")));
    l.add (std::make_unique<F> (id ("harshRelease"), "Harsh Release", NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.5f), 80.0f, FA().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterBool> (id ("bypass"), "Bypass", false));
    return l;
}

KitsuneProcessor::KitsuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "KITSUNE_STATE", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    apvts.addParameterListener ("mode", this);
}

KitsuneProcessor::~KitsuneProcessor() { apvts.removeParameterListener ("mode", this); cancelPendingUpdate(); }

bool KitsuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && l.getMainInputChannelSet() == o;
}

void KitsuneProcessor::parameterChanged (const juce::String&, float v)
{
    requestedMode.store ((int) v);
    triggerAsyncUpdate();
}

void KitsuneProcessor::handleAsyncUpdate()
{
    setLatencySamples (PitchEngine::latencyFor (sampleRate, requestedMode.load()));
}

void KitsuneProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    const int ch = juce::jmin (2, getTotalNumInputChannels());
    engine.prepare (sr, ch);
    tone.prepare (sr, ch);
    resonance.prepare (sr, ch);

    activeMode = (int) param ("mode");
    requestedMode.store (activeMode);
    engine.setMode (activeMode);
    setLatencySamples (PitchEngine::latencyFor (sr, activeMode));

    const int maxLat = PitchEngine::latencyFor (sr, 1) + 64;
    int size = 1; while (size < maxLat) size <<= 1;
    dryRing.assign ((size_t) ch, std::vector<float> ((size_t) size, 0.0f));
    dryMask = size - 1; dryWrite = 0;
    for (auto& s : dryScratch) s.assign (16384, 0.0f);   // grown on demand in processBlock only if host block is larger (see below)

    inGain.reset (sr, 0.02); outGain.reset (sr, 0.02); mixSmooth.reset (sr, 0.02);
    inGain.setCurrentAndSetTargetValue (1.0f); outGain.setCurrentAndSetTargetValue (1.0f); mixSmooth.setCurrentAndSetTargetValue (1.0f);
    wasBypassed = false;
}

void KitsuneProcessor::resetDsp()
{
    engine.reset(); tone.reset(); resonance.reset();
    for (auto& r : dryRing) std::fill (r.begin(), r.end(), 0.0f);
}

std::uint16_t KitsuneProcessor::currentNoteMask() const
{
    std::uint16_t m = PitchCorrector::maskForScale ((int) param ("scale"), (int) param ("key"));
    std::uint16_t en = 0;
    for (int i = 0; i < 12; ++i)
        if (param (("note" + juce::String (i)).toRawUTF8()) > 0.5f) en = (std::uint16_t) (en | (1u << i));
    return (std::uint16_t) (m & en);
}

float KitsuneProcessor::getLatencyMs() const { return (float) (1000.0 * getLatencySamples() / sampleRate); }

void KitsuneProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int nCh = juce::jmin ((int) dryRing.size(), buffer.getNumChannels()), n = buffer.getNumSamples();
    int w = dryWrite;
    const int lat = getLatencySamples();
    for (int c = 0; c < nCh; ++c)
    {
        auto* d = buffer.getWritePointer (c); auto& r = dryRing[(size_t) c]; w = dryWrite;
        for (int i = 0; i < n; ++i) { r[(size_t) (w & dryMask)] = d[i]; d[i] = r[(size_t) ((w - lat) & dryMask)]; ++w; }
    }
    dryWrite = w;
    wasBypassed = true;
}

void KitsuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nCh = juce::jmin ((int) dryRing.size(), buffer.getNumChannels());
    if (n == 0 || nCh == 0) return;

    if (wasBypassed) { resetDsp(); wasBypassed = false; }

    const int reqMode = requestedMode.load();
    if (reqMode != activeMode) { activeMode = reqMode; engine.setMode (activeMode); for (auto& r : dryRing) std::fill (r.begin(), r.end(), 0.0f); }

    // ---- parameters ----
    EngineParams ep;
    ep.corrector.refA = param ("refA");
    ep.corrector.mask = currentNoteMask();
    ep.corrector.speedMs = param ("speed");
    ep.corrector.strength = param ("strength") * 0.01f;
    ep.corrector.humanize = param ("humanize") * 0.01f;
    ep.corrector.vibrato = param ("vibrato") * 0.01f;
    ep.formantSemitones = param ("formantShift");
    ep.keepFormants = param ("formantKeep") > 0.5f;

    ToneParams tp { param ("hpf"), param ("toneLow"), param ("toneMid"), param ("toneHigh") };
    ResonanceParams rp;
    rp.enabled = param ("harshOn") > 0.5f; rp.sensitivity = param ("harshSens") * 0.01f; rp.amount = param ("harshAmount") * 0.01f;
    rp.attackMs = param ("harshAttack"); rp.releaseMs = param ("harshRelease");

    inGain.setTargetValue (juce::Decibels::decibelsToGain (param ("inGain")));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (param ("outGain")));
    mixSmooth.setTargetValue (param ("mix") * 0.01f);

    // ---- input gain + meters ----
    float* chans[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        for (int c = 0; c < nCh; ++c) { chans[c][i] *= g; pk = juce::jmax (pk, std::abs (chans[c][i])); }
    }
    inPeak.store (pk);

    // ---- delayed dry path (processed in chunks so no allocation depends on the host block size) ----
    const int lat = getLatencySamples();
    constexpr int kChunk = 4096;
    float* dryPtr[2] = { dryScratch[0].data(), dryScratch[1].data() };
    jassert ((int) dryScratch[0].size() >= kChunk);

    for (int off = 0; off < n; off += kChunk)
    {
        const int len = juce::jmin (kChunk, n - off);
        float* seg[2] = { chans[0] + off, nCh > 1 ? chans[1] + off : nullptr };
        int w = dryWrite;
        for (int c = 0; c < nCh; ++c)
        {
            auto& r = dryRing[(size_t) c]; w = dryWrite;
            for (int i = 0; i < len; ++i) { r[(size_t) (w & dryMask)] = seg[c][i]; dryPtr[c][i] = r[(size_t) ((w - lat) & dryMask)]; ++w; }
        }
        dryWrite = w;

        engine.process (seg, nCh, len, ep);
        tone.process (seg, nCh, len, tp);
        resonance.process (seg, nCh, len, rp);

        for (int i = 0; i < len; ++i)
        {
            const float m = mixSmooth.getNextValue(), og = outGain.getNextValue();
            for (int c = 0; c < nCh; ++c) seg[c][i] = (dryPtr[c][i] * (1.0f - m) + seg[c][i] * m) * og;
        }
    }

    float op = 0.0f;
    for (int c = 0; c < nCh; ++c) op = juce::jmax (op, buffer.getMagnitude (c, 0, n));
    outPeak.store (op);
}

void KitsuneProcessor::toggleAB()
{
    slots[activeSlot] = apvts.copyState().createXml();
    activeSlot = 1 - activeSlot;
    if (slots[activeSlot] != nullptr)
        apvts.replaceState (juce::ValueTree::fromXml (*slots[activeSlot]));
}

void KitsuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void KitsuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* KitsuneProcessor::createEditor() { return new KitsuneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KitsuneProcessor(); }
