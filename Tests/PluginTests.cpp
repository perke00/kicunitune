// JUCE UnitTest console app: state restore, presets, automation, latency, NaN safety.
// NOTE: written but NOT yet compiled/run by the author (no JUCE/MSVC available in the authoring sandbox).
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Presets/PresetManager.h"

struct KitsunePluginTests : public juce::UnitTest
{
    KitsunePluginTests() : juce::UnitTest ("KITSUNE plugin", "KITSUNE") {}

    void runTest() override
    {
        beginTest ("parameters exist and are automatable");
        {
            KitsuneProcessor p;
            expect (p.getParameters().size() > 30);
            auto* speed = p.apvts.getParameter ("speed");
            expect (speed != nullptr);
            speed->setValueNotifyingHost (speed->convertTo0to1 (123.0f));
            expectWithinAbsoluteError (p.apvts.getRawParameterValue ("speed")->load(), 123.0f, 0.2f);
        }
        beginTest ("state save / restore");
        {
            KitsuneProcessor a, b;
            kitsune::PresetManager::applyFactory (a.apvts, 6);   // Hard Tune
            juce::MemoryBlock mb; a.getStateInformation (mb);
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.apvts.getRawParameterValue ("speed")->load(), a.apvts.getRawParameterValue ("speed")->load());
            expectEquals (b.apvts.getRawParameterValue ("strength")->load(), 100.0f);
        }
        beginTest ("factory presets change real parameters");
        {
            KitsuneProcessor p;
            kitsune::PresetManager::applyFactory (p.apvts, 9);   // Studio Precision
            expectEquals ((int) p.apvts.getRawParameterValue ("mode")->load(), 1);
            kitsune::PresetManager::applyFactory (p.apvts, 6);
            expectEquals (p.apvts.getRawParameterValue ("speed")->load(), 0.0f);
        }
        beginTest ("latency reported and audio finite");
        {
            KitsuneProcessor p;
            p.setPlayConfigDetails (2, 2, 48000.0, 512);
            p.prepareToPlay (48000.0, 512);
            expect (p.getLatencySamples() > 0);
            juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
            juce::Random r (7);
            for (int blk = 0; blk < 200; ++blk)
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i)
                    buf.setSample (c, i, 0.4f * std::sin (0.03f * (float) (blk * 512 + i)) + 0.01f * r.nextFloat());
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) expect (std::isfinite (buf.getSample (c, i)));
            }
        }
    }
};
static KitsunePluginTests kitsunePluginTests;

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::UnitTestRunner runner; runner.runAllTests();
    int fails = 0;
    for (int i = 0; i < runner.getNumResults(); ++i) fails += runner.getResult (i)->failures;
    return fails ? 1 : 0;
}
