#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// Lightweight local settings (~/.dinger-derby/settings.txt).
namespace GameSettings {

struct Data {
    float sfxVolume = 0.85f;   // 0..1 master for bat/park SFX
    float musicVolume = 0.45f; // 0..1 ambient bed
    int derbyDiff = 1;         // 0 Easy, 1 Normal, 2 Hard
    bool showHelpOnLaunch = true;
};

inline std::filesystem::path settingsPath() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::filesystem::path(home) / ".dinger-derby" / "settings.txt";
    }
    return std::filesystem::path("settings.txt");
}

inline Data load() {
    Data d;
    std::ifstream in(settingsPath());
    if (!in) {
        return d;
    }
    std::string key;
    while (in >> key) {
        if (key == "sfx") {
            in >> d.sfxVolume;
        } else if (key == "music") {
            in >> d.musicVolume;
        } else if (key == "diff") {
            in >> d.derbyDiff;
        } else if (key == "help") {
            int h = 1;
            in >> h;
            d.showHelpOnLaunch = h != 0;
        } else {
            std::string junk;
            std::getline(in, junk);
        }
    }
    d.sfxVolume = std::max(0.0f, std::min(1.0f, d.sfxVolume));
    d.musicVolume = std::max(0.0f, std::min(1.0f, d.musicVolume));
    d.derbyDiff = std::max(0, std::min(2, d.derbyDiff));
    return d;
}

inline bool save(const Data& d) {
    std::filesystem::path p = settingsPath();
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p);
    if (!out) {
        std::cerr << "GameSettings: failed to write " << p << std::endl;
        return false;
    }
    out << "sfx " << d.sfxVolume << "\n";
    out << "music " << d.musicVolume << "\n";
    out << "diff " << d.derbyDiff << "\n";
    out << "help " << (d.showHelpOnLaunch ? 1 : 0) << "\n";
    return true;
}

} // namespace GameSettings
