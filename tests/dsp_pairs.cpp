// Runs the C++ engine on a deterministic test signal with idx=value parameter pairs and writes L/R as float32;
// used to compare against prototype/dsp.js. Extra arguments: "bpm=…" (host tempo, playing from ppq 0, default 160),
// "stopped" (free-running) and "pts=x,y;x,y;…" (custom curve).
#include "../Source/Pump96DSP.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
int main (int argc, char** argv)
{
    const int SR = 48000, n = SR * 2, beat = (int) (0.35 * SR);
    std::vector<float> x ((size_t) n), xr ((size_t) n), l ((size_t) n), r ((size_t) n);
    uint32_t seed = 12345; double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const int j = i % beat; const double t = j / (double) SR; if (j == 0) ph = 0;
        ph += 2.0 * M_PI * (50.0 + 180.0 * std::exp (-t / 0.03)) / SR;
        seed = seed * 1664525u + 1013904223u;
        const double noise = seed / 4294967296.0 * 2.0 - 1.0;
        x[(size_t) i] = (float) (0.8 * std::exp (-t / 0.25) * std::sin (ph) + 0.05 * noise * std::exp (-t / 0.01) + 0.08 * std::sin (2.0 * M_PI * 330.0 * i / SR));
        xr[(size_t) i] = x[(size_t) i] * 0.7f;
    }
    pm96::Engine e; e.prepare (SR);
    double bpm = 160; bool playing = true;
    for (int a = 2; a < argc; ++a)
    {
        std::string s = argv[a];
        if (s == "stopped") { playing = false; continue; }
        auto p = s.find ('=');
        const std::string k = s.substr (0, p), v = s.substr (p + 1);
        if (k == "bpm") bpm = std::stod (v);
        else if (k == "pts")
        {
            std::vector<pm96::Pt> pts; size_t at = 0;
            while (at < v.size()) { auto semi = v.find (';', at); if (semi == std::string::npos) semi = v.size(); auto item = v.substr (at, semi - at); auto c = item.find (',');
                                    pts.push_back ({ std::stod (item.substr (0, c)), std::stod (item.substr (c + 1)) }); at = semi + 1; }
            pts = pm96::sanitize (pts); e.setPoints (pts.data(), (int) pts.size());
        }
        else e.setParam (std::stoi (k), std::stod (v));
    }
    double ppq = 0;
    for (int i = 0; i < n; i += 128)
    {
        const int m = std::min (128, n - i);
        e.setTransport (playing, ppq, bpm);
        e.process (x.data() + i, xr.data() + i, l.data() + i, r.data() + i, m);
        ppq += m * bpm / 60.0 / SR;
    }
    FILE* f = std::fopen (argv[1], "wb"); std::fwrite (l.data(), 4, (size_t) n, f); std::fwrite (r.data(), 4, (size_t) n, f); std::fclose (f);
    return 0;
}
