#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Yardımcı bloklar
// ---------------------------------------------------------------------------

// Kesirli okumalı gecikme hattı (lineer interpolasyon)
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;

    void prepare(int maxSamples) { buf.assign((size_t)maxSamples + 4, 0.0f); w = 0; }
    void push(float x) { buf[(size_t)w] = x; if (++w >= (int)buf.size()) w = 0; }

    // d >= 1: kaç örnek geriye doğru okunacağı
    float read(float d) const
    {
        const int size = (int)buf.size();
        float rp = (float)w - d;
        while (rp < 0.0f) rp += (float)size;
        int i0 = (int)rp;
        float f = rp - (float)i0;
        if (i0 >= size) i0 -= size;
        int i1 = i0 + 1; if (i1 >= size) i1 = 0;
        return buf[(size_t)i0] * (1.0f - f) + buf[(size_t)i1] * f;
    }
};

// Schroeder all-pass (giriş difüzyonu)
struct Allpass
{
    std::vector<float> buf;
    int len = 1, pos = 0;
    float g = 0.6f;

    void allocate(int maxN) { buf.assign((size_t)std::max(1, maxN), 0.0f); len = (int)buf.size(); pos = 0; }
    void setLength(int n)
    {
        len = std::clamp(n, 1, (int)buf.size());
        pos = 0;
        std::fill(buf.begin(), buf.end(), 0.0f);
    }
    float process(float x)
    {
        float d = buf[(size_t)pos];
        float out = -g * x + d;
        buf[(size_t)pos] = x + g * d;
        if (++pos >= len) pos = 0;
        return out;
    }
};

// Tek kutuplu filtre durumu
struct OnePole
{
    float z = 0.0f;
    float tick(float x, float a) { z += a * (x - z); return z; }
};

// Oktav yukarı pitch shifter (iki kanallı çapraz geçişli gecikme yöntemi)
struct PitchShifterUp
{
    DelayLine dl;
    float window = 2048.0f, phase = 0.0f, inc = 1.0f / 2048.0f;

    void prepare(float sr)
    {
        window = 0.05f * sr; // 50 ms pencere
        inc = 1.0f / window;
        dl.prepare((int)window + 8);
        phase = 0.0f;
    }
    float process(float x)
    {
        dl.push(x);
        phase -= inc; if (phase < 0.0f) phase += 1.0f;
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        float g1 = std::sin(3.14159265f * phase); g1 *= g1;
        float g2 = std::sin(3.14159265f * p2);    g2 *= g2;
        return dl.read(1.0f + phase * window) * g1 + dl.read(1.0f + p2 * window) * g2;
    }
};

// ---------------------------------------------------------------------------
// Reverb modları
// ---------------------------------------------------------------------------
struct ModeConfig
{
    float baseMs[8]; // FDN gecikme hatları (ms, Size=1 iken)
    float modMul;    // modülasyon çarpanı
    float apScale;   // giriş all-pass uzunluk çarpanı
};

inline const ModeConfig kModes[4] =
{
    // 0: Room - kısa, yoğun
    { { 7.1f, 9.3f, 11.7f, 13.9f, 16.1f, 18.7f, 21.3f, 23.9f }, 0.5f, 0.5f },
    // 1: Hall - klasik büyük salon
    { { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.3f }, 1.0f, 1.0f },
    // 2: Plate - parlak, yoğun, hareketli
    { { 19.3f, 23.1f, 27.7f, 31.3f, 35.9f, 41.9f, 47.3f, 53.1f }, 1.6f, 0.75f },
    // 3: Cathedral - çok geniş, uzun
    { { 61.3f, 73.1f, 87.7f, 97.9f, 109.3f, 121.1f, 133.7f, 149.9f }, 1.2f, 1.6f }
};

// ---------------------------------------------------------------------------
// Ana reverb
// ---------------------------------------------------------------------------
class FDNReverb
{
public:
    static constexpr int N = 8;

    struct Params
    {
        int   mode = 1;           // 0 Room, 1 Hall, 2 Plate, 3 Cathedral
        float mix = 0.3f;         // 0..1
        float decay = 3.0f;       // RT60 (s)
        float size = 1.0f;        // 0.5..2
        float preDelayMs = 10.0f; // ms
        float dampHz = 8000.0f;   // feedback lowpass
        float diffusion = 0.6f;   // 0..0.75
        float mod = 0.3f;         // 0..1
        float width = 1.0f;       // 0..1
        float lowCutHz = 20.0f;   // wet highpass
        float highCutHz = 20000.0f; // wet lowpass
        float shimmer = 0.0f;     // 0..1 (oktav yukarı geri besleme)
        float duck = 0.0f;        // 0..1
        float duckRelMs = 250.0f; // ms
        bool  freeze = false;
    };

    void prepare(double sampleRate)
    {
        sr = (float)sampleRate;
        for (auto& d : lines) d.prepare((int)(0.4 * sampleRate));
        preL.prepare((int)(0.25 * sampleRate));
        preR.prepare((int)(0.25 * sampleRate));

        const float scale = sr / 44100.0f;
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < 4; ++j)
                ap[c][j].allocate((int)((float)apBase[c][j] * scale * 1.6f) + 2);

        psL.prepare(sr);
        psR.prepare(sr);

        for (int i = 0; i < N; ++i)
        {
            lp[i] = 0.0f;
            phase[i] = (float)i * 0.785398f;
            phaseInc[i] = 6.2831853f * (0.2f + 0.13f * (float)i) / sr;
        }
        shHp1L = shHp2L = shHp1R = shHp2R = OnePole{};
        hp1L = hp2L = hp1R = hp2R = lp1L = lp2L = lp1R = lp2R = OnePole{};
        env = 0.0f; inGain = 1.0f; mixSm = p.mix;
        lastWl = lastWr = 0.0f;
        first = true;
    }

    void setParams(const Params& np) { p = np; }

    void process(float* L, float* R, int n)
    {
        if (first || p.mode != appliedMode) applyMode();
        const ModeConfig& cfg = kModes[appliedMode];
        const float twoPi = 6.2831853f;

        const float dampCoef = p.freeze ? 1.0f : 1.0f - std::exp(-twoPi * p.dampHz / sr);
        const float depth = p.mod * cfg.modMul * 0.0015f * sr;
        const float preSamples = std::max(1.0f, p.preDelayMs * 0.001f * sr);
                const bool useHP = p.lowCutHz > 21.0f;
        const bool useLP = p.highCutHz < 19900.0f;
        const float hpA = 1.0f - std::exp(-twoPi * p.lowCutHz / sr);
        const float lpA = 1.0f - std::exp(-twoPi * p.highCutHz / sr);
        const float atk = std::exp(-1.0f / (0.005f * sr));
        const float rel = std::exp(-1.0f / (std::max(1.0f, p.duckRelMs) * 0.001f * sr));
        const float inTarget = p.freeze ? 0.0f : 1.0f;
        const float shHpA = 1.0f - std::exp(-twoPi * 150.0f / sr);

        for (auto& c : ap) for (auto& a : c) a.g = p.diffusion;

        float g[N];
        float meanMs = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float lenSec = curMs[i] * 0.001f;
            meanMs += curMs[i] / (float)N;
            g[i] = p.freeze ? 0.99999f
                            : std::pow(10.0f, -3.0f * lenSec / std::max(0.05f, p.decay));
        }

        // Ağın yaklaşık genlik kazancı: uzun decay = daha yüksek birikim.
        // Giriş buna göre normalize edilir, shimmer döngü kazancı da 1'in altında tutulur.
        const float g2 = std::pow(10.0f, -6.0f * meanMs * 0.001f / std::max(0.05f, p.decay));
        const float netGain = std::sqrt(1.0f / std::max(0.01f, 1.0f - g2));
        const float inNorm = kInputTrim / netGain;
        const float shimK = p.shimmer * 0.7f / netGain;

        for (int s = 0; s < n; ++s)
        {
            for (int i = 0; i < N; ++i)
                curMs[i] += 0.0003f * (cfg.baseMs[i] * p.size - curMs[i]);
            mixSm += 0.001f * (p.mix - mixSm);
            inGain += 0.002f * (inTarget - inGain);

            const float dryL = L[s], dryR = R[s];

            // Ducking zarfı (kuru sinyalden)
            const float lvl = std::max(std::fabs(dryL), std::fabs(dryR));
            env = lvl > env ? atk * env + (1.0f - atk) * lvl : rel * env + (1.0f - rel) * lvl;
            const float duckGain = 1.0f - p.duck * std::min(1.0f, env * 4.0f);

            // Pre-delay
            preL.push(dryL);
            preR.push(dryR);
            float inL = preL.read(preSamples) * inGain * inNorm;
            float inR = preR.read(preSamples) * inGain * inNorm;

            // Shimmer: önceki wet çıkışının oktav yukarısı girişe geri beslenir
            float shL = psL.process(lastWl);
            float shR = psR.process(lastWr);
            // DC / çok düşük frekansları at (geri besleme döngüsünde birikmesin)
            shL -= shHp1L.tick(shL, shHpA); shL -= shHp2L.tick(shL, shHpA);
            shR -= shHp1R.tick(shR, shHpA); shR -= shHp2R.tick(shR, shHpA);
            inL += std::tanh(shL) * shimK * inGain;
            inR += std::tanh(shR) * shimK * inGain;

            // Giriş difüzyonu
            for (int j = 0; j < 4; ++j) { inL = ap[0][j].process(inL); inR = ap[1][j].process(inR); }

            // FDN
            float y[N];
            float sum = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                float m = depth * std::sin(phase[i]);
                phase[i] += phaseInc[i];
                if (phase[i] > twoPi) phase[i] -= twoPi;

                float d = std::max(2.0f, curMs[i] * 0.001f * sr + m);
                float v = lines[i].read(d);
                lp[i] += dampCoef * (v - lp[i]);
                y[i] = lp[i] * g[i];
                sum += y[i];
            }
            sum *= (2.0f / (float)N); // Householder karıştırma

            float wl = 0.0f, wr = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                float in = (i & 1) ? inR : inL;
                lines[i].push(in * 0.5f + (y[i] - sum));
                if (i & 1) wr += y[i]; else wl += y[i];
            }
            wl *= 0.5f; wr *= 0.5f;
            lastWl = wl; lastWr = wr;

            // Genişlik
            float mid = 0.5f * (wl + wr);
            float side = 0.5f * (wl - wr) * p.width;
            wl = mid + side; wr = mid - side;

            // Low cut / High cut (12 dB/oct)
            if (useHP)
            {
                wl -= hp1L.tick(wl, hpA); wl -= hp2L.tick(wl, hpA);
                wr -= hp1R.tick(wr, hpA); wr -= hp2R.tick(wr, hpA);
            }
            if (useLP)
            {
                wl = lp2L.tick(lp1L.tick(wl, lpA), lpA);
                wr = lp2R.tick(lp1R.tick(wr, lpA), lpA);
            }

            wl *= duckGain; wr *= duckGain;

            L[s] = dryL * (1.0f - mixSm) + wl * mixSm;
            R[s] = dryR * (1.0f - mixSm) + wr * mixSm;
        }
    }

private:
    void applyMode()
    {
        appliedMode = std::clamp(p.mode, 0, 3);
        const ModeConfig& cfg = kModes[appliedMode];
        const float scale = sr / 44100.0f;
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < 4; ++j)
                ap[c][j].setLength((int)((float)apBase[c][j] * scale * cfg.apScale));
        if (first)
            for (int i = 0; i < N; ++i) curMs[i] = cfg.baseMs[i] * p.size;
        first = false;
    }

    static constexpr float kInputTrim = 1.3f;
    static constexpr int apBase[2][4] = { {142, 107, 379, 277}, {151, 113, 389, 283} };

    float sr = 44100.0f;
    Params p;
    bool first = true;
    int appliedMode = 1;

    DelayLine lines[N];
    DelayLine preL, preR;
    Allpass ap[2][4];
    PitchShifterUp psL, psR;
    OnePole shHp1L, shHp2L, shHp1R, shHp2R;
    OnePole hp1L, hp2L, hp1R, hp2R, lp1L, lp2L, lp1R, lp2R;

    float lp[N] = {}, phase[N] = {}, phaseInc[N] = {}, curMs[N] = {};
    float mixSm = 0.3f, env = 0.0f, inGain = 1.0f, lastWl = 0.0f, lastWr = 0.0f;
};
