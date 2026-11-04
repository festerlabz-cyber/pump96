// BoomTown Beats Pump 96 — DSP engine.
// Straight C++ port of the browser prototype (prototype/dsp.js). No JUCE dependency, so it can be tested on its own.
// A tempo-synced volume curve ("sidechain pump" without a sidechain): the user draws the curve, it repeats every Rate
// (1/32 .. 2 bars), Depth scales it, Offset shifts its phase, Smooth rounds its corners, and Band picks whether it works
// on the whole signal, only the lows or only the highs (Linkwitz-Riley split, the bands sum to an allpass).
// Transport: the host gives the beat position (ppq) per block; when the host is stopped the curve free-runs.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace pm96
{
constexpr double kTau = 6.283185307179586476925286766559;

inline double dbl (double d) noexcept { return std::pow (10.0, d / 20.0); }
inline double clampd (double v, double a, double b) noexcept { return v < a ? a : (v > b ? b : v); }

//==============================================================================
namespace P
{
    enum Id { depth, rate, shape, smooth, offset, band, split, out, bypass, total };
}

struct ParamInfo { double def; bool logSmooth; bool noSmooth; };
inline const std::array<ParamInfo, P::total>& paramInfo()
{
    static const std::array<ParamInfo, P::total> t {{
        { 80, false, false },     // depth %
        { 3, false, true },       // rate index (1/32 .. 2/1)
        { 0, false, true },       // shape index (Classic .. Bounce, Custom)
        { 2, false, false },      // smooth ms
        { 0, false, false },      // offset % of the period
        { 0, false, true },       // band (Full, Lows, Highs)
        { 150, true, false },     // split Hz
        { 0, false, false },      // output dB
        { 0, false, false },      // bypass
    }};
    return t;
}

constexpr const char* kRateNames[7] = { "1/32", "1/16", "1/8", "1/4", "1/2", "1/1", "2/1" };
constexpr double kRateBeats[7] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
constexpr const char* kBandNames[3] = { "Full", "Lows", "Highs" };
constexpr const char* kShapeNames[7] = { "Classic", "Smooth", "Hard", "Gate", "Ramp", "Bounce", "Custom" };
constexpr int kNumShapes = 6, kCustomShape = 6, kMaxPoints = 64, kLut = 512;

struct Pt { double x, y; };
// curve points: x in 0..1 (one Rate period), y = gain 0..1; x of the first/last point is fixed at 0/1
inline const std::vector<Pt>& shapePoints (int shape)
{
    static const std::vector<Pt> s[kNumShapes] = {
        { {0,0},{0.04,0.05},{0.1,0.35},{0.2,0.68},{0.32,0.86},{0.5,0.96},{1,1} },     // Classic
        { {0,0.1},{0.12,0.2},{0.3,0.55},{0.5,0.82},{0.7,0.95},{1,1} },                // Smooth
        { {0,0},{0.18,0},{0.24,0.9},{0.4,1},{1,1} },                                  // Hard
        { {0,1},{0.5,1},{0.5,0},{1,0} },                                              // Gate
        { {0,0},{1,1} },                                                              // Ramp
        { {0,1},{0.25,0.3},{0.5,1},{0.75,0.3},{1,1} },                                // Bounce
    };
    return s[std::clamp (shape, 0, kNumShapes - 1)];
}

// clamp to 0..1, sort by x (stable, so equal x keep their order), pin the end points, at most kMaxPoints
inline std::vector<Pt> sanitize (std::vector<Pt> p)
{
    for (auto& q : p) { q.x = clampd (std::isfinite (q.x) ? q.x : 0.0, 0.0, 1.0); q.y = clampd (std::isfinite (q.y) ? q.y : 0.0, 0.0, 1.0); }
    std::stable_sort (p.begin(), p.end(), [] (const Pt& a, const Pt& b) { return a.x < b.x; });
    if ((int) p.size() > kMaxPoints) p.resize ((size_t) kMaxPoints);
    if (p.size() < 2) return { {0, 1}, {1, 1} };
    p.front().x = 0.0; p.back().x = 1.0;
    return p;
}

//==============================================================================
using Coef  = std::array<double, 5>;
using State = std::array<double, 2>;
inline double bq (const Coef& c, State& s, double x) noexcept
{
    const double y = c[0] * x + s[0];
    s[0] = c[1] * x - c[3] * y + s[1];
    s[1] = c[2] * x - c[4] * y;
    return y;
}
inline void setHP2 (Coef& o, double fs, double f, double q) noexcept
{
    f = clampd (f, 5.0, fs * 0.45);
    const double w = kTau * f / fs, sn = std::sin (w), cs = std::cos (w), al = sn / (2.0 * q), a0 = 1.0 + al;
    o = { (1.0 + cs) / 2.0 / a0, -(1.0 + cs) / a0, (1.0 + cs) / 2.0 / a0, -2.0 * cs / a0, (1.0 - al) / a0 };
}
inline void setLP2 (Coef& o, double fs, double f, double q) noexcept
{
    f = clampd (f, 20.0, fs * 0.45);
    const double w = kTau * f / fs, sn = std::sin (w), cs = std::cos (w), al = sn / (2.0 * q), a0 = 1.0 + al;
    o = { (1.0 - cs) / 2.0 / a0, (1.0 - cs) / a0, (1.0 - cs) / 2.0 / a0, -2.0 * cs / a0, (1.0 - al) / a0 };
}

//==============================================================================
class Engine
{
public:
    Engine()
    {
        const auto& info = paramInfo();
        for (int i = 0; i < P::total; ++i) tgt[(size_t) i] = cur[(size_t) i] = info[(size_t) i].def;
        setPoints (shapePoints (0).data(), (int) shapePoints (0).size());
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        sa = 1.0 - std::exp (-kBlock / (fs * 0.025));
        reset();
    }

    void reset()
    {
        for (int i = 0; i < P::total; ++i) cur[(size_t) i] = tgt[(size_t) i];
        for (auto* arr : { sLP, sHP }) for (int c = 0; c < 2; ++c) { arr[c][0] = {}; arr[c][1] = {}; }
        g = 1.0;
        design(); derive();
    }

    // like the prototype: choosing a preset shape loads its points
    void setParam (int index, double value) noexcept
    {
        if (index == P::shape && value != tgt[P::shape] && (int) value < kNumShapes)
            setPoints (shapePoints ((int) value).data(), (int) shapePoints ((int) value).size());
        tgt[(size_t) index] = value;
    }

    // points must already be sanitised (sorted, ends at x = 0 and 1); builds the lookup table, no allocation
    void setPoints (const Pt* pts, int n) noexcept
    {
        n = std::clamp (n, 2, kMaxPoints);
        int j = 0;
        for (int i = 0; i <= kLut; ++i)
        {
            const double x = i / (double) kLut;
            while (j < n - 2 && pts[j + 1].x <= x) ++j;
            const Pt a = pts[j], b = pts[j + 1];
            const double dx = b.x - a.x;
            lut[(size_t) i] = dx <= 1e-9 ? b.y : a.y + (b.y - a.y) * clampd ((x - a.x) / dx, 0.0, 1.0);
        }
    }

    // ppq = beat position at the start of the next block, playing = false when the host is stopped
    void setTransport (bool playing, double ppqNow, double bpmNow) noexcept
    {
        if (bpmNow > 20.0 && bpmNow < 999.0) bpm = bpmNow;
        isPlaying = playing;
        if (playing) ppq = ppqNow;
    }

    // meters / display, read by the caller
    double inPeak = 0, outPeak = 0, phase = 0, gain = 1;

    void process (const float* iL, const float* iR, float* oL, float* oR, int n) noexcept
    {
        const double dpq = bpm / 60.0 / fs;
        double pos = isPlaying ? ppq : freePpq;
        for (int b0 = 0; b0 < n; b0 += kBlock)
        {
            const int e = std::min (n, b0 + kBlock);
            if (smooth()) design();
            derive();
            for (int s = b0; s < e; ++s)
            {
                double ph = pos / period + off; ph -= std::floor (ph);
                const double target = 1.0 - depth * (1.0 - curveAt (ph));
                g += (target - g) * kSm;
                pos += dpq;
                const double inL = iL[s], inR = iR[s];
                double yl, yr;
                if (! split) { yl = inL * g; yr = inR * g; }
                else
                {
                    const double lol = bq (cLP, sLP[0][1], bq (cLP, sLP[0][0], inL)), lor = bq (cLP, sLP[1][1], bq (cLP, sLP[1][0], inR));
                    const double hil = bq (cHP, sHP[0][1], bq (cHP, sHP[0][0], inL)), hir = bq (cHP, sHP[1][1], bq (cHP, sHP[1][0], inR));
                    const double gl = applyLo ? g : 1.0, gh = applyHi ? g : 1.0;
                    yl = lol * gl + hil * gh; yr = lor * gl + hir * gh;
                }
                yl *= outG; yr *= outG;
                yl += (inL - yl) * byp; yr += (inR - yr) * byp;
                oL[s] = (float) yl; oR[s] = (float) yr;
                inPeak = std::max (inPeak, std::max (std::abs (inL), std::abs (inR)));
                outPeak = std::max (outPeak, std::max (std::abs (yl), std::abs (yr)));
                phase = ph;
            }
        }
        ppq = pos; freePpq = pos;      // when the host stops, the curve free-runs from here
        gain = g;
    }

    double curveAt (double ph) const noexcept
    {
        const double x = ph * kLut;
        const int i = std::min (kLut - 1, (int) std::floor (x));
        const double f = x - i;
        return lut[(size_t) i] + (lut[(size_t) i + 1] - lut[(size_t) i]) * f;
    }

private:
    static constexpr int kBlock = 32;

    double fs = 48000.0, sa = 0.0;
    std::array<double, P::total> tgt {}, cur {};
    std::array<double, kLut + 1> lut {};
    Coef cLP {}, cHP {};
    State sLP[2][2] {}, sHP[2][2] {};
    double bpm = 120.0, ppq = 0.0, freePpq = 0.0;
    bool isPlaying = false;
    double g = 1.0, depth = 0.8, period = 1.0, off = 0.0, kSm = 1.0, outG = 1.0, byp = 0.0;
    bool split = false, applyLo = true, applyHi = true;

    bool smooth() noexcept
    {
        const auto& info = paramInfo();
        bool moved = false;
        for (int k = 0; k < P::total; ++k)
        {
            const auto& pi = info[(size_t) k];
            const double target = tgt[(size_t) k], v = cur[(size_t) k];
            if (v == target) continue;
            double nv;
            if (pi.noSmooth) nv = target;
            else if (pi.logSmooth) { nv = v * std::pow (target / v, sa); if (std::abs (nv / target - 1.0) < 1e-4) nv = target; }
            else { nv = v + (target - v) * sa; if (std::abs (nv - target) < 1e-5) nv = target; }
            cur[(size_t) k] = nv;
            moved = true;
        }
        return moved;
    }

    void design() noexcept { setLP2 (cLP, fs, cur[P::split], 0.7071); setHP2 (cHP, fs, cur[P::split], 0.7071); }

    void derive() noexcept
    {
        depth = cur[P::depth] / 100.0;
        period = kRateBeats[std::clamp ((int) cur[P::rate], 0, 6)];
        off = cur[P::offset] / 100.0;
        kSm = cur[P::smooth] <= 0.01 ? 1.0 : 1.0 - std::exp (-1.0 / (fs * cur[P::smooth] / 1000.0));
        outG = dbl (cur[P::out]); byp = cur[P::bypass];
        const int b = (int) cur[P::band];
        split = b != 0;
        applyLo = b != 2; applyHi = b != 1;
    }
};
} // namespace pm96
