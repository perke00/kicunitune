#pragma once
#include <JuceHeader.h>
#include "DSP/PitchEngine.h"
#include "DSP/ToneProcessor.h"
#include "DSP/DynamicResonanceReducer.h"

class KitsuneProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::AsyncUpdater
{
public:
    static constexpr const char* kVersion = "0.1.0";

    KitsuneProcessor();
    ~KitsuneProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "KITSUNE Auto Tune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // --- UI helpers ---
    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;
    kitsune::PitchEngine& getEngine() { return engine; }
    kitsune::DynamicResonanceReducer& getResonance() { return resonance; }
    std::uint16_t currentNoteMask() const;
    float getLatencyMs() const;
    void toggleAB();
    int  getActiveSlot() const { return activeSlot; }
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void parameterChanged (const juce::String& id, float value) override;
    void handleAsyncUpdate() override;
    void resetDsp();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    kitsune::PitchEngine engine;
    kitsune::ToneProcessor tone;
    kitsune::DynamicResonanceReducer resonance;

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<int> requestedMode { 0 };
    int activeMode = 0;
    bool wasBypassed = false;
    double sampleRate = 48000.0;

    // fixed-latency dry path (for dry/wet mix and bypass)
    std::vector<std::vector<float>> dryRing;
    std::vector<float> dryScratch[2];
    int dryMask = 0, dryWrite = 0;

    juce::SmoothedValue<float> inGain, outGain, mixSmooth;

    std::unique_ptr<juce::XmlElement> slots[2];
    int activeSlot = 0;
};
