#include "PresetManager.h"

namespace kitsune
{
const std::vector<FactoryPreset>& PresetManager::factoryPresets()
{
    static const std::vector<FactoryPreset> p = {
        { "Init - Transparent",  { { "speed", 60 }, { "strength", 60 }, { "humanize", 50 }, { "vibrato", 100 } } },
        { "Natural Vocal",       { { "speed", 60 }, { "strength", 70 }, { "humanize", 60 }, { "vibrato", 100 } } },
        { "Transparent Tuning",  { { "speed", 90 }, { "strength", 55 }, { "humanize", 80 }, { "vibrato", 100 } } },
        { "Modern Pop",          { { "speed", 25 }, { "strength", 85 }, { "humanize", 30 }, { "vibrato", 70 }, { "toneHigh", 1.5f } } },
        { "Rap Vocal",           { { "speed", 15 }, { "strength", 80 }, { "humanize", 15 }, { "vibrato", 20 }, { "scale", 0 }, { "hpf", 90 } } },
        { "Melodic Rap",         { { "speed", 8 },  { "strength", 90 }, { "humanize", 10 }, { "vibrato", 40 }, { "hpf", 80 } } },
        { "Hard Tune",           { { "speed", 0 },  { "strength", 100 }, { "humanize", 0 }, { "vibrato", 0 } } },
        { "Aggressive Trap",     { { "speed", 0 },  { "strength", 100 }, { "humanize", 0 }, { "vibrato", 0 }, { "formantShift", 1.5f },
                                   { "harshOn", 1 }, { "harshAmount", 60 }, { "hpf", 100 } } },
        { "Smooth R&B",          { { "speed", 80 }, { "strength", 75 }, { "humanize", 70 }, { "vibrato", 100 }, { "toneLow", 1.5f },
                                   { "harshOn", 1 }, { "harshAmount", 35 } } },
        { "Studio Precision",    { { "speed", 35 }, { "strength", 100 }, { "humanize", 25 }, { "vibrato", 80 }, { "mode", 1 } } },
    };
    return p;
}

static void setReal (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void PresetManager::applyFactory (juce::AudioProcessorValueTreeState& apvts, int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;

    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (rp->paramID != "bypass")
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& v : list[(size_t) index].values) setReal (apvts, v.id, v.value);
}

juce::File PresetManager::userPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("KITSUNE").getChildFile ("Auto Tune").getChildFile ("Presets");
}

juce::StringArray PresetManager::userPresetNames()
{
    juce::StringArray names;
    for (auto& f : userPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

bool PresetManager::saveUser (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto dir = userPresetDirectory();
    if (! dir.createDirectory()) return false;
    if (auto xml = apvts.copyState().createXml())
        return xml->writeTo (dir.getChildFile (clean + ".xml"));
    return false;
}

bool PresetManager::loadUser (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    auto f = userPresetDirectory().getChildFile (juce::File::createLegalFileName (name) + ".xml");
    if (auto xml = juce::XmlDocument::parse (f))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            return true;
        }
    return false;
}
} // namespace kitsune
