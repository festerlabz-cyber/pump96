#pragma once

#include "PluginProcessor.h"
#include "UIComponents.h"

//==============================================================================
// The drawn volume curve: drag points, click on an empty spot to add one, double-click (or right-click) to remove one.
// Points snap to a 1/32 grid unless Shift is held. The playhead shows where the host is in the period.
class CurveEditor : public juce::Component
{
public:
    explicit CurveEditor (Pump96Processor& p) : proc (p) {}
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { if (drag < 0) { hover = -1; repaint(); } }

private:
    Pump96Processor& proc;
    std::vector<pm96::Pt> pts;
    int shownVersion = -1, hover = -1, drag = -1;

    static constexpr float PL = 14, PR = 14, PT = 22, PB = 20;
    float px (double x) const { return PL + (float) x * ((float) getWidth() - PL - PR); }
    float py (double y) const { return PT + (float) (1.0 - y) * ((float) getHeight() - PT - PB); }
    double ux (float X) const { return (X - PL) / ((float) getWidth() - PL - PR); }
    double uy (float Y) const { return 1.0 - (Y - PT) / ((float) getHeight() - PT - PB); }
    double evalCurve (double x) const;
    int hit (juce::Point<float>) const;
    void moveTo (int i, double x, double y, bool freeMove);
    void commit();
};

//==============================================================================
// Everything is laid out at the design size; the editor scales this component as a whole.
// Layout (left to right): Depth / Rate column, the curve editor, Feel / Band / Out groups; bottom bar with Shape,
// host tempo and Presets. Same layout as the browser prototype.
class MainPanel : public juce::Component
{
public:
    static constexpr int W = 1160, H = 372;

    explicit MainPanel (Pump96Processor&);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void tick();

private:
    struct Group { juce::String title; juce::Colour colour; juce::Rectangle<int> bounds; };

    Pump96Processor& proc;
    juce::Image logo;
    std::vector<std::unique_ptr<btui::Knob>> knobs;
    btui::Knob *depth = nullptr, *rate = nullptr, *outKnob = nullptr;
    std::array<std::pair<btui::Knob*, btui::Knob*>, 2> pairs {};
    std::array<Group, 4> groups;     // Curve, Feel, Band, Out
    CurveEditor curve;
    juce::Rectangle<int> gainArea;

    btui::LedButton bypassBtn { "Bypass", btui::col::magenta, true };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    juce::ComboBox shapeBox, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAtt;
    btui::ArrowButton prevBtn { "Previous preset", true }, nextBtn { "Next preset", false };
    int shownPreset = -2; bool shownModified = false;
    double shownBpm = -1; bool shownPlaying = false;
    float shownGainDb = 1000.0f;

    btui::Knob* addKnob (const juce::String& id, const juce::String& label, juce::Colour c, int diameter, bool big = false);
    void refreshPresetBox();
    void stepPreset (int delta);
};

//==============================================================================
class Pump96Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Pump96Editor (Pump96Processor&);
    ~Pump96Editor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (btui::col::ink); }

private:
    void timerCallback() override { panel.tick(); }
    btui::LookAndFeel lnf;
    MainPanel panel;
};
