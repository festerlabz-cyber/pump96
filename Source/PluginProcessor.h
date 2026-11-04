#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Pump96DSP.h"
#include "Parameters.h"

class Pump96Processor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    Pump96Processor();
    ~Pump96Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) pm96::factoryPresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    void loadPreset (int index);
    int getPresetIndex() const noexcept { return currentPreset; }
    bool isPresetModified() const noexcept { return presetModified.load(); }

    // the drawn curve (message thread); setting it from the editor switches Shape to Custom
    std::vector<pm96::Pt> getPoints() const;
    void setPointsFromEditor (const std::vector<pm96::Pt>& pts);
    int getPointsVersion() const noexcept { return pointsVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<float> phase { 0.0f }, gain { 1.0f }, inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<bool> hostPlaying { false };

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void storePoints (const pm96::Pt* pts, int n) noexcept;        // pts already sanitised; no allocation
    void storePoints (const std::vector<pm96::Pt>& pts) { auto clean = pm96::sanitize (pts); storePoints (clean.data(), (int) clean.size()); }

    pm96::Engine engine;
    std::vector<std::pair<std::atomic<float>*, const pm96::ParamSpec*>> paramPointers;
    std::vector<float> lastSent;
    int currentPreset = 1;
    std::atomic<bool> presetModified { false };
    bool loadingPreset = false;

    // curve hand-over: written under the lock by the message thread, copied by the audio thread with tryLock
    juce::SpinLock pointsLock;
    std::array<pm96::Pt, pm96::kMaxPoints> points {};
    int numPoints = 0;
    std::atomic<int> pointsVersion { 0 };
    int audioPointsVersion = -1;
    std::array<pm96::Pt, pm96::kMaxPoints> audioPoints {};
    double lastBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pump96Processor)
};
