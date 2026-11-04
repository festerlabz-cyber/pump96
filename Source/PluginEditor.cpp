#include "PluginEditor.h"

using namespace btui;

namespace
{
    constexpr int kTopH = 66, kBarH = 60, kPad = 22;
    constexpr int kSmall = 46, kBig = 140;
}


//==============================================================================
void CurveEditor::refresh()
{
    const int v = proc.getPointsVersion();
    if (v != shownVersion && drag < 0) { pts = proc.getPoints(); shownVersion = v; }
    repaint();
}

double CurveEditor::evalCurve (double x) const
{
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const auto a = pts[i], b = pts[i + 1];
        if (x <= b.x || i + 2 == pts.size())
        {
            const double dx = b.x - a.x;
            return dx <= 1e-9 ? b.y : a.y + (b.y - a.y) * juce::jlimit (0.0, 1.0, (x - a.x) / dx);
        }
    }
    return pts.empty() ? 1.0 : pts.back().y;
}

void CurveEditor::paint (juce::Graphics& g)
{
    auto& f = Fonts::get();
    const auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1510), r.getCentreX(), r.getY(), juce::Colour (0xff0a0807), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    if (pts.size() < 2) return;

    const float W = (float) getWidth(), H = (float) getHeight();
    g.setFont (f.numbers (9.5f));
    const char* q[4] = { "0", "1/4", "1/2", "3/4" };
    for (int i = 0; i <= 16; ++i)
    {
        const float x = std::round (px (i / 16.0)) + 0.5f;
        const bool major = i % 4 == 0;
        g.setColour (juce::Colours::white.withAlpha (major ? 0.09f : 0.035f));
        g.drawVerticalLine ((int) x, PT, H - PB);
        if (major && i < 16) { g.setColour (col::faint); g.drawText (q[i / 4], (int) x + 3, (int) (H - PB) + 2, 40, 14, juce::Justification::centredLeft, false); }
    }
    for (double v : { 0.0, 0.5, 1.0 })
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) py (v), PL, W - PR);
    }
    auto& s = proc.apvts;
    const int rate = juce::roundToInt (s.getRawParameterValue ("rate")->load());
    const float offset = s.getRawParameterValue ("offset")->load();
    g.setColour (col::faint);
    juce::String head = juce::String ("1 period = ") + pm96::kRateNames[juce::jlimit (0, 6, rate)];
    if (offset > 0.5f) head << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  offset ")) << juce::roundToInt (offset) << " %";
    g.drawText (head, (int) PL, 4, 300, 14, juce::Justification::centredLeft, false);

    // effective gain (depth applied) as a filled area
    const double d = s.getRawParameterValue ("depth")->load() / 100.0;
    juce::Path area; area.startNewSubPath (px (0), py (0));
    const int N = juce::jmax (64, getWidth());
    for (int i = 0; i <= N; ++i) { const double x = i / (double) N; area.lineTo (px (x), py (1.0 - d * (1.0 - evalCurve (x)))); }
    area.lineTo (px (1), py (0)); area.closeSubPath();
    g.setColour (col::amber.withAlpha (0.13f));
    g.fillPath (area);

    // drawn curve
    juce::Path line;
    for (size_t i = 0; i < pts.size(); ++i) { const auto p = juce::Point<float> (px (pts[i].x), py (pts[i].y)); if (i == 0) line.startNewSubPath (p); else line.lineTo (p); }
    g.setColour (col::amber);
    g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // playhead
    const float xh = px (proc.phase.load());
    g.setColour (col::magenta.withAlpha (0.55f));
    g.drawVerticalLine ((int) xh, PT, H - PB);
    g.setColour (col::magenta);
    const float yh = py (proc.gain.load());
    g.fillEllipse (xh - 4, yh - 4, 8, 8);

    // handles
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const bool on = i == hover || i == drag;
        const float rr = on ? 7.0f : 5.0f, cx = px (pts[(size_t) i].x), cy = py (pts[(size_t) i].y);
        g.setColour (juce::Colour (0xff0b0908));
        g.fillEllipse (cx - rr, cy - rr, rr * 2, rr * 2);
        g.setColour (i == 0 || i == (int) pts.size() - 1 ? col::gold : col::amber);
        g.drawEllipse (cx - rr, cy - rr, rr * 2, rr * 2, 2.0f);
    }
}

int CurveEditor::hit (juce::Point<float> m) const
{
    int best = -1; float bd = 11.0f;
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const float dd = m.getDistanceFrom ({ px (pts[(size_t) i].x), py (pts[(size_t) i].y) });
        if (dd < bd) { bd = dd; best = i; }
    }
    return best;
}

void CurveEditor::moveTo (int i, double x, double y, bool freeMove)
{
    if (! freeMove) { x = std::round (x * 32.0) / 32.0; y = std::round (y * 20.0) / 20.0; }
    y = juce::jlimit (0.0, 1.0, y);
    const int last = (int) pts.size() - 1;
    if (i == 0) x = 0.0;
    else if (i == last) x = 1.0;
    else x = juce::jlimit (pts[(size_t) i - 1].x, pts[(size_t) i + 1].x, x);
    pts[(size_t) i] = { x, y };
}

void CurveEditor::commit()
{
    proc.setPointsFromEditor (pts);
    shownVersion = proc.getPointsVersion();
    repaint();
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        const int i = hit (e.position);
        if (i > 0 && i < (int) pts.size() - 1) { pts.erase (pts.begin() + i); hover = -1; commit(); }
        return;
    }
    int i = hit (e.position);
    if (i < 0)
    {
        if ((int) pts.size() >= pm96::kMaxPoints) return;
        double nx = juce::jlimit (0.0, 1.0, ux (e.position.x));
        if (! e.mods.isShiftDown()) nx = std::round (nx * 32.0) / 32.0;
        if (nx <= 0.0 || nx >= 1.0) return;
        size_t k = 1;
        while (k < pts.size() && pts[k].x <= nx) ++k;
        pts.insert (pts.begin() + (long) k, { nx, juce::jlimit (0.0, 1.0, uy (e.position.y)) });
        i = (int) k;
        moveTo (i, nx, uy (e.position.y), e.mods.isShiftDown());
        commit();
    }
    drag = i;
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    moveTo (drag, ux (e.position.x), uy (e.position.y), e.mods.isShiftDown());
    commit();
}

void CurveEditor::mouseUp (const juce::MouseEvent&) { drag = -1; }

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = hit (e.position);
    if (i > 0 && i < (int) pts.size() - 1) { pts.erase (pts.begin() + i); hover = -1; drag = -1; commit(); }
}

//==============================================================================
MainPanel::MainPanel (Pump96Processor& p) : proc (p), curve (p)
{
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    depth = addKnob ("depth", "Depth", col::amber, kSmall);
    rate  = addKnob ("rate", "Rate", col::amber, kSmall);
    pairs = {{ { addKnob ("smooth", "Smooth", col::magenta, kSmall), addKnob ("offset", "Offset", col::magenta, kSmall) },
               { addKnob ("band", "Band", col::gold, kSmall),        addKnob ("split", "Split", col::gold, kSmall) } }};
    outKnob = addKnob ("out", "Output", col::olive, kSmall);
    groups = {{ { "Curve", col::amber, {} }, { "Feel", col::magenta, {} }, { "Band", col::gold, {} }, { "Out", col::olive, {} } }};
    addAndMakeVisible (curve);

    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", bypassBtn);

    for (auto* sh : pm96::kShapeNames) shapeBox.addItem (sh, shapeBox.getNumItems() + 1);
    shapeBox.setTitle ("Shape"); presetBox.setTitle ("Preset");
    addAndMakeVisible (shapeBox);
    shapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "shape", shapeBox);

    const auto& presets = pm96::factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i) presetBox.addItem (presets[(size_t) i].name, i + 1);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) { proc.loadPreset (id - 1); shownPreset = -2; }
    };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (prevBtn); addAndMakeVisible (nextBtn);

    setSize (W, H);
    tick();
}

Knob* MainPanel::addKnob (const juce::String& id, const juce::String& label, juce::Colour c, int diameter, bool big)
{
    const auto* spec = pm96::findSpec (id);
    const auto unit = spec != nullptr ? spec->unit : pm96::Unit::percent;
    std::function<juce::String (double)> fmt = [unit] (double v) { return pm96::formatValue (unit, v); };
    if (spec != nullptr && unit == pm96::Unit::choice)
        fmt = [choices = spec->choices] (double v) { return choices[juce::jlimit (0, choices.size() - 1, juce::roundToInt (v))]; };
    std::optional<float> ref;
    if (id == "out") ref = 0.0f;
    knobs.push_back (std::make_unique<Knob> (proc.apvts, id, label, c, fmt, diameter, big, ref));
    addAndMakeVisible (*knobs.back());
    return knobs.back().get();
}

void MainPanel::paint (juce::Graphics& g)
{
    auto& f = Fonts::get();
    juce::ColourGradient bg (col::panel2, 0, 0, col::panel, 0, (float) H, false);
    g.setGradientFill (bg);
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (int x = 0; x < W; x += 3) g.drawVerticalLine (x, 0, (float) (H - kBarH));

    // title strip
    if (logo.isValid())
    {
        const float lh = 48.0f, lw = lh * (float) logo.getWidth() / (float) logo.getHeight();
        g.setOpacity (1.0f);   // drawImage uses the current opacity, which the texture lines above left at 1 %
        g.drawImage (logo, juce::Rectangle<float> ((float) kPad, 10.0f, lw, lh), juce::RectanglePlacement::centred);
        const auto title = f.display (36.0f, 0.04f);
        const float tx = (float) kPad + lw + 18.0f;
        g.setFont (title);
        g.setColour (col::text);
        g.drawText ("PUMP", juce::Rectangle<float> (tx, 12.0f, 200.0f, 44.0f), juce::Justification::centredLeft, false);
        const float rw = juce::GlyphArrangement::getStringWidth (title, "PUMP ");
        g.setColour (col::amber);
        g.drawText ("96", juce::Rectangle<float> (tx + rw, 12.0f, 80.0f, 44.0f), juce::Justification::centredLeft, false);
    }
    g.setColour (col::gold);
    g.setFont (f.label (9.5f, 0.14f));
    g.drawText ("VERSION 0.1", bypassBtn.getX() - 220, 18, 200, 13, juce::Justification::centredRight, false);
    g.drawText ("BOOMTOWNBEATS.COM", bypassBtn.getX() - 220, 33, 200, 13, juce::Justification::centredRight, false);

    for (auto& grp : groups)
    {
        const auto r = grp.bounds.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.22f), 0, r.getY(), juce::Colours::black.withAlpha (0.05f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (grp.colour);
        g.drawRoundedRectangle (r.reduced (1.0f), 13.0f, 2.0f);
        g.setFont (f.display (16.0f, 0.16f));
        const bool left = grp.title == "Curve";
        g.drawText (grp.title.toUpperCase(), grp.bounds.withHeight (32).withTrimmedTop (8).withTrimmedLeft (left ? 14 : 0),
                    left ? juce::Justification::centredLeft : juce::Justification::centred, false);
    }

    // curve hint and the live gain readout
    g.setColour (col::faint);
    g.setFont (f.body (10.5f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Drag points \xc2\xb7 click = add \xc2\xb7 double-click = remove \xc2\xb7 Shift = free")),
                groups[0].bounds.getRight() - 360, groups[0].bounds.getY() + 10, 344, 16, juce::Justification::centredRight, false);
    g.setColour (col::dim);
    g.setFont (f.label (9.0f, 0.12f));
    g.drawText ("GAIN", gainArea.withHeight (13), juce::Justification::centred, false);
    g.setColour (col::magenta);
    g.setFont (f.numbers (11.0f));
    const float gd = juce::Decibels::gainToDecibels (proc.gain.load(), -100.0f);
    const juce::String gs = gd < -60.0f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92\xe2\x88\x9e")) : (gd > -0.05f ? juce::String ("0.0") : juce::String (gd, 1));
    g.drawText (gs + " dB", gainArea.withTrimmedTop (16).withHeight (14), juce::Justification::centred, false);

    // bottom bar
    const int by = H - kBarH;
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRect (0, by, W, kBarH);
    g.setColour (col::line);
    g.drawHorizontalLine (by, 0, (float) W);
    const auto barFont = f.display (17.0f, 0.12f);
    auto barLabel = [&] (const juce::Component& c, const juce::String& t)
    {
        g.setFont (barFont);
        g.setColour (col::text);
        const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (barFont, t));
        g.drawText (t, c.getX() - w - 10, c.getY(), w + 2, c.getHeight(), juce::Justification::centredRight, false);
    };
    barLabel (shapeBox, "SHAPE:");
    barLabel (presetBox, "PRESETS:");
    g.setFont (barFont);
    g.setColour (col::text);
    const int tx = shapeBox.getRight() + 50;
    g.drawText ("TEMPO:", tx, shapeBox.getY(), 80, shapeBox.getHeight(), juce::Justification::centredLeft, false);
    g.setColour (col::amber);
    g.setFont (f.body (15.0f, true));
    g.drawText (juce::String (proc.hostBpm.load(), 1) + " BPM", tx + 76, shapeBox.getY(), 100, shapeBox.getHeight(), juce::Justification::centredLeft, false);
    g.setColour (col::faint);
    g.setFont (f.label (9.5f, 0.14f));
    g.drawText (proc.hostPlaying.load() ? "FROM HOST" : "FREE RUNNING", tx + 166, shapeBox.getY(), 140, shapeBox.getHeight(), juce::Justification::centredLeft, false);
}

void MainPanel::paintOverChildren (juce::Graphics& g)
{
    for (auto pt : { juce::Point<float> (12, 12), { (float) W - 12, 12 }, { 12, (float) H - 12 }, { (float) W - 12, (float) H - 12 } })
    {
        juce::ColourGradient sg (juce::Colour (0xff6b5f52), pt.x - 2, pt.y - 2, juce::Colour (0xff2b241e), pt.x + 4, pt.y + 4, true);
        g.setGradientFill (sg);
        g.fillEllipse (pt.x - 5.5f, pt.y - 5.5f, 11, 11);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (juce::Line<float> (pt.x - 3.5f, pt.y - 2.5f, pt.x + 3.5f, pt.y + 2.5f), 1.0f);
    }
}

void MainPanel::resized()
{
    bypassBtn.setBounds (W - kPad - 104, 18, 104, 30);

    const int top = kTopH + 8, bottom = H - kBarH - 12, mainH = bottom - top;
    // Depth / Rate column
    const int colW = depth->naturalWidth(), colH = depth->naturalHeight();
    depth->setBounds (kPad - 6, top + 6, colW, colH);
    rate->setBounds (kPad - 6, bottom - colH - 6, colW, colH);

    // Feel, Band and Out on the right, the curve fills the rest
    const int gap = 10, gw = 124;
    const int gx1 = W - kPad, gx0 = gx1 - 3 * gw - 2 * gap;
    const int kw = pairs[0].first->naturalWidth(), kh = pairs[0].first->naturalHeight();
    for (int i = 0; i < 3; ++i)
    {
        auto r = juce::Rectangle<int> (gx0 + i * (gw + gap), top, gw, mainH);
        groups[(size_t) i + 1].bounds = r;
        const int kx = r.getCentreX() - kw / 2;
        if (i < 2)
        {
            pairs[(size_t) i].first->setBounds (kx, r.getY() + 34, kw, kh);
            pairs[(size_t) i].second->setBounds (kx, r.getY() + 34 + kh + 8, kw, kh);
        }
        else
        {
            outKnob->setBounds (kx, r.getY() + 34, kw, kh);
            gainArea = { r.getX(), r.getY() + 34 + kh + 18, r.getWidth(), 32 };
        }
    }
    const int cx0 = kPad - 6 + colW + 12;
    groups[0].bounds = { cx0, top, gx0 - 12 - cx0, mainH };
    curve.setBounds (groups[0].bounds.reduced (12, 0).withTrimmedTop (34).withTrimmedBottom (12));

    // bottom bar
    const int by = H - kBarH, cy = by + (kBarH - 30) / 2;
    shapeBox.setBounds (kPad + 72, cy, 120, 30);
    nextBtn.setBounds (W - kPad - 28, cy + 1, 28, 28);
    prevBtn.setBounds (nextBtn.getX() - 34, cy + 1, 28, 28);
    presetBox.setBounds (prevBtn.getX() - 12 - 200, cy, 200, 30);
}

void MainPanel::refreshPresetBox()
{
    const int idx = proc.getPresetIndex();
    const bool mod = proc.isPresetModified();
    if (idx == shownPreset && mod == shownModified) return;
    shownPreset = idx; shownModified = mod;
    const auto& presets = pm96::factoryPresets();
    const auto name = presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, idx)].name;
    if (mod) presetBox.setText (name + " *", juce::dontSendNotification);
    else presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

void MainPanel::stepPreset (int delta)
{
    const int n = (int) pm96::factoryPresets().size();
    proc.loadPreset ((proc.getPresetIndex() + delta + n) % n);
    shownPreset = -2;
    refreshPresetBox();
}

void MainPanel::tick()
{
    refreshPresetBox();
    curve.refresh();
    const float gd = juce::Decibels::gainToDecibels (proc.gain.load(), -100.0f);
    if (std::abs (gd - shownGainDb) > 0.05f) { shownGainDb = gd; repaint (gainArea); }
    const double bpm = proc.hostBpm.load(); const bool playing = proc.hostPlaying.load();
    if (std::abs (bpm - shownBpm) > 0.01 || playing != shownPlaying) { shownBpm = bpm; shownPlaying = playing; repaint (0, H - kBarH, W, kBarH); }
}

//==============================================================================
Pump96Editor::Pump96Editor (Pump96Processor& p)
    : AudioProcessorEditor (p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits ((int) (MainPanel::W * 0.7), (int) (MainPanel::H * 0.7), (int) (MainPanel::W * 1.6), (int) (MainPanel::H * 1.6));
    getConstrainer()->setFixedAspectRatio ((double) MainPanel::W / MainPanel::H);
    setSize (MainPanel::W, MainPanel::H);
    startTimerHz (30);
}

Pump96Editor::~Pump96Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Pump96Editor::resized()
{
    const float s = (float) getWidth() / (float) MainPanel::W;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, MainPanel::W, MainPanel::H);
}
