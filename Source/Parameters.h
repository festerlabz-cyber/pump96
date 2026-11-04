// Parameter definitions, value formatting and factory presets.
// Parameter IDs match the browser prototype keys, so presets carry over 1:1.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Pump96DSP.h"

namespace pm96
{
enum class Unit { percent, dB, ms, hz, toggle, choice };

struct ParamSpec
{
    juce::String id, name;
    int engineIndex;
    Unit unit;
    float min, max, def;
    bool logScale = false;
    juce::StringArray choices {};
};

inline juce::String formatHz (double v)
{
    if (juce::roundToInt (v) >= 1000) return juce::String (v / 1000.0, v >= 9995.0 ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (v)) + " Hz";
}

inline juce::String formatValue (Unit unit, double v)
{
    switch (unit)
    {
        case Unit::percent: return juce::String (juce::roundToInt (v)) + " %";
        case Unit::dB:      return (v > 0.04 ? "+" : "") + juce::String (v, 1) + " dB";
        case Unit::ms:      return v < 0.05 ? juce::String ("Off") : (v < 10.0 ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms";
        case Unit::hz:      return formatHz (v);
        case Unit::toggle:  return v > 0.5 ? "On" : "Off";
        case Unit::choice:  break;
    }
    return juce::String (v);
}

inline const std::vector<ParamSpec>& paramSpecs()
{
    static const std::vector<ParamSpec> specs = []
    {
        std::vector<ParamSpec> s;
        juce::StringArray rates, bands, shapes;
        for (auto* r : kRateNames) rates.add (r);
        for (auto* b : kBandNames) bands.add (b);
        for (auto* sh : kShapeNames) shapes.add (sh);
        s.push_back ({ "depth",  "Depth",  P::depth,  Unit::percent, 0, 100, 80 });
        s.push_back ({ "rate",   "Rate",   P::rate,   Unit::choice,  0, 6, 3, false, rates });
        s.push_back ({ "shape",  "Shape",  P::shape,  Unit::choice,  0, 6, 0, false, shapes });
        s.push_back ({ "smooth", "Smooth", P::smooth, Unit::ms,      0, 50, 2 });
        s.push_back ({ "offset", "Offset", P::offset, Unit::percent, 0, 100, 0 });
        s.push_back ({ "band",   "Band",   P::band,   Unit::choice,  0, 2, 0, false, bands });
        s.push_back ({ "split",  "Split",  P::split,  Unit::hz,      40, 2000, 150, true });
        s.push_back ({ "out",    "Output", P::out,    Unit::dB,      -12, 12, 0 });
        s.push_back ({ "bypass", "Bypass", P::bypass, Unit::toggle,  0, 1, 0 });
        return s;
    }();
    return specs;
}

inline const ParamSpec* findSpec (const juce::String& id)
{
    for (auto& s : paramSpecs()) if (s.id == id) return &s;
    return nullptr;
}

inline juce::NormalisableRange<float> makeRange (const ParamSpec& s)
{
    if (s.logScale)
        return { s.min, s.max,
                 [] (float a, float b, float x) { return a * std::pow (b / a, x); },
                 [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                 nullptr };
    return { s.min, s.max };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& s : paramSpecs())
    {
        const juce::ParameterID pid { s.id, 1 };
        if (s.unit == Unit::toggle)
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f));
        else if (s.unit == Unit::choice)
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name, s.choices, (int) s.def));
        else
        {
            const auto unit = s.unit;
            auto attrs = juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([unit] (float v, int) { return formatValue (unit, v); })
                .withValueFromStringFunction ([unit] (const juce::String& t)
                {
                    auto v = t.retainCharacters ("-0123456789.").getFloatValue();
                    if (unit == Unit::hz && t.containsIgnoreCase ("k")) v *= 1000.0f;
                    return v;
                });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, s.name, makeRange (s), s.def, attrs));
        }
    }
    return layout;
}

//==============================================================================
struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; std::vector<Pt> points {}; };

// Choice values are indices. Bypass is never changed by presets; anything not listed goes to its default.
// A preset with points uses the Custom shape and that curve.
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init", {} },
        { "Classic Pump",     { {"shape",0},{"depth",80},{"rate",3} } },
        { "Gabber Sidechain", { {"shape",2},{"depth",100},{"rate",3},{"smooth",1} } },
        { "Bass Duck",        { {"shape",0},{"depth",100},{"rate",3},{"band",1},{"split",180},{"smooth",3} } },
        { "Rave Chop",        { {"shape",3},{"depth",100},{"rate",1},{"smooth",1.5f} } },
        { "Hoover Breathe",   { {"shape",1},{"depth",70},{"rate",4},{"smooth",8} } },
        { "Offbeat Bounce",   { {"shape",6},{"depth",75},{"rate",3},{"smooth",4} }, { {0,0.15},{0.5,1},{1,0.15} } },
        { "Hakkuh Stutter",   { {"shape",3},{"depth",100},{"rate",0},{"band",2},{"split",400},{"smooth",0.5f} } },
    };
    return presets;
}
// curve points as text for the saved state: "x,y;x,y;..."
inline juce::String pointsToString (const std::vector<Pt>& pts)
{
    juce::StringArray a;
    for (auto& p : pts) a.add (juce::String (p.x, 5) + "," + juce::String (p.y, 5));
    return a.joinIntoString (";");
}
inline std::vector<Pt> pointsFromString (const juce::String& s)
{
    std::vector<Pt> pts;
    for (auto& item : juce::StringArray::fromTokens (s, ";", ""))
        if (item.contains (","))
            pts.push_back ({ item.upToFirstOccurrenceOf (",", false, false).getDoubleValue(), item.fromFirstOccurrenceOf (",", false, false).getDoubleValue() });
    return sanitize (pts);
}
} // namespace pm96
