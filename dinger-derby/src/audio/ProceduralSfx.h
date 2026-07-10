#pragma once

#include <SFML/Audio.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// Bat / park SFX. Prefers optional WAV files under assets/sfx/ (royalty-free or
// your own recordings). Falls back to high-rate procedural synthesis.
//
// NOTE: Real MLB broadcast audio is copyrighted — do not rip game/TV audio.
// Drop legally licensed samples here if you have them:
//   assets/sfx/bat_crack.wav      solid barrel
//   assets/sfx/bat_crack_soft.wav  mishit / handle
//   assets/sfx/crowd_pop.wav       optional crowd surge
namespace ProceduralSfx {
namespace {

constexpr float kPi = 3.14159265358979323846f;

inline float frand(std::uint32_t& rng) {
    rng = rng * 1664525u + 1013904223u;
    return (static_cast<float>(rng & 0xFFFFFFu) / static_cast<float>(0x1000000u)) * 2.0f - 1.0f;
}

inline float softClip(float x) {
    // Gentle tape-like saturation (sounds less "synth beep").
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

// Find assets/sfx relative to cwd or common run locations (build/, repo root).
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

// Layered wood-bat crack inspired by broadcast hits (not a copy of any recording).
// quality: 0 = mishit, 1 = solid barrel
inline bool synthesizeBatCrack(sf::SoundBuffer& buffer, float quality) {
    quality = std::clamp(quality, 0.0f, 1.0f);
    constexpr unsigned sampleRate = 44100;
    // Barrels ring a touch longer; mishits die faster.
    const float duration = 0.18f + quality * 0.10f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<std::int16_t> samples(n, 0);
    std::uint32_t rng = 0xBADC0FFEu + static_cast<std::uint32_t>(quality * 1000.0f);

    // Characteristic wood modes (approx) — bright crack + mid body + low thump.
    const float fCrack = 2800.0f + quality * 1600.0f; // 2.8–4.4 kHz snap
    const float fCrack2 = 5100.0f + quality * 900.0f;
    const float fWood1 = 420.0f + quality * 80.0f;
    const float fWood2 = 780.0f + quality * 120.0f;
    const float fThump = 95.0f + quality * 40.0f;

    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);

        // Ultra-fast attack (the "pop" you hear on TV).
        float attack = 1.0f - std::exp(-t * 900.0f);
        float envCrack = attack * std::exp(-t * (55.0f - quality * 12.0f));
        float envBody = attack * std::exp(-t * (22.0f - quality * 6.0f));
        float envThump = attack * std::exp(-t * (14.0f - quality * 4.0f));
        float envAir = std::exp(-t * 40.0f);

        // Broadband noise shaped for contact transient
        float noise = frand(rng);
        // Crude one-pole high emphasis for crack noise
        static thread_local float hp = 0.0f;
        hp = noise - hp * 0.65f + hp;
        float crackNoise = (noise * 0.45f + hp * 0.55f);

        // Frequency chirp down slightly (impact → wood ring)
        float chirp = 1.0f - 0.18f * std::min(t * 40.0f, 1.0f);
        float snap =
            std::sin(2.0f * kPi * fCrack * chirp * t) * 0.55f +
            std::sin(2.0f * kPi * fCrack2 * chirp * t) * 0.28f +
            crackNoise * (0.55f + quality * 0.35f);

        float wood =
            std::sin(2.0f * kPi * fWood1 * t) * 0.55f +
            std::sin(2.0f * kPi * fWood2 * t) * 0.35f +
            std::sin(2.0f * kPi * (fWood1 * 2.01f) * t) * 0.12f * quality;

        float thump = std::sin(2.0f * kPi * fThump * t) * (0.55f + quality * 0.35f);

        // Slight "whoosh" residual for barrel exit
        float air = frand(rng) * 0.12f * quality;

        float mix =
            snap * envCrack * (0.85f + quality * 0.35f) +
            wood * envBody * (0.45f + quality * 0.40f) +
            thump * envThump * (0.35f + quality * 0.45f) +
            air * envAir;

        // Mishits: duller, more mid, less snap
        if (quality < 0.45f) {
            mix = mix * 0.75f + thump * envThump * 0.35f + wood * envBody * 0.25f;
            mix *= 0.85f;
        }

        float v = softClip(mix * (0.72f + quality * 0.28f)) * 30000.0f;
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

inline bool makeCrowdPop(sf::SoundBuffer& buffer) {
    constexpr unsigned sampleRate = 44100;
    constexpr float duration = 2.1f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<std::int16_t> samples(n, 0);
    std::uint32_t rng = 0xA11CEu;
    float lp = 0.0f;
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float attack = std::clamp(t / 0.10f, 0.0f, 1.0f);
        float sustain = std::exp(-std::max(0.0f, t - 0.45f) * 1.35f);
        float env = attack * sustain;
        float n1 = frand(rng);
        float n2 = frand(rng);
        // Low-passed roar
        lp = lp * 0.86f + (n1 * 0.7f + n2 * 0.3f) * 0.14f;
        float mod = 0.75f + 0.25f * std::sin(2.0f * kPi * 6.5f * t);
        float v = softClip(lp * mod * env * 1.1f) * 14000.0f;
        samples[i] = static_cast<std::int16_t>(std::clamp(v, -32000.0f, 32000.0f));
    }
    return buffer.loadFromSamples(
        samples.data(), samples.size(), 1, sampleRate, {sf::SoundChannel::Mono}
    );
}

} // namespace

struct BatParkSfx {
    sf::SoundBuffer crackBarrelBuf;
    sf::SoundBuffer crackSolidBuf;
    sf::SoundBuffer crackSoftBuf;
    sf::SoundBuffer crowdBuf;
    sf::Sound crackBarrel;
    sf::Sound crackSolid;
    sf::Sound crackSoft;
    sf::Sound crowd;
    bool ok = false;
    bool usedFileSample = false;

    BatParkSfx()
        : crackBarrel(crackBarrelBuf)
        , crackSolid(crackSolidBuf)
        , crackSoft(crackSoftBuf)
        , crowd(crowdBuf) {
        // Prefer user-supplied WAVs (licensed / recorded yourself).
        bool fileBarrel = loadMonoWav(crackBarrelBuf, resolveSfxPath("bat_crack.wav"));
        bool fileSoft = loadMonoWav(crackSoftBuf, resolveSfxPath("bat_crack_soft.wav"));
        bool fileCrowd = loadMonoWav(crowdBuf, resolveSfxPath("crowd_pop.wav"));

        if (fileBarrel) {
            usedFileSample = true;
            // Duplicate solid from barrel if only one file provided.
            crackSolidBuf = crackBarrelBuf;
            if (!fileSoft) {
                synthesizeBatCrack(crackSoftBuf, 0.25f);
            }
        } else {
            synthesizeBatCrack(crackBarrelBuf, 1.0f);
            synthesizeBatCrack(crackSolidBuf, 0.72f);
            synthesizeBatCrack(crackSoftBuf, 0.28f);
        }

        if (!fileCrowd) {
            makeCrowdPop(crowdBuf);
        }

        crackBarrel = sf::Sound(crackBarrelBuf);
        crackSolid = sf::Sound(crackSolidBuf);
        crackSoft = sf::Sound(crackSoftBuf);
        crowd = sf::Sound(crowdBuf);
        ok = true;

        crackBarrel.setVolume(92.0f);
        crackSolid.setVolume(82.0f);
        crackSoft.setVolume(62.0f);
        crowd.setVolume(72.0f);

        if (usedFileSample) {
            std::cerr << "ProceduralSfx: using assets/sfx bat_crack.wav" << std::endl;
        } else {
            std::cerr << "ProceduralSfx: using synthesized MLB-style bat crack "
                         "(drop assets/sfx/bat_crack.wav for a real sample)"
                      << std::endl;
        }
    }

    // sweet01 0..1, exitMph for pitch/volume, barrelHr for loudest tier.
    void playContact(float sweet01, bool barrelHr, float exitMph = 95.0f) {
        if (!ok) {
            return;
        }
        sweet01 = std::clamp(sweet01, 0.0f, 1.0f);
        float mphNorm = std::clamp((exitMph - 70.0f) / 50.0f, 0.0f, 1.0f);

        sf::Sound* s = &crackSoft;
        if (barrelHr || (sweet01 > 0.82f && exitMph >= 100.0f)) {
            s = &crackBarrel;
        } else if (sweet01 > 0.55f || exitMph >= 90.0f) {
            s = &crackSolid;
        }

        // Faster / harder contact reads slightly brighter.
        s->setPitch(0.94f + sweet01 * 0.10f + mphNorm * 0.06f);
        s->setVolume(58.0f + sweet01 * 28.0f + mphNorm * 14.0f);
        s->play();
    }

    void playCrowdPop(bool big) {
        if (!ok) {
            return;
        }
        crowd.setVolume(big ? 92.0f : 58.0f);
        crowd.setPitch(big ? 1.0f : 0.94f);
        crowd.play();
    }
};

} // namespace ProceduralSfx
