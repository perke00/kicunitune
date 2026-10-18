#pragma once
#include <JuceHeader.h>

namespace kitsune
{
struct PresetValue { const char* id; float value; };
struct FactoryPreset { const char* name; std::vector<PresetValue> values; };

class PresetManager
{
public:
    static const std::vector<FactoryPreset>& factoryPresets();
    /** Resets every parameter (except bypass) to default, then applies the preset. Message thread. */
    static void applyFactory (juce::AudioProcessorValueTreeState& apvts, int index);
    static juce::File userPresetDirectory();
    static juce::StringArray userPresetNames();
    static bool saveUser (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
    static bool loadUser (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
};
} // namespace kitsune
