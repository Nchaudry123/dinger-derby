#pragma once

#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

// Ballpark SFX: bat crack (WAV or synthesis), swing whoosh, mitt pop,
// outfield wall bang, and crowd cheer swell. Prefers WAVs under assets/sfx/
// for the crack; everything else is synthesized at startup.
//
// NOTE: Real MLB broadcast audio is copyrighted — do not rip game/TV audio.
//   assets/sfx/bat_crack.wav       barrel / best contact
//   assets/sfx/bat_crack_solid.wav solid square-up
//   assets/sfx/bat_crack_soft.wav  mishit / light contact
namespace ProceduralSfx {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

inline float frand(std::uint32_t& rng) {
    rng = rng * 1664525u + 1013904223u;
    return (static_cast<float>(rng & 0xFFFFFFu) / static_cast<float>(0x1000000u)) * 2.0f - 1.0f;
}

inline float softClip(float x) {
    return std::tanh(x * 1.15f);
}

inline bool loadMonoWav(sf::SoundBuffer& buffer, const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    if (!buffer.loadFromFile(path)) {
        std::cerr << "ProceduralSfx: failed to load " << path << std::endl;
        return false;
    }
    return true;
}

inline std::filesystem::path resolveSfxPath(const char* filename) {
    const char* roots[] = {
        "assets/sfx",
        "../assets/sfx",
        "../../assets/sfx",
        "dinger-derby/assets/sfx",
    };
    for (const char* root : roots) {
        std::filesystem::path p = std::filesystem::path(root) / filename;
        if (std::filesystem::exists(p)) {
            return p;
        }
    }
    return std::filesystem::path("assets/sfx") / filename;
}

// Modal wood-bat crack: impulse + resonant filters (maple-like).
// quality: 0 = mishit, 1 = solid barrel.
inline bool synthesizeBatCrack(sf::SoundBuffer& buffer, float quality) {
    quality = std::clamp(quality, 0.0f, 1.0f);
    constexpr unsigned sampleRate = 44100;
    const float duration = quality > 0.55f ? 0.32f : 0.22f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<float> raw(n, 0.0f);
    std::uint32_t rng = 0xBADC0FFEu + static_cast<std::uint32_t>(quality * 997.0f);

    struct Mode {
        float b1 = 0;
        float a2 = 0;
        float amp = 0;
        float y1 = 0;
        float y2 = 0;
    };

    auto makeMode = [&](float freq, float amp, float damp) {
        Mode m;
        float r = std::exp(-damp / static_cast<float>(sampleRate));
        float w = 2.0f * kPi * freq / static_cast<float>(sampleRate);
        m.b1 = 2.0f * r * std::cos(w);
        m.a2 = r * r;
        m.amp = amp;
        return m;
    };

    const float q = quality;
    std::vector<Mode> modes = {
        makeMode(3550.0f + q * 1100.0f, 1.10f, 95.0f - q * 25.0f),
        makeMode(5200.0f + q * 800.0f, 0.70f, 110.0f - q * 20.0f),
        makeMode(7800.0f, 0.28f * q, 140.0f),
        makeMode(210.0f + q * 35.0f, 0.90f, 14.0f - q * 3.0f),
        makeMode(380.0f + q * 45.0f, 0.75f, 18.0f - q * 4.0f),
        makeMode(560.0f + q * 50.0f, 0.50f, 24.0f - q * 5.0f),
        makeMode(910.0f + q * 40.0f, 0.28f, 32.0f),
        makeMode(68.0f + q * 18.0f, 1.05f, 9.0f - q * 2.0f),
        makeMode(125.0f, 0.55f, 12.0f),
    };

    float hpX1 = 0.0f;
    float hpY1 = 0.0f;
    float peak = 1e-6f;
    const int burstN = static_cast<int>(0.003f * sampleRate);

    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float exc = 0.0f;
        if (i == 0) {
            exc = 2.2f + 0.8f * q;
        } else if (static_cast<int>(i) < burstN) {
            float env = std::exp(-t * 900.0f);
            exc = frand(rng) * env * (1.2f + 0.6f * q);
        } else {
            exc = frand(rng) * 0.015f * std::exp(-t * 30.0f) * q;
        }

        float wood = 0.0f;
        for (Mode& m : modes) {
            float y = exc * m.amp + m.b1 * m.y1 - m.a2 * m.y2;
            m.y2 = m.y1;
            m.y1 = y;
            wood += y;
        }

        float noise = frand(rng);
        float hp = 0.965f * (hpY1 + noise - hpX1);
        hpX1 = noise;
        hpY1 = hp;
        float crackEnv = std::exp(-t * (100.0f - q * 30.0f));
        if (t < 0.0004f) {
            crackEnv *= t / 0.0004f;
        }
        float crack = hp * crackEnv * (1.0f + 0.6f * q);

        float mix = wood * 0.42f + crack * 1.05f;
        if (q < 0.4f) {
            mix = mix * 0.65f + wood * 0.2f;
        }
        raw[i] = mix;
        peak = std::max(peak, std::abs(mix));
    }

    std::vector<std::int16_t> samples(n);
    for (std::size_t i = 0; i < n; i++) {
        float v = softClip(raw[i] / peak * 1.35f) * 30000.0f;
        samples[i] = static_cast<std::int16_t>(std::clamp(v, -32000.0f, 32000.0f));
    }

    return buffer.loadFromSamples(
        samples.data(),
        samples.size(),
        1,
        sampleRate,
        {sf::SoundChannel::Mono}
    );
}

inline bool storeSamples(sf::SoundBuffer& buffer, const std::vector<float>& raw) {
    float peak = 1e-6f;
    for (float v : raw) {
        peak = std::max(peak, std::abs(v));
    }
    std::vector<std::int16_t> samples(raw.size());
    for (std::size_t i = 0; i < raw.size(); i++) {
        float v = softClip(raw[i] / peak * 1.25f) * 30000.0f;
        samples[i] = static_cast<std::int16_t>(std::clamp(v, -32000.0f, 32000.0f));
    }
    return buffer.loadFromSamples(
        samples.data(),
        samples.size(),
        1,
        44100,
        {sf::SoundChannel::Mono}
    );
}

// Bat whoosh — band-limited noise swept up then down as the bat accelerates.
// intensity 0..1 (swing power) widens the sweep and fattens the body.
inline bool synthesizeWhoosh(sf::SoundBuffer& buffer, float intensity) {
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    constexpr unsigned sampleRate = 44100;
    const float duration = 0.26f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<float> raw(n, 0.0f);
    std::uint32_t rng = 0xBEEF01u + static_cast<std::uint32_t>(intensity * 331.0f);

    float lpY = 0.0f;
    float bpY = 0.0f;
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float u = t / duration;
        // Center frequency arcs 480 → ~2.2k → 900 Hz across the swing.
        float sweepHz = 480.0f + (1500.0f + 500.0f * intensity) * std::sin(u * kPi) +
                        420.0f * u;
        float w = 2.0f * kPi * sweepHz / static_cast<float>(sampleRate);
        w = std::min(w, 0.95f);
        float noise = frand(rng);
        lpY += w * (noise - lpY);
        bpY += w * ((noise - lpY) - bpY);
        // Fast attack, smooth decay — a touch slower for power swings.
        float envU = std::min(u * (1.12f - 0.12f * intensity), 1.0f);
        float env = std::sin(envU * kPi);
        raw[i] = bpY * env * (0.85f + 0.55f * intensity);
    }
    return storeSamples(buffer, raw);
}

// Catcher's mitt pop — leather snap: 2ms noise burst into two low modes.
inline bool synthesizeMittPop(sf::SoundBuffer& buffer, float pitchMph) {
    constexpr unsigned sampleRate = 44100;
    const float duration = 0.15f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<float> raw(n, 0.0f);
    std::uint32_t rng = 0xCA7C401u;
    float mphN = std::clamp((pitchMph - 45.0f) / 60.0f, 0.0f, 1.0f);

    // Faster pitches pop higher and harder.
    const float f1 = 170.0f + mphN * 70.0f;
    const float f2 = 430.0f + mphN * 120.0f;
    auto mode = [&](float freq, float damp) {
        float r = std::exp(-damp / static_cast<float>(sampleRate));
        float w = 2.0f * kPi * freq / static_cast<float>(sampleRate);
        return std::array{2.0f * r * std::cos(w), r * r};
    };
    auto m1 = mode(f1, 26.0f);
    auto m2 = mode(f2, 44.0f);
    float y1a = 0, y1b = 0, y2a = 0, y2b = 0;
    const int burstN = static_cast<int>(0.002f * sampleRate);
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float exc = 0.0f;
        if (i == 0) {
            exc = 2.0f;
        } else if (static_cast<int>(i) < burstN) {
            exc = frand(rng) * 0.8f;
        }
        float a = exc * 1.0f + m1[0] * y1a - m1[1] * y1b;
        y1b = y1a; y1a = a;
        float b = exc * 0.55f + m2[0] * y2a - m2[1] * y2b;
        y2b = y2a; y2a = b;
        float snap = frand(rng) * 0.22f * std::exp(-t * 260.0f);
        raw[i] = a * 0.5f + b * 0.35f + snap;
    }
    return storeSamples(buffer, raw);
}

// Outfield wall bang — padded-fence thud: deep low modes + dull burst.
inline bool synthesizeWallBang(sf::SoundBuffer& buffer, float exitMph) {
    constexpr unsigned sampleRate = 44100;
    const float duration = 0.30f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<float> raw(n, 0.0f);
    std::uint32_t rng = 0xBA49u;
    float mphN = std::clamp((exitMph - 70.0f) / 50.0f, 0.0f, 1.0f);

    auto mode = [&](float freq, float damp) {
        float r = std::exp(-damp / static_cast<float>(sampleRate));
        float w = 2.0f * kPi * freq / static_cast<float>(sampleRate);
        return std::array{2.0f * r * std::cos(w), r * r};
    };
    auto m1 = mode(95.0f + mphN * 30.0f, 18.0f);
    auto m2 = mode(210.0f + mphN * 45.0f, 30.0f);
    auto m3 = mode(640.0f, 70.0f);
    float y1a = 0, y1b = 0, y2a = 0, y2b = 0, y3a = 0, y3b = 0;
    const int burstN = static_cast<int>(0.004f * sampleRate);
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float exc = 0.0f;
        if (i == 0) {
            exc = 2.4f;
        } else if (static_cast<int>(i) < burstN) {
            exc = frand(rng) * std::exp(-t * 500.0f) * 1.4f;
        }
        float a = exc * 1.1f + m1[0] * y1a - m1[1] * y1b;
        y1b = y1a; y1a = a;
        float b = exc * 0.7f + m2[0] * y2a - m2[1] * y2b;
        y2b = y2a; y2a = b;
        float c = exc * 0.22f + m3[0] * y3a - m3[1] * y3b;
        y3b = y3a; y3a = c;
        raw[i] = a * 0.55f + b * 0.4f + c * 0.25f;
    }
    return storeSamples(buffer, raw);
}

// Crowd cheer swell — sustained banded noise with slow amplitude wobble.
// big = home-run-roar tier (longer sustain, wider band, louder).
inline bool synthesizeCrowdCheer(sf::SoundBuffer& buffer, bool big) {
    constexpr unsigned sampleRate = 44100;
    const float duration = big ? 2.1f : 1.2f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<float> raw(n, 0.0f);
    std::uint32_t rng = 0xC0CCC0u + (big ? 77u : 0u);

    // Voices live in a few broad bands; drive resonators with steady noise.
    auto mode = [&](float freq, float damp) {
        float r = std::exp(-damp / static_cast<float>(sampleRate));
        float w = 2.0f * kPi * freq / static_cast<float>(sampleRate);
        return std::array{2.0f * r * std::cos(w), r * r};
    };
    auto m1 = mode(520.0f, 3.0f);
    auto m2 = mode(880.0f, 4.0f);
    auto m3 = mode(big ? 1500.0f : 1250.0f, 5.0f);
    float y1a = 0, y1b = 0, y2a = 0, y2b = 0, y3a = 0, y3b = 0;
    float wobble = 0.0f;
    const float sustain = big ? 1.15f : 0.55f;
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float exc = frand(rng);
        float a = exc * 0.9f + m1[0] * y1a - m1[1] * y1b;
        y1b = y1a; y1a = a;
        float b = exc * 0.75f + m2[0] * y2a - m2[1] * y2b;
        y2b = y2a; y2a = b;
        float c = exc * 0.5f + m3[0] * y3a - m3[1] * y3b;
        y3b = y3a; y3a = c;
        // Slow crowd surging.
        wobble += 0.0008f * (frand(rng) * 0.5f - wobble);
        float env = std::min(t / 0.10f, 1.0f);
        if (t > sustain) {
            env *= std::exp(-(t - sustain) * (big ? 2.6f : 3.6f));
        }
        env *= 1.0f + 0.25f * wobble + 0.10f * std::sin(2.0f * kPi * 4.3f * t);
        raw[i] = (a * 0.5f + b * 0.42f + c * 0.3f) * env;
    }
    return storeSamples(buffer, raw);
}

} // namespace

// Full ballpark SFX set — bat cracks (soft / solid / barrel), swing whoosh,
// mitt pop, wall bang, and crowd cheer tiers.
struct BatParkSfx {
    sf::SoundBuffer crackBarrelBuf;
    sf::SoundBuffer crackSolidBuf;
    sf::SoundBuffer crackSoftBuf;
    sf::SoundBuffer whooshBuf;
    sf::SoundBuffer mittPopBuf;
    sf::SoundBuffer wallBangBuf;
    sf::SoundBuffer cheerBigBuf;
    sf::SoundBuffer cheerSmallBuf;
    sf::Sound crackBarrel;
    sf::Sound crackSolid;
    sf::Sound crackSoft;
    sf::Sound whoosh;
    sf::Sound mittPop;
    sf::Sound wallBang;
    sf::Sound cheerBig;
    sf::Sound cheerSmall;
    bool ok = false;
    bool usedFileSample = false;
    float masterSfx = 0.85f;

    BatParkSfx()
        : crackBarrel(crackBarrelBuf)
        , crackSolid(crackSolidBuf)
        , crackSoft(crackSoftBuf)
        , whoosh(whooshBuf)
        , mittPop(mittPopBuf)
        , wallBang(wallBangBuf)
        , cheerBig(cheerBigBuf)
        , cheerSmall(cheerSmallBuf) {
        bool fileBarrel = loadMonoWav(crackBarrelBuf, resolveSfxPath("bat_crack.wav"));
        bool fileSolid = loadMonoWav(crackSolidBuf, resolveSfxPath("bat_crack_solid.wav"));
        bool fileSoft = loadMonoWav(crackSoftBuf, resolveSfxPath("bat_crack_soft.wav"));

        if (!fileBarrel) {
            synthesizeBatCrack(crackBarrelBuf, 1.0f);
        } else {
            usedFileSample = true;
        }
        if (!fileSolid) {
            if (fileBarrel) {
                crackSolidBuf = crackBarrelBuf;
            } else {
                synthesizeBatCrack(crackSolidBuf, 0.70f);
            }
        }
        if (!fileSoft) {
            if (fileBarrel) {
                crackSoftBuf = crackBarrelBuf;
            } else {
                synthesizeBatCrack(crackSoftBuf, 0.25f);
            }
        }

        synthesizeWhoosh(whooshBuf, 0.8f);
        synthesizeMittPop(mittPopBuf, 70.0f);
        synthesizeWallBang(wallBangBuf, 95.0f);
        synthesizeCrowdCheer(cheerBigBuf, true);
        synthesizeCrowdCheer(cheerSmallBuf, false);

        crackBarrel = sf::Sound(crackBarrelBuf);
        crackSolid = sf::Sound(crackSolidBuf);
        crackSoft = sf::Sound(crackSoftBuf);
        whoosh = sf::Sound(whooshBuf);
        mittPop = sf::Sound(mittPopBuf);
        wallBang = sf::Sound(wallBangBuf);
        cheerBig = sf::Sound(cheerBigBuf);
        cheerSmall = sf::Sound(cheerSmallBuf);
        ok = true;
        applyMasterVolumes();

        std::cerr << "ProceduralSfx: full park set "
                  << (usedFileSample ? "(crack from assets/sfx/*.wav)" : "(all synthesis)")
                  << std::endl;
    }

    void setMasterVolumes(float sfx01, float /*musicUnused*/ = 0.0f) {
        masterSfx = std::clamp(sfx01, 0.0f, 1.0f);
        applyMasterVolumes();
    }

    void applyMasterVolumes() {
        crackBarrel.setVolume(96.0f * masterSfx);
        crackSolid.setVolume(88.0f * masterSfx);
        crackSoft.setVolume(70.0f * masterSfx);
        whoosh.setVolume(55.0f * masterSfx);
        mittPop.setVolume(65.0f * masterSfx);
        wallBang.setVolume(90.0f * masterSfx);
        cheerBig.setVolume(70.0f * masterSfx);
        cheerSmall.setVolume(55.0f * masterSfx);
    }

    // Ambient bed unused — kept so call sites stay simple.
    void startParkBed() {}
    void stopParkBed() {}

    // Crowd reaction — big = home-run roar, otherwise a shorter swell.
    void playCrowdPop(bool big) {
        if (!ok) {
            return;
        }
        sf::Sound& s = big ? cheerBig : cheerSmall;
        s.setPitch(0.95f + 0.08f * (big ? 1.0f : 0.0f));
        s.play();
    }

    // Padded outfield fence thud; faster exits thud harder and higher.
    void playWallBang(float exitMph = 95.0f) {
        if (!ok) {
            return;
        }
        float mphN = std::clamp((exitMph - 70.0f) / 50.0f, 0.0f, 1.0f);
        wallBang.setPitch(0.90f + mphN * 0.20f);
        wallBang.setVolume((70.0f + mphN * 28.0f) * masterSfx);
        wallBang.play();
    }

    // Bat swing whoosh; power01 0..1 scales volume/pitch with swing type.
    void playWhoosh(float power01) {
        if (!ok) {
            return;
        }
        power01 = std::clamp(power01, 0.0f, 1.0f);
        whoosh.setPitch(0.95f + power01 * 0.20f);
        whoosh.setVolume((45.0f + power01 * 25.0f) * masterSfx);
        whoosh.play();
    }

    // Catcher's mitt pop when a taken pitch lands in the glove.
    void playMittPop(float pitchMph = 70.0f) {
        if (!ok) {
            return;
        }
        float mphN = std::clamp((pitchMph - 45.0f) / 60.0f, 0.0f, 1.0f);
        mittPop.setPitch(0.92f + mphN * 0.22f);
        mittPop.setVolume((50.0f + mphN * 25.0f) * masterSfx);
        mittPop.play();
    }

    // sweet01 0..1, exitMph for pitch/volume, barrelHr for loudest tier.
    // Always a bat crack — soft / solid / barrel only.
    void playContact(float sweet01, bool barrelHr, float exitMph = 95.0f) {
        if (!ok) {
            return;
        }
        sweet01 = std::clamp(sweet01, 0.0f, 1.0f);
        float mphNorm = std::clamp((exitMph - 70.0f) / 50.0f, 0.0f, 1.0f);

        sf::Sound* s = &crackSoft;
        float baseVol = 58.0f;
        if (barrelHr || (sweet01 > 0.78f && exitMph >= 98.0f)) {
            s = &crackBarrel;
            baseVol = 92.0f;
        } else if (sweet01 > 0.42f || exitMph >= 85.0f) {
            s = &crackSolid;
            baseVol = 78.0f;
        }

        s->setPitch(0.90f + sweet01 * 0.14f + mphNorm * 0.08f);
        s->setVolume((baseVol + sweet01 * 22.0f + mphNorm * 12.0f) * masterSfx);
        s->play();
    }
};

} // namespace ProceduralSfx
