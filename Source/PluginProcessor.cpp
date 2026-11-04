#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace pm96;

Pump96Processor::Pump96Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Pump96", createLayout())
{
    for (auto& s : paramSpecs())
    {
        paramPointers.emplace_back (apvts.getRawParameterValue (s.id), &s);
        apvts.addParameterListener (s.id, this);
    }
    lastSent.assign (paramPointers.size(), std::numeric_limits<float>::quiet_NaN());
    storePoints (shapePoints (0));
    loadPreset (1); // start on "Classic Pump", like the prototype
}

Pump96Processor::~Pump96Processor()
{
    for (auto& s : paramSpecs()) apvts.removeParameterListener (s.id, this);
}

bool Pump96Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void Pump96Processor::storePoints (const Pt* pts, int n) noexcept
{
    n = juce::jlimit (2, kMaxPoints, n);
    {
        const juce::SpinLock::ScopedLockType lock (pointsLock);
        numPoints = n;
        std::copy (pts, pts + n, points.begin());
    }
    ++pointsVersion;
}

std::vector<Pt> Pump96Processor::getPoints() const
{
    const juce::SpinLock::ScopedLockType lock (pointsLock);
    return { points.begin(), points.begin() + numPoints };
}

void Pump96Processor::setPointsFromEditor (const std::vector<Pt>& pts)
{
    storePoints (pts);
    if (auto* p = apvts.getParameter ("shape"))
        if (juce::roundToInt (apvts.getRawParameterValue ("shape")->load()) != kCustomShape)
        {
            loadingPreset = true;    // the shape change itself must not reload preset points
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) kCustomShape));
            p->endChangeGesture();
            loadingPreset = false;
        }
    presetModified = true;
}

void Pump96Processor::prepareToPlay (double sampleRate, int)
{
    for (size_t i = 0; i < paramPointers.size(); ++i)
    {
        const auto v = paramPointers[i].first->load();
        engine.setParam (paramPointers[i].second->engineIndex, v);
        lastSent[i] = v;
    }
    audioPointsVersion = -1;
    engine.prepare (sampleRate);
}

void Pump96Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (size_t i = 0; i < paramPointers.size(); ++i)
    {
        const auto v = paramPointers[i].first->load();
        if (! juce::exactlyEqual (v, lastSent[i]))
        {
            engine.setParam (paramPointers[i].second->engineIndex, v);
            lastSent[i] = v;
        }
    }

    // the drawn curve, when it changed (never waits for the GUI)
    const int ver = pointsVersion.load();
    if (ver != audioPointsVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (pointsLock);
        if (lock.isLocked())
        {
            const int np = numPoints;
            std::copy (points.begin(), points.begin() + np, audioPoints.begin());
            engine.setPoints (audioPoints.data(), np);
            audioPointsVersion = ver;
        }
    }

    // song position and tempo from the host; free-running when the host is stopped or gives no position
    bool playing = false; double ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) if (*bpm > 1.0) lastBpm = juce::jlimit (20.0, 400.0, *bpm);
            if (auto p = pos->getPpqPosition()) { ppq = *p; playing = pos->getIsPlaying(); }
        }
    engine.setTransport (playing, ppq, lastBpm);
    hostBpm = lastBpm; hostPlaying = playing;

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : l;
    const float* inR = getTotalNumInputChannels() > 1 ? r : l; // mono in -> stereo out
    engine.process (l, inR, l, r, n);

    phase = (float) engine.phase; gain = (float) engine.gain;
    inPeak = juce::jmax (inPeak.load(), (float) engine.inPeak);
    outPeak = juce::jmax (outPeak.load(), (float) engine.outPeak);
    engine.inPeak = engine.outPeak = 0;
}

void Pump96Processor::parameterChanged (const juce::String& id, float v)
{
    // choosing a preset shape loads its curve (the engine does the same on the audio thread)
    if (id == "shape" && ! loadingPreset && juce::roundToInt (v) < kNumShapes)
    {
        const auto& sp = shapePoints (juce::roundToInt (v));    // may run on the audio thread (automation): no allocation
        storePoints (sp.data(), (int) sp.size());
    }
    if (! loadingPreset && id != "bypass") presetModified = true;
}

const juce::String Pump96Processor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void Pump96Processor::loadPreset (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    loadingPreset = true;
    const auto& pr = presets[(size_t) index];
    for (auto& s : paramSpecs())
    {
        if (s.id == "bypass") continue;
        float v = s.def;
        for (auto& kv : pr.values) if (kv.first == s.id) v = kv.second;
        if (auto* p = apvts.getParameter (s.id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    }
    const int shape = juce::roundToInt (apvts.getRawParameterValue ("shape")->load());
    storePoints (pr.points.empty() ? shapePoints (juce::jmin (shape, kNumShapes - 1)) : pr.points);
    currentPreset = index;
    loadingPreset = false;
    presetModified = false;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void Pump96Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetModified", presetModified.load(), nullptr);
    state.setProperty ("points", pointsToString (getPoints()), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void Pump96Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            loadingPreset = true;
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) tree.getProperty ("preset", 1);
            apvts.replaceState (tree);
            const auto pts = tree.getProperty ("points").toString();
            const int shape = juce::roundToInt (apvts.getRawParameterValue ("shape")->load());
            storePoints (pts.isNotEmpty() ? pointsFromString (pts) : shapePoints (juce::jmin (shape, kNumShapes - 1)));
            presetModified = (bool) tree.getProperty ("presetModified", false);
            loadingPreset = false;
        }
}

juce::AudioProcessorEditor* Pump96Processor::createEditor() { return new Pump96Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Pump96Processor(); }
