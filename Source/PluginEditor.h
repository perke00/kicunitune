#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Editor/KitsuneLookAndFeel.h"
#include "Editor/PitchDisplay.h"

namespace kitsune::ui
{
using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& title, const juce::String& paramId,
          const juce::String& suffix, int decimals);
    void resized() override;
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<SliderAtt> att;
};

class MainComponent : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 980, kH = 670;
    explicit MainComponent (KitsuneProcessor&);
    ~MainComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int tab);
    void rebuildPresetMenu();
    void presetSelected();
    void promptSavePreset();

    KitsuneProcessor& proc;
    KitsuneLookAndFeel lnf;

    PitchDisplay pitchDisplay;
    ReductionDisplay reductionDisplay;

    juce::ComboBox presetBox;
    juce::TextButton bypassBtn { "BYPASS" }, pitchTab { "PITCH" }, toneTab { "TONE" };
    juce::TextButton saveBtn { "SAVE" }, abBtn { "A" }, undoBtn { "UNDO" }, redoBtn { "REDO" };
    juce::Label versionLabel;

    // pitch tab
    Knob speed, strength, humanize, vibrato, formant, refA;
    juce::ComboBox keyBox, scaleBox, modeBox;
    juce::TextButton formantKeep { "FORMANT KEEP" };
    juce::TextButton noteBtn[12];
    juce::Label keyL, scaleL, modeL;
    // tone tab
    Knob hpf, lowG, midG, highG, harshSens, harshAmount, harshAttack, harshRelease;
    juce::TextButton harshOn { "HARSH REDUCTION" };
    // always visible
    Knob inGain, mix, outGain;

    std::unique_ptr<ComboAtt> keyAtt, scaleAtt, modeAtt;
    std::unique_ptr<ButtonAtt> bypassAtt, formantKeepAtt, harshOnAtt, noteAtt[12];

    int tab = 0;
    int factoryCount = 0;
    juce::Rectangle<int> bottomPanel { 14, 372, 952, 250 };
};
} // namespace kitsune::ui

class KitsuneEditor : public juce::AudioProcessorEditor
{
public:
    explicit KitsuneEditor (KitsuneProcessor&);
    void resized() override;
private:
    kitsune::ui::MainComponent main;
};
