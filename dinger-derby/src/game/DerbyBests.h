#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Local career bests for HR Derby (simple key=value file, no JSON dependency).
namespace DerbyBests {

struct Stats {
    int mostHrsInRound = 0;
    float longestHrFeet = 0.0f;
    float bestExitMph = 0.0f;
    int roundsPlayed = 0;
};

inline std::filesystem::path bestsPath() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::filesystem::path(home) / ".dinger-derby" / "bests.txt";
    }
    return std::filesystem::path("bests.txt");
}

inline Stats load() {
    Stats s;
    std::filesystem::path p = bestsPath();
    std::ifstream in(p);
    if (!in) {
        return s;
    }
    std::string key;
    while (in >> key) {
        if (key == "most_hrs") {
            in >> s.mostHrsInRound;
        } else if (key == "longest_ft") {
            in >> s.longestHrFeet;
        } else if (key == "best_ev") {
            in >> s.bestExitMph;
        } else if (key == "rounds") {
            in >> s.roundsPlayed;
        } else {
            // skip unknown
            std::string junk;
            std::getline(in, junk);
        }
    }
    return s;
}

inline bool save(const Stats& s) {
    std::filesystem::path p = bestsPath();
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p);
    if (!out) {
        std::cerr << "DerbyBests: failed to write " << p << std::endl;
        return false;
    }
    out << "most_hrs " << s.mostHrsInRound << "\n";
    out << "longest_ft " << s.longestHrFeet << "\n";
    out << "best_ev " << s.bestExitMph << "\n";
    out << "rounds " << s.roundsPlayed << "\n";
    return true;
}

// Merge a finished round into career bests; returns true if any record broken.
inline bool recordRound(Stats& career, int hrCount, float longestFeet, float bestEv) {
    bool improved = false;
    career.roundsPlayed += 1;
    if (hrCount > career.mostHrsInRound) {
        career.mostHrsInRound = hrCount;
        improved = true;
    }
    if (longestFeet > career.longestHrFeet) {
        career.longestHrFeet = longestFeet;
        improved = true;
    }
    if (bestEv > career.bestExitMph) {
        career.bestExitMph = bestEv;
        improved = true;
    }
    save(career);
    return improved;
}

} // namespace DerbyBests
