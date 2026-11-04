// Look and feel plus small widgets for the BoomTown Beats 96 GUIs.
// Shares its visual language with Console 96, Reverb 96 and Delay 96: boomtownbeats.com colours, Anton / Inter / JetBrains Mono.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace btui
{
namespace col
{
    const juce::Colour ink     { 0xff0d0b0a };
    const juce::Colour panel   { 0xff171310 };
    const juce::Colour panel2  { 0xff1f1a15 };
    const juce::Colour text    { 0xfff5efe4 };
    const juce::Colour dim     { 0xffcbbfa9 };
    const juce::Colour faint   { 0xff8c7f6b };
    const juce::Colour amber   { 0xffff9a1f };
    const juce::Colour magenta { 0xffff3d68 };
    const juce::Colour gold    { 0xffe2c38a };
    const juce::Colour olive   { 0xffb9c46a };
    const juce::Colour line    { 0xff332a22 };
}

//==============================================================================
struct Fonts
{
    juce::Typeface::Ptr anton, interSemi, interBold, mono;

    static Fonts& get() { static Fonts f; return f; }

    juce::Font display (float h, float tracking = 0.0f) const { return juce::Font (juce::FontOptions (anton).withHeight (h)).withExtraKerningFactor (tracking); }
    juce::Font body (float h, bool bold = false) const { return juce::Font (juce::FontOptions (bold ? interBold : interSemi).withHeight (h)); }
    juce::Font label (float h, float tracking = 0.12f) const { return body (h, true).withExtraKerningFactor (tracking); }
    juce::Font numbers (float h) const { return juce::Font (juce::FontOptions (mono).withHeight (h)); }

private:
    Fonts()
    {
        anton     = juce::Typeface::createSystemTypefaceFor (BinaryData::AntonRegular_ttf, BinaryData::AntonRegular_ttfSize);
        interSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
        interBold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
        mono      = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoMedium_ttf, BinaryData::JetBrainsMonoMedium_ttfSize);
    }
};

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::panel2);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber);
        setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, col::amber);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, col::amber);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override { return Fonts::get().interSemi; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const auto kc = s.findColour (juce::Slider::rotarySliderFillColourId);
        const bool big = (bool) s.getProperties().getWithDefault ("big", false);
        const float size = big ? (float) juce::jmin (w, h) - 20.0f : (float) juce::jmin (w, h);   // big knobs get 10 px of glow room
        const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float rArc = size * 0.46f, stroke = size * (big ? 0.065f : 0.07f);
        const float ang = a0 + pos * (a1 - a0);
        const float refPos = (float) s.getProperties().getWithDefault ("refPos", 0.0);
        const float angRef = a0 + refPos * (a1 - a0);
        const auto stroker = juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        const int steps = (int) s.getProperties().getWithDefault ("steps", 0);
        if (steps > 1)
        {
            for (int i = 0; i < steps; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / (float) (steps - 1);
                const float r = size * 0.44f, dot = size * 0.035f;
                const bool on = std::abs (a - ang) < 0.01f;
                g.setColour (on ? kc : juce::Colours::white.withAlpha (0.25f));
                g.fillEllipse (c.x + r * std::sin (a) - dot, c.y - r * std::cos (a) - dot, dot * 2, dot * 2);
            }
        }
        else
        {
        juce::Path track; track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, a0, a1, true);
        g.setColour (col::line);
        g.strokePath (track, stroker);
        }

        if (steps <= 1 && std::abs (ang - angRef) > 0.01f)
        {
            juce::Path v; v.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, juce::jmin (ang, angRef), juce::jmax (ang, angRef), true);
            if (big)
            {
                g.setColour (kc.withAlpha (0.25f));
                g.strokePath (v, juce::PathStrokeType (stroke * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (kc);
            g.strokePath (v, stroker);
        }

        const float rBody = size * 0.36f, rCap = size * 0.33f;
        if (big)
        {
            g.setColour (kc.withAlpha (0.18f));
            const float rDots = size * 0.40f;
            for (int i = 0; i <= 40; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / 40.0f;
                g.fillEllipse (c.x + rDots * std::sin (a) - 0.8f, c.y - rDots * std::cos (a) - 0.8f, 1.6f, 1.6f);
            }
        }
        g.setColour (juce::Colour (0xff0b0908));
        g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
        juce::ColourGradient grad (juce::Colour (0xff4a4038), c.x - rCap * 0.35f, c.y - rCap * 0.55f,
                                   juce::Colour (0xff141009), c.x + rCap * 0.6f, c.y + rCap * 0.9f, true);
        grad.addColour (0.55, juce::Colour (0xff231d18));
        g.setGradientFill (grad);
        g.fillEllipse (c.x - rCap, c.y - rCap, rCap * 2, rCap * 2);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - rCap, c.y - rCap, rCap * 2, rCap * 2, 1.0f);
        if (big)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            const float r2 = size * 0.27f;
            g.drawEllipse (c.x - r2, c.y - r2, r2 * 2, r2 * 2, 0.8f);
        }

        juce::Path ptr;
        const float pw = size * 0.06f, pl = size * 0.19f;
        ptr.addRoundedRectangle (-pw * 0.5f, -rCap, pw, pl, pw * 0.5f);
        g.setColour (kc);
        g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    // flat selectors in the bottom bar: amber text, small arrow, no box
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        if (box.isMouseOver (true))
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
        }
        const float ax = (float) w - 10.0f, ay = (float) h * 0.5f + 1.0f;
        juce::Path arrow; arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (col::amber);
        g.fillPath (arrow);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::get().body (15.0f, true); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 0, box.getWidth() - 20, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }
    juce::Font getPopupMenuFont() override { return Fonts::get().body (14.0f); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::panel2);
        g.setColour (col::line);
        g.drawRect (0, 0, w, h);
    }
};

//==============================================================================
// Rotary knob: label on top, value below; one parameter per knob
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, juce::String labelText,
          juce::Colour colour, std::function<juce::String (double)> fmt, int knobDiameter, bool isBig = false, std::optional<float> refValue = {})
        : label (std::move (labelText)), format (std::move (fmt)), big (isBig), diameter (knobDiameter)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setMouseDragSensitivity (big ? 240 : 180);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        slider.getProperties().set ("big", big);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
        if (auto* p = state.getParameter (paramID))
        {
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            if (refValue) slider.getProperties().set ("refPos", p->convertTo0to1 (*refValue));
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
            {
                slider.getProperties().set ("steps", choice->choices.size());
                slider.setMouseDragSensitivity (40 * choice->choices.size());
            }
        }
        slider.onValueChange = [this] { repaint(); };
        slider.setTitle (label);
        labelColour = big ? colour : col::dim;
    }

    int labelH() const noexcept { return big ? 22 : 13; }
    int naturalWidth() const noexcept { return big ? diameter + 24 : juce::jmax (diameter + 16, 76); }
    int naturalHeight() const noexcept { return labelH() + 4 + diameter + 4 + (big ? 18 : 14); }

    void refresh() { repaint(); }

    void resized() override
    {
        const int pad = big ? 10 : 0;   // big knobs keep room for their glow
        slider.setBounds ((getWidth() - diameter) / 2 - pad, labelH() + 4 - pad, diameter + 2 * pad, diameter + 2 * pad);
    }

    void paint (juce::Graphics& g) override
    {
        auto& f = Fonts::get();
        g.setColour (labelColour);
        if (big) g.setFont (f.display (18.0f, 0.14f));
        else     g.setFont (f.label (9.0f, 0.12f));
        g.drawText (label.toUpperCase(), 0, 0, getWidth(), labelH(), juce::Justification::centred, false);
        g.setColour (col::text);
        g.setFont (f.numbers (big ? 14.0f : 11.0f));
        const int vy = labelH() + 4 + diameter + 4;
        g.drawText (format (slider.getValue()), 0, vy, getWidth(), big ? 18 : 14, juce::Justification::centred, false);
    }

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String label;
    std::function<juce::String (double)> format;
    bool big;
    int diameter;
    juce::Colour labelColour;
};

//==============================================================================
class LedButton : public juce::Button
{
public:
    LedButton (const juce::String& buttonText, juce::Colour ledColour, bool warnStyle = false)
        : juce::Button (buttonText), led (ledColour), warn (warnStyle)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        g.setColour (col::ink);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (warn && on ? col::magenta : (over ? col::faint : col::line));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        const float d = 7.0f, lx = 10.0f, ly = r.getCentreY() - d * 0.5f;
        if (on) { g.setColour (led.withAlpha (0.35f)); g.fillEllipse (lx - 3, ly - 3, d + 6, d + 6); }
        g.setColour (on ? led : juce::Colour (0xff3a3029));
        g.fillEllipse (lx, ly, d, d);
        g.setColour (warn && on ? col::magenta : col::text);
        g.setFont (Fonts::get().label (10.0f, 0.1f));
        g.drawText (getButtonText().toUpperCase(), juce::Rectangle<float> (lx + d + 7, 0, r.getWidth() - (lx + d + 10), (float) getHeight()),
                    juce::Justification::centredLeft, false);
    }

private:
    juce::Colour led;
    bool warn;
};

//==============================================================================
// Small square ◀ / ▶ button for stepping through presets
class ArrowButton : public juce::Button
{
public:
    ArrowButton (const juce::String& name, bool pointsLeft) : juce::Button (name), left (pointsLeft) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? col::panel2 : col::ink);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (over ? col::faint : col::line);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        const auto c = r.getCentre();
        juce::Path p;
        if (left) p.addTriangle (c.x + 3.5f, c.y - 5, c.x + 3.5f, c.y + 5, c.x - 4, c.y);
        else      p.addTriangle (c.x - 3.5f, c.y - 5, c.x - 3.5f, c.y + 5, c.x + 4, c.y);
        g.setColour (col::amber);
        g.fillPath (p);
    }

private:
    bool left;
};
} // namespace btui
