#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Local career bests + light unlocks for HR Derby (key=value file).
namespace DerbyBests {

struct Stats {
    int mostHrsInRound = 0;
    float longestHrFeet = 0.0f;
    float bestExitMph = 0.0f;
    int roundsPlayed = 0;
    // Unlocks / milestones (persisted).
    int totalHrsCareer = 0;
    int hardUnlocked = 0;   // 1 after 3+ HR on Normal in one round
    int goalsCleared = 0;   // session goals completed
    int moonballs = 0;
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
        } else if (key == "total_hrs") {
            in >> s.totalHrsCareer;
        } else if (key == "hard_unlocked") {
            in >> s.hardUnlocked;
        } else if (key == "goals") {
            in >> s.goalsCleared;
        } else if (key == "moonballs") {
            in >> s.moonballs;
        } else {
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
    out << "total_hrs " << s.totalHrsCareer << "\n";
    out << "hard_unlocked " << s.hardUnlocked << "\n";
    out << "goals " << s.goalsCleared << "\n";
    out << "moonballs " << s.moonballs << "\n";
    return true;
}

// Merge a finished round into career bests; returns true if any record broken.
inline bool recordRound(
    Stats& career,
    int hrCount,
    float longestFeet,
    float bestEv,
    int difficulty, // 0 Easy 1 Normal 2 Hard
    int moonballCount = 0
) {
    bool improved = false;
    career.roundsPlayed += 1;
    career.totalHrsCareer += std::max(0, hrCount);
    career.moonballs += std::max(0, moonballCount);
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
    // Unlock Hard after a solid Normal (or Hard) round.
    if (difficulty >= 1 && hrCount >= 3 && career.hardUnlocked == 0) {
        career.hardUnlocked = 1;
        improved = true;
    }
    save(career);
    return improved;
}

// Session goal HRs for a difficulty (product targets).
inline int sessionGoalHrs(int difficulty) {
    switch (difficulty) {
        case 0:
            return 4; // Easy
        case 2:
            return 3; // Hard
        default:
            return 5; // Normal
    }
}

} // namespace DerbyBests
