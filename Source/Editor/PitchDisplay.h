#pragma once
#include <JuceHeader.h>
#include "KitsuneLookAndFeel.h"
#include "../PluginProcessor.h"

namespace kitsune::ui
{
/** Real-time pitch display: detected pitch (purple) vs corrected pitch (pink) over a note grid, plus I/O meters. */
class PitchDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PitchDisplay (KitsuneProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0f4a), 0, 0, juce::Colour (0xff10072a), 0, bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, 10.0f);

        const float meterW = 46.0f;
        auto plot = bounds.reduced (10.0f, 8.0f);
        plot.removeFromRight (meterW);
        plot.removeFromLeft (28.0f);

        const float lo = viewCentre - 12.0f, hi = viewCentre + 12.0f;
        auto yFor = [&] (float midi) { return plot.getBottom() - (midi - lo) / (hi - lo) * plot.getHeight(); };
        const auto mask = proc.currentNoteMask();
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        for (int m = (int) std::floor (lo); m <= (int) std::ceil (hi); ++m)
        {
            const int pc = ((m % 12) + 12) % 12;
            const bool allowed = (mask >> pc) & 1;
            const float y = yFor ((float) m);
            if (y < plot.getY() - 1 || y > plot.getBottom() + 1) continue;
            g.setColour (allowed ? purple.withAlpha (0.28f) : juce::Colour (0x18ffffff));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            if (allowed)
            {
                g.setColour (textDim);
                g.setFont (font (10.0f));
                g.drawText (juce::String (names[pc]) + juce::String (m / 12 - 1), (int) bounds.getX() + 6, (int) y - 7, 36, 14, juce::Justification::left);
            }
        }

        // history
        const int N = 768;
        const auto& e = proc.getEngine();
        const int wr = e.historyWrite.load (std::memory_order_acquire);
        juce::Path pDet, pCor; bool dOpen = false, cOpen = false;
        for (int i = 0; i < N; ++i)
        {
            const int idx = ((wr - N + i) % kitsune::PitchEngine::kHistory + kitsune::PitchEngine::kHistory) % kitsune::PitchEngine::kHistory;
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (N - 1);
            const float d = e.detectedHistory[(size_t) idx].load (std::memory_order_relaxed);
            const float c = e.correctedHistory[(size_t) idx].load (std::memory_order_relaxed);
            auto add = [&] (juce::Path& p, bool& open, float v)
            {
                if (v < 0) { open = false; return; }
                const float y = yFor (v);
                if (! open) { p.startNewSubPath (x, y); open = true; } else p.lineTo (x, y);
            };
            add (pDet, dOpen, d); add (pCor, cOpen, c);
        }
        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        g.setColour (purple.withAlpha (0.9f));
        g.strokePath (pDet, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (pink);
        g.strokePath (pCor, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();

        // note read-out
        const float det = e.lastDetectedMidi.load();
        g.setColour (textBright);
        g.setFont (font (34.0f));
        juce::String big = "--", small = "";
        if (det >= 0)
        {
            const int nn = (int) std::lround (det);
            big = juce::String (names[((nn % 12) + 12) % 12]) + juce::String (nn / 12 - 1);
            small = juce::String ((int) std::lround ((det - (float) nn) * 100.0f)) + " ct";
        }
        g.drawText (big, (int) plot.getX() + 8, (int) plot.getY() + 4, 110, 40, juce::Justification::left);
        g.setColour (textDim); g.setFont (font (12.0f));
        g.drawText (small, (int) plot.getX() + 10, (int) plot.getY() + 42, 90, 16, juce::Justification::left);

        g.setFont (font (10.0f));
        g.setColour (purple); g.drawText ("DETECTED", (int) plot.getRight() - 150, (int) plot.getY() + 4, 70, 12, juce::Justification::right);
        g.setColour (pink);   g.drawText ("CORRECTED", (int) plot.getRight() - 78, (int) plot.getY() + 4, 76, 12, juce::Justification::right);

        // meters
        auto meters = bounds.reduced (10.0f, 8.0f).removeFromRight (meterW - 4.0f);
        drawMeter (g, meters.removeFromLeft (16.0f), inLevel, "IN");
        meters.removeFromLeft (6.0f);
        drawMeter (g, meters.removeFromLeft (16.0f), outLevel, "OUT");

        g.setColour (border);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.4f);
    }

private:
    static float levelToNorm (float lin)
    {
        const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    }
    void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float level, const char* label)
    {
        auto bar = r.withTrimmedBottom (14.0f);
        g.setColour (juce::Colour (0xff1b0d38)); g.fillRoundedRectangle (bar, 3.0f);
        const float h = bar.getHeight() * levelToNorm (level);
        auto fill = bar.withTop (bar.getBottom() - h);
        g.setGradientFill (juce::ColourGradient (pink, 0, bar.getY(), purple, 0, bar.getBottom(), false));
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (textDim); g.setFont (font (9.0f));
        g.drawText (label, r.removeFromBottom (13.0f).toNearestInt(), juce::Justification::centred);
    }

    void timerCallback() override
    {
        const auto& e = proc.getEngine();
        const int wr = e.historyWrite.load (std::memory_order_acquire);
        std::vector<float> v;
        for (int i = 0; i < 128; ++i)
        {
            const int idx = ((wr - 1 - i) % kitsune::PitchEngine::kHistory + kitsune::PitchEngine::kHistory) % kitsune::PitchEngine::kHistory;
            const float d = e.detectedHistory[(size_t) idx].load (std::memory_order_relaxed);
            if (d >= 0) v.push_back (d);
        }
        if (v.size() > 8)
        {
            std::sort (v.begin(), v.end());
            viewCentre += 0.08f * (v[v.size() / 2] - viewCentre);
        }
        inLevel = juce::jmax (proc.inPeak.exchange (0.0f), inLevel * 0.85f);
        outLevel = juce::jmax (proc.outPeak.exchange (0.0f), outLevel * 0.85f);
        repaint();
    }

    KitsuneProcessor& proc;
    float viewCentre = 60.0f, inLevel = 0.0f, outLevel = 0.0f;
};

/** Per-band activity of the dynamic harsh-frequency reducer. */
class ReductionDisplay : public juce::Component, private juce::Timer
{
public:
    explicit ReductionDisplay (KitsuneProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0f4a), 0, 0, juce::Colour (0xff10072a), 0, b.getBottom(), false));
        g.fillRoundedRectangle (b, 10.0f);
        auto area = b.reduced (30.0f, 24.0f);
        const int nb = kitsune::DynamicResonanceReducer::kBands;
        const float bw = area.getWidth() / (float) nb;
        g.setFont (font (10.0f));
        g.setColour (textDim);
        g.drawText ("REDUCTION (dB)", 14, 6, 160, 14, juce::Justification::left);
        for (int i = 0; i < nb; ++i)
        {
            const float red = -proc.getResonance().reductionDb[(size_t) i].load();
            const float h = juce::jlimit (0.0f, 1.0f, red / 18.0f) * area.getHeight();
            auto bar = juce::Rectangle<float> (area.getX() + bw * (float) i + 6.0f, area.getY(), bw - 12.0f, h);
            g.setGradientFill (juce::ColourGradient (pink, 0, area.getY(), purple, 0, area.getBottom(), false));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (textDim);
            const float f = kitsune::DynamicResonanceReducer::bandFrequency (i);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 1) + "k" : juce::String ((int) f),
                        (int) (area.getX() + bw * (float) i), (int) area.getBottom() + 4, (int) bw, 14, juce::Justification::centred);
        }
        g.setColour (border);
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.4f);
    }
private:
    void timerCallback() override { repaint(); }
    KitsuneProcessor& proc;
};
} // namespace kitsune::ui
