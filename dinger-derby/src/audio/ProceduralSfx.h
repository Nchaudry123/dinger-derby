#pragma once

#include <SFML/Audio.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// Bat crack SFX only. Prefers WAVs under assets/sfx/; falls back to synthesis.
//
// NOTE: Real MLB broadcast audio is copyrighted — do not rip game/TV audio.
//   assets/sfx/bat_crack.wav       barrel / best contact
//   assets/sfx/bat_crack_solid.wav solid square-up
//   assets/sfx/bat_crack_soft.wav  mishit / light contact
namespace ProceduralSfx {
namespace {

constexpr float kPi = 3.14159265358979323846f;

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

} // namespace

// Bat crack only — soft / solid / barrel tiers of the same wooden hit.
struct BatParkSfx {
    sf::SoundBuffer crackBarrelBuf;
    sf::SoundBuffer crackSolidBuf;
    sf::SoundBuffer crackSoftBuf;
    sf::Sound crackBarrel;
    sf::Sound crackSolid;
    sf::Sound crackSoft;
    bool ok = false;
    bool usedFileSample = false;
    float masterSfx = 0.85f;

    BatParkSfx()
        : crackBarrel(crackBarrelBuf)
        , crackSolid(crackSolidBuf)
        , crackSoft(crackSoftBuf) {
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

        crackBarrel = sf::Sound(crackBarrelBuf);
        crackSolid = sf::Sound(crackSolidBuf);
        crackSoft = sf::Sound(crackSoftBuf);
        ok = true;
        applyMasterVolumes();

        std::cerr << "ProceduralSfx: bat crack only "
                  << (usedFileSample ? "(assets/sfx/*.wav)" : "(synthesis fallback)")
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
    }

    // No-ops kept so call sites stay simple.
    void startParkBed() {}
    void stopParkBed() {}
    void playCrowdPop(bool /*big*/) {}
    void playWallBang(float /*exitMph*/ = 95.0f) {}

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
