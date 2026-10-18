// JUCE-free DSP tests for KITSUNE Auto Tune. Build: see CMakeLists.txt (target KitsuneDSPTests).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include "../Source/DSP/PitchEngine.h"
#include "../Source/DSP/DynamicResonanceReducer.h"
#include "../Source/DSP/ToneProcessor.h"

using namespace kitsune;
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<float> sine (double f, double sr, int n, float amp = 0.5f)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2.0 * M_PI * f * i / sr);
    return v;
}
static std::vector<float> harmonic (double f, double sr, int n, float amp = 0.3f)
{
    std::vector<float> v ((size_t) n, 0.0f);
    for (int h = 1; h <= 10; ++h)
        for (int i = 0; i < n; ++i) v[(size_t) i] += amp / (float) h * (float) std::sin (2.0 * M_PI * f * h * i / sr);
    return v;
}
static double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double e = 0; for (size_t i = a; i < b; ++i) e += (double) v[i] * v[i]; return std::sqrt (e / (double) (b - a));
}
static double goertzel (const std::vector<float>& x, size_t a, size_t b, double f, double sr)
{
    double w = 2 * M_PI * f / sr, c = 2 * std::cos (w), s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i) { double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a);
}
static double measure (const std::vector<float>& x, double sr)
{
    PitchDetector d; d.prepare (sr, 65.0f, 1000.0f); d.configure (65.0f, 1.5f);
    double acc = 0; int cnt = 0;
    const size_t start = x.size() / 2;
    for (size_t i = 0; i < x.size(); i += 64)
    {
        const int len = (int) std::min<size_t> (64, x.size() - i);
        if (d.process (x.data() + i, len) && i > start && d.result().voiced) { acc += std::log2 (d.result().frequency); ++cnt; }
    }
    return cnt ? std::exp2 (acc / cnt) : 0.0;
}

static void testDetection()
{
    std::printf("[pitch detection accuracy]\n");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (double f : { 82.4, 110.0, 220.0, 440.0, 880.0 })
            for (int mode = 0; mode < 2; ++mode)
            {
                if (mode == 0 && f < 100.0) continue;
                PitchDetector d; d.prepare (sr, 65.0f, 1000.0f);
                d.configure (mode == 0 ? 100.0f : 65.0f, mode == 0 ? 1.0f : 1.5f);
                auto x = sine (f, sr, (int) sr);
                double acc = 0; int cnt = 0;
                for (size_t i = 0; i < x.size(); i += 64)
                    if (d.process (x.data() + i, 64) && i > x.size() / 2 && d.result().voiced) { acc += d.result().frequency; ++cnt; }
                const double est = cnt ? acc / cnt : 0;
                CHECK (cnt > 0 && std::fabs (cents (est, f)) < 12.0, "sr=%.0f f=%.1f mode=%d est=%.2f (%.1f ct)", sr, f, mode, est, cnt ? cents (est, f) : 999.0);
            }
    // harmonically rich: no octave errors
    for (double f : { 120.0, 196.0, 330.0 })
    {
        auto x = harmonic (f, 48000, 48000);
        const double est = measure (x, 48000);
        CHECK (std::fabs (cents (est, f)) < 15.0, "harmonic f=%.0f est=%.2f", f, est);
    }
}

static void testSilenceAndNoise()
{
    std::printf("[silence / unvoiced]\n");
    PitchDetector d; d.prepare (48000, 65.0f, 1000.0f); d.configure (65.0f, 1.5f);
    std::vector<float> z (48000, 0.0f);
    int voiced = 0;
    for (size_t i = 0; i < z.size(); i += 64) if (d.process (z.data() + i, 64) && d.result().voiced) ++voiced;
    CHECK (voiced == 0, "silence flagged voiced %d times", voiced);

    std::mt19937 rng (1); std::normal_distribution<float> nd (0.0f, 0.2f);
    std::vector<float> nz (96000); for (auto& s : nz) s = nd (rng);
    int frames = 0; voiced = 0; d.reset();
    for (size_t i = 0; i < nz.size(); i += 64) if (d.process (nz.data() + i, 64)) { ++frames; if (d.result().voiced) ++voiced; }
    CHECK (voiced < frames / 5, "noise voiced %d / %d", voiced, frames);
}

static void testQuantisation()
{
    std::printf("[scale quantisation]\n");
    PitchCorrector pc; CorrectorParams p; p.speedMs = 0; p.strength = 1; p.vibrato = 0;
    p.mask = PitchCorrector::maskForScale (1, 0);   // C major
    PitchResult r; r.voiced = true; r.confidence = 1;
    r.frequency = 440.0f * std::exp2 ((61.4f - 69.0f) / 12.0f);
    float s = 0; for (int i = 0; i < 50; ++i) s = pc.update (r, p, 0.0027f);
    CHECK (std::fabs (pc.targetMidi() - 62.0f) < 1e-3f, "target %.2f (expected D4=62)", pc.targetMidi());
    CHECK (std::fabs (s - 0.6f) < 0.02f, "shift %.3f (expected +0.6)", s);

    // excluded notes are never targeted
    p.mask = PitchCorrector::maskForScale (0, 0) & (std::uint16_t) ~(1u << 2);   // chromatic without D
    pc.reset(); for (int i = 0; i < 50; ++i) s = pc.update (r, p, 0.0027f);
    CHECK (((int) pc.targetMidi() % 12) != 2, "excluded D was targeted (%.1f)", pc.targetMidi());

    // all scales: target always a member of the scale
    for (int sc = 0; sc < PitchCorrector::numScales(); ++sc)
        for (int root = 0; root < 12; ++root)
        {
            p.mask = PitchCorrector::maskForScale (sc, root);
            for (float m = 48.0f; m < 72.0f; m += 0.37f)
            {
                pc.reset(); r.frequency = 440.0f * std::exp2 ((m - 69.0f) / 12.0f);
                for (int i = 0; i < 20; ++i) pc.update (r, p, 0.0027f);
                const int pcn = ((int) std::lround (pc.targetMidi()) % 12 + 12) % 12;
                CHECK ((p.mask >> pcn) & 1, "scale %d root %d note %.2f target pc %d not in scale", sc, root, m, pcn);
            }
        }
}

static std::vector<float> runEngine (const std::vector<float>& in, double sr, int mode, const EngineParams& ep, int block = 512)
{
    PitchEngine e; e.prepare (sr, 1); e.setMode (mode);
    std::vector<float> out = in;
    for (size_t i = 0; i < out.size(); i += (size_t) block)
    {
        float* ch[1] = { out.data() + i };
        e.process (ch, 1, (int) std::min<size_t> ((size_t) block, out.size() - i), ep);
    }
    return out;
}

static void testEndToEnd()
{
    std::printf("[end-to-end correction]\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int mode = 0; mode < 2; ++mode)
        {
            EngineParams ep; ep.corrector.speedMs = 0; ep.corrector.strength = 1; ep.corrector.vibrato = 0;
            ep.corrector.mask = PitchCorrector::maskForScale (1, 0);
            const double fIn = 440.0 * std::exp2 ((61.4 - 69.0) / 12.0), fTarget = 440.0 * std::exp2 ((62.0 - 69.0) / 12.0);
            for (int rich = 0; rich < 2; ++rich)
            {
                auto in = rich ? harmonic (fIn, sr, (int) (2 * sr)) : sine (fIn, sr, (int) (2 * sr));
                auto out = runEngine (in, sr, mode, ep);
                std::vector<float> tail (out.begin() + (long) sr, out.end());
                const double est = measure (tail, sr);
                CHECK (std::fabs (cents (est, fTarget)) < 15.0, "sr=%.0f mode=%d rich=%d out=%.2f Hz target=%.2f (%.1f ct)", sr, mode, rich, est, fTarget, cents (est, fTarget));
                const double ratio = rms (out, (size_t) sr, out.size()) / rms (in, (size_t) sr, in.size());
                CHECK (ratio > 0.7 && ratio < 1.4, "sr=%.0f mode=%d rich=%d level ratio %.2f", sr, mode, rich, ratio);
            }
        }
}

static void testLatency()
{
    std::printf("[latency]\n");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int mode = 0; mode < 2; ++mode)
        {
            PitchEngine e; e.prepare (sr, 1); e.setMode (mode);
            const int lat = e.latencySamples();
            std::vector<float> x ((size_t) (lat + 4000), 0.0f); x[100] = 1.0f;
            EngineParams ep;
            float* ch[1] = { x.data() };
            e.process (ch, 1, (int) x.size(), ep);
            size_t peak = 0; for (size_t i = 0; i < x.size(); ++i) if (std::fabs (x[i]) > std::fabs (x[peak])) peak = i;
            CHECK ((int) peak == 100 + lat, "sr=%.0f mode=%d reported %d, measured %d", sr, mode, lat, (int) peak - 100);
            CHECK (lat == PitchEngine::latencyFor (sr, mode), "latencyFor mismatch");
            std::printf ("  sr=%6.0f %-11s latency %5d samples = %5.2f ms\n", sr, mode == 0 ? "LowLatency" : "Studio", lat, 1000.0 * lat / sr);
        }
}

static void testContinuity()
{
    std::printf("[continuity under parameter change]\n");
    const double sr = 48000;
    EngineParams ep; ep.corrector.mask = PitchCorrector::maskForScale (1, 0); ep.corrector.speedMs = 20;
    PitchEngine e; e.prepare (sr, 1);
    auto in = sine (233.0, sr, 96000);
    float maxStep = 0, prev = 0;
    for (size_t i = 0; i < in.size(); i += 256)
    {
        if (i == 48000) { ep.formantSemitones = 3.0f; ep.corrector.strength = 0.5f; ep.keepFormants = false; }
        if (i == 72000) { ep.corrector.refA = 450.0f; ep.corrector.mask = PitchCorrector::maskForScale (11, 4); }
        float* ch[1] = { in.data() + i };
        e.process (ch, 1, 256, ep);
        if (i > 20000) for (int k = 0; k < 256; ++k) { maxStep = std::max (maxStep, std::fabs (in[i + (size_t) k] - prev)); prev = in[i + (size_t) k]; }
        else prev = in[i + 255];
    }
    std::printf ("  max sample step %.3f\n", maxStep);
    CHECK (maxStep < 0.35f, "discontinuity detected: %.3f", maxStep);
}

static void testResonance()
{
    std::printf("[dynamic resonance reducer]\n");
    const double sr = 48000; const int n = (int) sr * 2;
    std::mt19937 rng (3); std::normal_distribution<float> nd (0.0f, 0.05f);
    std::vector<float> a (n), b;
    for (int i = 0; i < n; ++i) a[(size_t) i] = nd (rng) + 0.15f * (float) std::sin (2 * M_PI * 6000.0 * i / sr);
    b = a;
    DynamicResonanceReducer r; r.prepare (sr, 1);
    ResonanceParams p; p.enabled = true; p.sensitivity = 0.7f; p.amount = 0.8f;
    for (size_t i = 0; i < b.size(); i += 128) { float* ch[1] = { b.data() + i }; r.process (ch, 1, 128, p); }
    const double before = goertzel (a, n / 2, n, 6000, sr), after = goertzel (b, n / 2, n, 6000, sr);
    std::printf ("  6 kHz resonance: %.1f dB reduction\n", 20 * std::log10 (before / after));
    CHECK (20 * std::log10 (before / after) > 4.0, "resonance not reduced enough");
    // disabled => bit-exact pass-through
    std::vector<float> c = a; DynamicResonanceReducer r2; r2.prepare (sr, 1); p.enabled = false;
    for (size_t i = 0; i < c.size(); i += 128) { float* ch[1] = { c.data() + i }; r2.process (ch, 1, 128, p); }
    CHECK (c == a, "disabled reducer altered signal");
    // flat broadband material (no resonance) should be barely touched
    std::vector<float> w (n); for (auto& s : w) s = nd (rng);
    auto w2 = w; DynamicResonanceReducer r3; r3.prepare (sr, 1); p.enabled = true;
    for (size_t i = 0; i < w2.size(); i += 128) { float* ch[1] = { w2.data() + i }; r3.process (ch, 1, 128, p); }
    CHECK (std::fabs (20 * std::log10 (rms (w2, n / 2, n) / rms (w, n / 2, n))) < 2.0, "white noise changed by >2 dB");
}

static void testCpu()
{
    std::printf("[cpu / real-time]\n");
    const double sr = 48000; const int n = (int) sr * 10;
    PitchEngine e; e.prepare (sr, 2); e.setMode (1);
    DynamicResonanceReducer dr; dr.prepare (sr, 2); ToneProcessor tp; tp.prepare (sr, 2);
    auto l = harmonic (180.0, sr, n), r = l;
    EngineParams ep; ep.corrector.mask = PitchCorrector::maskForScale (1, 0);
    ResonanceParams rp; rp.enabled = true; ToneParams tpar; tpar.hpfHz = 80; tpar.lowDb = 2;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; i += 512)
    {
        float* ch[2] = { l.data() + i, r.data() + i };
        e.process (ch, 2, std::min (512, n - i), ep);
        tp.process (ch, 2, std::min (512, n - i), tpar);
        dr.process (ch, 2, std::min (512, n - i), rp);
    }
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("  10 s stereo studio-mode audio processed in %.3f s  => %.2f %% of one core\n", sec, 100.0 * sec / 10.0);
    CHECK (sec / 10.0 < 0.25, "CPU load too high");
}

int main()
{
    testDetection(); testSilenceAndNoise(); testQuantisation(); testEndToEnd();
    testLatency(); testContinuity(); testResonance(); testCpu();
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
