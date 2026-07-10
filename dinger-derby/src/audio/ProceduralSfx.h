#pragma once

#include <SFML/Audio.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Tiny procedural SFX for demos (no external wav assets).
// Generates mono 16-bit samples into SoundBuffers that outlive sf::Sound.
namespace ProceduralSfx {

inline void fillNoise(std::vector<std::int16_t>& out, float amp, std::uint32_t& rng) {
    for (std::int16_t& s : out) {
        rng = rng * 1664525u + 1013904223u;
        float n = (static_cast<float>(rng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        float v = n * amp * 32000.0f;
        s = static_cast<std::int16_t>(std::clamp(v, -32000.0f, 32000.0f));
    }
}

inline bool makeBatCrack(sf::SoundBuffer& buffer) {
    constexpr unsigned sampleRate = 22050;
    constexpr float duration = 0.12f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<std::int16_t> samples(n, 0);
    std::uint32_t rng = 0xC0FFEEu;
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float env = std::exp(-t * 48.0f);
        // Sharp transient + mid body
        rng = rng * 1664525u + 1013904223u;
        float noise = (static_cast<float>(rng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        float click = std::sin(2.0f * 3.14159265f * 2100.0f * t) * std::exp(-t * 90.0f);
        float body = std::sin(2.0f * 3.14159265f * 380.0f * t) * std::exp(-t * 28.0f);
        float v = (noise * 0.55f + click * 0.85f + body * 0.45f) * env * 28000.0f;
        samples[i] = static_cast<std::int16_t>(std::clamp(v, -32000.0f, 32000.0f));
    }
    return buffer.loadFromSamples(samples.data(), samples.size(), 1, sampleRate, {sf::SoundChannel::Mono});
}

inline bool makeContactThud(sf::SoundBuffer& buffer) {
    constexpr unsigned sampleRate = 22050;
    constexpr float duration = 0.09f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<std::int16_t> samples(n, 0);
    std::uint32_t rng = 0xBEEFu;
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float env = std::exp(-t * 35.0f);
        rng = rng * 1664525u + 1013904223u;
        float noise = (static_cast<float>(rng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        float low = std::sin(2.0f * 3.14159265f * 140.0f * t);
        float v = (noise * 0.35f + low * 0.7f) * env * 18000.0f;
        samples[i] = static_cast<std::int16_t>(std::clamp(v, -32000.0f, 32000.0f));
    }
    return buffer.loadFromSamples(samples.data(), samples.size(), 1, sampleRate, {sf::SoundChannel::Mono});
}

// Longer crowd surge for home runs (~1.8s).
inline bool makeCrowdPop(sf::SoundBuffer& buffer) {
    constexpr unsigned sampleRate = 22050;
    constexpr float duration = 1.85f;
    const std::size_t n = static_cast<std::size_t>(sampleRate * duration);
    std::vector<std::int16_t> samples(n, 0);
    std::uint32_t rng = 0xA11CEu;
    for (std::size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        // Attack + sustained roar + decay
        float attack = std::clamp(t / 0.08f, 0.0f, 1.0f);
        float sustain = std::exp(-std::max(0.0f, t - 0.35f) * 1.6f);
        float env = attack * sustain;
        rng = rng * 1664525u + 1013904223u;
        float n1 = (static_cast<float>(rng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        rng = rng * 1664525u + 1013904223u;
        float n2 = (static_cast<float>(rng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
        // Band-limited-ish roar: low noise + slow amplitude modulation
        float mod = 0.7f + 0.3f * std::sin(2.0f * 3.14159265f * 7.0f * t);
        float roar = (n1 * 0.65f + n2 * 0.35f) * mod;
        float cheer = std::sin(2.0f * 3.14159265f * 520.0f * t) * 0.08f * env;
        float v = (roar * 0.9f + cheer) * env * 12000.0f;
        samples[i] = static_cast<std::int16_t>(std::clamp(v, -32000.0f, 32000.0f));
    }
    return buffer.loadFromSamples(samples.data(), samples.size(), 1, sampleRate, {sf::SoundChannel::Mono});
}

struct BatParkSfx {
    sf::SoundBuffer crackBuf;
    sf::SoundBuffer thudBuf;
    sf::SoundBuffer crowdBuf;
    sf::Sound crack;
    sf::Sound thud;
    sf::Sound crowd;
    bool ok = false;

    BatParkSfx()
        : crack(crackBuf)
        , thud(thudBuf)
        , crowd(crowdBuf) {
        bool a = makeBatCrack(crackBuf);
        bool b = makeContactThud(thudBuf);
        bool c = makeCrowdPop(crowdBuf);
        // Re-bind after buffers are filled (SFML 3 Sound holds buffer ref).
        crack = sf::Sound(crackBuf);
        thud = sf::Sound(thudBuf);
        crowd = sf::Sound(crowdBuf);
        ok = a && b && c;
        if (ok) {
            crack.setVolume(85.0f);
            thud.setVolume(55.0f);
            crowd.setVolume(70.0f);
        }
    }

    void playContact(float sweet01, bool barrelHr) {
        if (!ok) {
            return;
        }
        if (barrelHr || sweet01 > 0.72f) {
            crack.setPitch(0.92f + sweet01 * 0.2f);
            crack.setVolume(70.0f + sweet01 * 30.0f);
            crack.play();
        } else {
            thud.setPitch(0.85f + sweet01 * 0.25f);
            thud.play();
        }
    }

    void playCrowdPop(bool big) {
        if (!ok) {
            return;
        }
        crowd.setVolume(big ? 90.0f : 55.0f);
        crowd.setPitch(big ? 1.0f : 0.92f);
        crowd.play();
    }
};

} // namespace ProceduralSfx
