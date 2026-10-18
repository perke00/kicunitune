#include "PluginEditor.h"
#include "Presets/PresetManager.h"

namespace kitsune::ui
{
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& title, const juce::String& paramId,
            const juce::String& suffix, int decimals)
{
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (11.0f));
    label.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    att = std::make_unique<SliderAtt> (apvts, paramId, slider);
    slider.textFromValueFunction = [suffix, decimals] (double v) { return juce::String (v, decimals) + " " + suffix; };
    slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    if (auto* p = apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.updateText();
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (16));
    slider.setBounds (b);
}

MainComponent::MainComponent (KitsuneProcessor& p)
    : proc (p), pitchDisplay (p), reductionDisplay (p),
      speed (p.apvts, "RETUNE SPEED", "speed", "ms", 1),
      strength (p.apvts, "STRENGTH", "strength", "%", 0),
      humanize (p.apvts, "HUMANIZE", "humanize", "%", 0),
      vibrato (p.apvts, "VIBRATO KEEP", "vibrato", "%", 0),
      formant (p.apvts, "FORMANT", "formantShift", "st", 2),
      refA (p.apvts, "REF A4", "refA", "Hz", 1),
      hpf (p.apvts, "HIGH-PASS", "hpf", "Hz", 0),
      lowG (p.apvts, "LOW", "toneLow", "dB", 1),
      midG (p.apvts, "MID", "toneMid", "dB", 1),
      highG (p.apvts, "HIGH", "toneHigh", "dB", 1),
      harshSens (p.apvts, "SENSITIVITY", "harshSens", "%", 0),
      harshAmount (p.apvts, "REDUCTION", "harshAmount", "%", 0),
      harshAttack (p.apvts, "ATTACK", "harshAttack", "ms", 1),
      harshRelease (p.apvts, "RELEASE", "harshRelease", "ms", 0),
      inGain (p.apvts, "INPUT", "inGain", "dB", 1),
      mix (p.apvts, "MIX", "mix", "%", 0),
      outGain (p.apvts, "OUTPUT", "outGain", "dB", 1)
{
    setLookAndFeel (&lnf);
    setSize (kW, kH);

    addAndMakeVisible (pitchDisplay);
    addChildComponent (reductionDisplay);

    // top bar
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.onChange = [this] { presetSelected(); };
    rebuildPresetMenu();

    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<ButtonAtt> (p.apvts, "bypass", bypassBtn);

    for (auto* b : { &pitchTab, &toneTab })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        addAndMakeVisible (*b);
    }
    pitchTab.setToggleState (true, juce::dontSendNotification);
    pitchTab.onClick = [this] { showTab (0); };
    toneTab.onClick  = [this] { showTab (1); };

    // pitch tab
    for (auto* k : { &speed, &strength, &humanize, &vibrato, &formant, &refA }) addAndMakeVisible (*k);

    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    for (int i = 0; i < kitsune::PitchCorrector::numScales(); ++i) scaleBox.addItem (kitsune::PitchCorrector::scaleName (i), i + 1);
    modeBox.addItemList ({ "Low Latency", "Studio Quality" }, 1);
    for (auto* c : { &keyBox, &scaleBox, &modeBox }) addAndMakeVisible (*c);
    keyAtt = std::make_unique<ComboAtt> (p.apvts, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt> (p.apvts, "scale", scaleBox);
    modeAtt = std::make_unique<ComboAtt> (p.apvts, "mode", modeBox);

    for (auto* l : { &keyL, &scaleL, &modeL })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (font (11.0f));
        addAndMakeVisible (*l);
    }
    keyL.setText ("KEY", juce::dontSendNotification);
    scaleL.setText ("SCALE", juce::dontSendNotification);
    modeL.setText ("PROCESSING MODE", juce::dontSendNotification);

    addAndMakeVisible (formantKeep);
    formantKeepAtt = std::make_unique<ButtonAtt> (p.apvts, "formantKeep", formantKeep);

    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
    {
        noteBtn[i].setButtonText (names[i]);
        addAndMakeVisible (noteBtn[i]);
        noteAtt[i] = std::make_unique<ButtonAtt> (p.apvts, "note" + juce::String (i), noteBtn[i]);
    }

    // tone tab
    for (auto* k : { &hpf, &lowG, &midG, &highG, &harshSens, &harshAmount, &harshAttack, &harshRelease }) addChildComponent (*k);
    addChildComponent (harshOn);
    harshOnAtt = std::make_unique<ButtonAtt> (p.apvts, "harshOn", harshOn);

    // always visible
    for (auto* k : { &inGain, &mix, &outGain }) addAndMakeVisible (*k);

    // bottom bar
    for (auto* b : { &saveBtn, &abBtn, &undoBtn, &redoBtn }) addAndMakeVisible (*b);
    saveBtn.onClick = [this] { promptSavePreset(); };
    abBtn.onClick = [this] { proc.toggleAB(); };
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    versionLabel.setFont (font (11.0f, false));
    versionLabel.setJustificationType (juce::Justification::centredLeft);
    versionLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (versionLabel);

    showTab (0);
    startTimerHz (5);
    timerCallback();
}

MainComponent::~MainComponent() { setLookAndFeel (nullptr); }

void MainComponent::showTab (int t)
{
    tab = t;
    const bool pitch = t == 0;
    pitchDisplay.setVisible (pitch);
    reductionDisplay.setVisible (! pitch);
    for (auto* c : std::initializer_list<juce::Component*> { &speed, &strength, &humanize, &vibrato, &formant, &refA,
                                                              &keyBox, &scaleBox, &modeBox, &keyL, &scaleL, &modeL, &formantKeep })
        c->setVisible (pitch);
    for (auto& b : noteBtn) b.setVisible (pitch);
    for (auto* c : std::initializer_list<juce::Component*> { &hpf, &lowG, &midG, &highG, &harshSens, &harshAmount,
                                                              &harshAttack, &harshRelease, &harshOn })
        c->setVisible (! pitch);
    repaint();
}

void MainComponent::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& list = kitsune::PresetManager::factoryPresets();
    factoryCount = (int) list.size();
    for (int i = 0; i < factoryCount; ++i) presetBox.addItem (list[(size_t) i].name, i + 1);
    auto users = kitsune::PresetManager::userPresetNames();
    if (users.size() > 0)
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < users.size(); ++i) presetBox.addItem (users[i], 1000 + i);
    }
}

void MainComponent::presetSelected()
{
    const int id = presetBox.getSelectedId();
    if (id >= 1 && id <= factoryCount) kitsune::PresetManager::applyFactory (proc.apvts, id - 1);
    else if (id >= 1000) kitsune::PresetManager::loadUser (proc.apvts, presetBox.getText());
}

void MainComponent::promptSavePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Preset");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainComponent> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            const auto name = w->getTextEditorContents ("name");
            if (kitsune::PresetManager::saveUser (safe->proc.apvts, name))
            {
                safe->rebuildPresetMenu();
                safe->presetBox.setText (name, juce::dontSendNotification);
            }
        }
    }), true);
}

void MainComponent::timerCallback()
{
    versionLabel.setText (juce::String ("KITSUNE Auto Tune v") + KitsuneProcessor::kVersion + "   |   "
                          + (proc.getEngine().getMode() == 0 ? "LOW LATENCY" : "STUDIO") + "   |   plugin latency "
                          + juce::String (proc.getLatencyMs(), 1) + " ms (+ host buffer)",
                          juce::dontSendNotification);
    abBtn.setButtonText (proc.getActiveSlot() == 0 ? "A" : "B");
    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());
}

void MainComponent::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b0a38), 0, 0, bgDark, 0, (float) kH, false));
    g.fillAll();

    // faint perspective grid under the panel
    g.setColour (purple.withAlpha (0.10f));
    for (int i = -10; i <= 10; ++i) g.drawLine ((float) kW * 0.5f + (float) i * 30.0f, 626.0f, (float) kW * 0.5f + (float) i * 110.0f, (float) kH);
    for (int y = 628; y < kH; y += 8) g.drawHorizontalLine (y, 0.0f, (float) kW);

    // logo
    auto logo = juce::Rectangle<float> (16.0f, 8.0f, 66.0f, 66.0f);
    g.setColour (juce::Colour (0xff2a1250)); g.fillEllipse (logo);
    g.setColour (border); g.drawEllipse (logo, 1.4f);
    juce::Path fox;
    auto pt = [&] (float x, float y) { return juce::Point<float> (logo.getX() + x * logo.getWidth(), logo.getY() + y * logo.getHeight()); };
    fox.startNewSubPath (pt (0.20f, 0.18f)); fox.lineTo (pt (0.40f, 0.36f)); fox.lineTo (pt (0.60f, 0.36f)); fox.lineTo (pt (0.80f, 0.18f));
    fox.lineTo (pt (0.84f, 0.56f)); fox.lineTo (pt (0.5f, 0.88f)); fox.lineTo (pt (0.16f, 0.56f)); fox.closeSubPath();
    g.setColour (juce::Colour (0xff4a2380)); g.fillPath (fox);
    g.setColour (pink); g.strokePath (fox, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
    g.setColour (pink); g.fillEllipse (pt (0.36f, 0.52f).x - 2.5f, pt (0.36f, 0.52f).y - 2.5f, 5.0f, 5.0f);
    g.fillEllipse (pt (0.64f, 0.52f).x - 2.5f, pt (0.64f, 0.52f).y - 2.5f, 5.0f, 5.0f);

    // title
    g.setGradientFill (juce::ColourGradient (pink, 96.0f, 10.0f, purple, 420.0f, 50.0f, false));
    g.setFont (font (34.0f, true, true).withExtraKerningFactor (0.06f));
    g.drawText ("KITSUNE AUTO TUNE", 96, 8, 330, 44, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (font (9.5f).withExtraKerningFactor (0.28f));
    g.drawText ("INTELLIGENT PITCH PROCESSOR", 98, 52, 320, 14, juce::Justification::centredLeft);

    // bottom panel
    g.setColour (panel.withAlpha (0.92f));
    g.fillRoundedRectangle (bottomPanel.toFloat(), 12.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bottomPanel.toFloat().reduced (0.5f), 12.0f, 1.4f);

    g.setColour (pink);
    g.setFont (font (15.0f, true, true));
    g.drawText ("By prodbyperke", kW - 220, 636, 204, 24, juce::Justification::centredRight);
}

void MainComponent::resized()
{
    presetBox.setBounds (598, 22, 206, 30);
    bypassBtn.setBounds (884, 22, 82, 30);
    pitchTab.setBounds (432, 28, 72, 26);
    toneTab.setBounds (508, 28, 72, 26);

    juce::Rectangle<int> disp (14, 88, 952, 270);
    pitchDisplay.setBounds (disp);
    reductionDisplay.setBounds (disp);

    const juce::Rectangle<int> bigKnob (28, 384, 176, 226);
    speed.setBounds (bigKnob);
    harshAmount.setBounds (bigKnob);

    auto col = [] (int i) { return 222 + 96 * i; };
    const int r1 = 384, r2 = 498, w = 92, h = 112;

    strength.setBounds (col (0), r1, w, h);  humanize.setBounds (col (1), r1, w, h);  vibrato.setBounds (col (2), r1, w, h);
    formant.setBounds  (col (3), r1, w, h);  refA.setBounds     (col (4), r1, w, h);

    keyL.setBounds (232, 506, 70, 14);    keyBox.setBounds (232, 522, 72, 28);
    scaleL.setBounds (322, 506, 120, 14); scaleBox.setBounds (322, 522, 148, 28);
    modeL.setBounds (486, 506, 130, 14);  modeBox.setBounds (486, 522, 130, 28);
    formantKeep.setBounds (632, 522, 112, 28);
    for (int i = 0; i < 12; ++i) noteBtn[i].setBounds (232 + i * 43, 566, 40, 28);

    hpf.setBounds (col (0), r1, w, h);  lowG.setBounds (col (1), r1, w, h);  midG.setBounds (col (2), r1, w, h);
    highG.setBounds (col (3), r1, w, h); harshSens.setBounds (col (4), r1, w, h);
    harshAttack.setBounds (col (0), r2, w, h); harshRelease.setBounds (col (1), r2, w, h);
    harshOn.setBounds (col (2) + 6, r2 + 44, 170, 30);

    mix.setBounds (772, r1, w, h);   outGain.setBounds (868, r1, w, h);   inGain.setBounds (772, r2, w, h);

    saveBtn.setBounds (14, 634, 64, 26);  abBtn.setBounds (86, 634, 44, 26);
    undoBtn.setBounds (138, 634, 64, 26); redoBtn.setBounds (210, 634, 64, 26);
    versionLabel.setBounds (290, 634, 480, 26);
}
} // namespace kitsune::ui

KitsuneEditor::KitsuneEditor (KitsuneProcessor& p) : juce::AudioProcessorEditor (&p), main (p)
{
    addAndMakeVisible (main);
    setResizable (true, true);
    setResizeLimits (kitsune::ui::MainComponent::kW * 3 / 4, kitsune::ui::MainComponent::kH * 3 / 4,
                     kitsune::ui::MainComponent::kW * 2, kitsune::ui::MainComponent::kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kitsune::ui::MainComponent::kW / kitsune::ui::MainComponent::kH);
    setSize (kitsune::ui::MainComponent::kW, kitsune::ui::MainComponent::kH);
}

void KitsuneEditor::resized()
{
    main.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kitsune::ui::MainComponent::kW));
    main.setBounds (0, 0, kitsune::ui::MainComponent::kW, kitsune::ui::MainComponent::kH);
}
