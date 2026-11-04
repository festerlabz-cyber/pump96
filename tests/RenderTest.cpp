// Offline check: runs pad + kick through the real plug-in processor (all presets) with a fake host playing at 160 BPM,
// checks that the curve sits on the host's beat grid, that custom points survive a state round trip, and renders the
// editor to PNG.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

struct FakeHost : juce::AudioPlayHead
{
    double bpm = 160.0, ppq = 0.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setIsPlaying (playing); p.setPpqPosition (ppq); return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const double sr = 48000.0; const int block = 256;

    auto proc = std::make_unique<Pump96Processor>();
    FakeHost host; proc->setPlayHead (&host);
    proc->setPlayConfigDetails (2, 2, sr, block);
    proc->prepareToPlay (sr, block);
    auto set = [&] (const char* id, float v) { if (auto* p = proc->apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };

    auto run = [&] (int blocks, auto&& fill, auto&& onOut)
    {
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            fill (buf);
            proc->processBlock (buf, midi);
            onOut (buf);
            host.ppq += block * host.bpm / 60.0 / sr;
        }
    };

    for (int preset = 0; preset < proc->getNumPrograms(); ++preset)
    {
        proc->setCurrentProgram (preset);
        host.ppq = 0; int n = 0; double inSum = 0, sum = 0, peak = 0; bool bad = false;
        run ((int) (sr * 3 / block), [&] (juce::AudioBuffer<float>& buf) {
            for (int i = 0; i < block; ++i, ++n) { const double t = n / sr; const float x = (float) (0.3 * std::sin (2 * juce::MathConstants<double>::pi * 220 * t) + 0.3 * std::sin (2 * juce::MathConstants<double>::pi * 55 * t));
                                                   buf.setSample (0, i, x); buf.setSample (1, i, x); inSum += x * x; } },
            [&] (juce::AudioBuffer<float>& buf) { for (int i = 0; i < block; ++i) { const float y = buf.getSample (0, i); if (! std::isfinite (y)) bad = true; sum += y * y; peak = std::max (peak, (double) std::abs (y)); } });
        std::printf ("%-18s rms %6.1f dB (in %6.1f)  peak %6.1f dBFS  points %d%s\n", proc->getProgramName (preset).toRawUTF8(),
                     10 * std::log10 (sum / n), 10 * std::log10 (inSum / n), 20 * std::log10 (peak), (int) proc->getPoints().size(), bad ? "  NaN!" : "");
    }

    // beat grid: Ramp, Depth 100 %, Smooth off, 1/4 at 160 BPM; host starts at ppq 10.25 -> gain = 0.25 at the start,
    // 0 again exactly on the next beat (ppq 11 = 0.28125 s later)
    {
        proc->setCurrentProgram (0);
        set ("shape", 4); set ("depth", 100); set ("smooth", 0); set ("rate", 3);
        proc->prepareToPlay (sr, block);
        host.ppq = 10.25; std::vector<float> out;
        run (64, [&] (juce::AudioBuffer<float>& buf) { for (int i = 0; i < block; ++i) { buf.setSample (0, i, 0.5f); buf.setSample (1, i, 0.5f); } },
             [&] (juce::AudioBuffer<float>& buf) { for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i) / 0.5f); });
        const int beatAt = (int) std::round (0.75 * 60.0 / 160.0 * sr);
        std::printf ("beat grid: gain at start %.3f (0.25), just before next beat %.3f (~1), on the beat %.3f (0), host tempo %.1f\n",
                     out[1], out[(size_t) beatAt - 2], out[(size_t) beatAt + 1], proc->hostBpm.load());
    }

    // custom points and state round trip
    {
        proc->setCurrentProgram (1);
        proc->setPointsFromEditor ({ { 0, 0.2 }, { 0.3, 0.9 }, { 0.6, 0.1 }, { 1, 1 } });
        juce::MemoryBlock mb; proc->getStateInformation (mb);
        proc->setCurrentProgram (3);
        proc->setStateInformation (mb.getData(), (int) mb.getSize());
        const auto p = proc->getPoints();
        std::printf ("state restore: shape %s, %d points, third point (%.2f, %.2f), modified %d\n",
                     pm96::kShapeNames[juce::roundToInt (proc->apvts.getRawParameterValue ("shape")->load())], (int) p.size(), p.size() > 2 ? p[2].x : -1.0, p.size() > 2 ? p[2].y : -1.0,
                     (int) proc->isPresetModified());
    }

    proc->setCurrentProgram (1);
    host.ppq = 0.3;
    run (4, [&] (juce::AudioBuffer<float>& buf) { for (int i = 0; i < block; ++i) { buf.setSample (0, i, 0.4f); buf.setSample (1, i, 0.4f); } }, [] (juce::AudioBuffer<float>&) {});
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
    for (int i = 0; i < 3; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    for (float sc : { 1.0f, 2.0f })
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, sc);
        juce::PNGImageFormat png; auto f = outDir.getChildFile ("pump96_gui_" + juce::String ((int) sc) + "x.png"); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
        std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    }
    ed.reset();
    proc->setPlayHead (nullptr);
    return 0;
}
