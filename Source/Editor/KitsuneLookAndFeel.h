#pragma once
#include <JuceHeader.h>

namespace kitsune::ui
{
inline const juce::Colour bgDark   { 0xff0b0618 };
inline const juce::Colour panel    { 0xff150a2b };
inline const juce::Colour border   { 0xff5b2f99 };
inline const juce::Colour pink     { 0xffff5fb8 };
inline const juce::Colour purple   { 0xffb35cf2 };
inline const juce::Colour textDim  { 0xffb99af0 };
inline const juce::Colour textBright { 0xfff0e2ff };

inline juce::Font font (float h, bool bold = true, bool italic = false)
{
    int flags = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
    return juce::Font (juce::FontOptions (h, flags));
}

class KitsuneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KitsuneLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, textBright);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff120a26));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xffc0a2f5));
        setColour (juce::Slider::textBoxHighlightColourId, purple.withAlpha (0.5f));
        setColour (juce::ComboBox::textColourId, textBright);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, textBright);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, purple.withAlpha (0.55f));
        setColour (juce::TextButton::textColourOffId, textBright);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, textDim);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, textBright);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight()), r = d * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const float stroke = juce::jmax (2.5f, r * 0.10f);

        juce::Path track; track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (0xff2a1450));
        g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val; val.addCentredArc (c.x, c.y, r, r, 0.0f, a0, ang, true);
        g.setGradientFill (juce::ColourGradient (pink, c.x - r, c.y, purple, c.x + r, c.y, false));
        g.strokePath (val, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float br = r * 0.76f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d1655), c.x, c.y - br, juce::Colour (0xff0e0720), c.x, c.y + br, false));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (juce::Colour (0xff4b2a85));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.2f);

        const float i0 = br * 0.30f, i1 = br * 0.92f;
        g.setColour (juce::Colours::white);
        g.drawLine (c.x + std::sin (ang) * i0, c.y - std::cos (ang) * i0, c.x + std::sin (ang) * i1, c.y - std::cos (ang) * i1,
                    juce::jmax (2.0f, r * 0.07f));
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (0xff120a26));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (border.brighter (0.2f));
        g.drawRoundedRectangle (b, 5.0f, 1.2f);
        juce::Path arrow;
        const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 5.0f, ay - 2.5f); arrow.lineTo (ax, ay + 3.0f); arrow.lineTo (ax + 5.0f, ay - 2.5f);
        g.setColour (pink);
        g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& bt, const juce::Colour&, bool over, bool down) override
    {
        auto b = bt.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = bt.getToggleState();
        auto fill = on ? juce::ColourGradient (pink.withAlpha (0.85f), 0.0f, 0.0f, purple.withAlpha (0.85f), 0.0f, b.getBottom(), false)
                       : juce::ColourGradient (juce::Colour (0xff1c0e38), 0.0f, 0.0f, juce::Colour (0xff120a26), 0.0f, b.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (b, b.getHeight() * 0.3f);
        g.setColour ((on ? pink : border).brighter (over ? 0.4f : 0.1f).withAlpha (down ? 0.7f : 1.0f));
        g.drawRoundedRectangle (b, b.getHeight() * 0.3f, 1.3f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f); }
};
} // namespace kitsune::ui
