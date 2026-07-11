#pragma once

#include <vector>

#include "Mesh3D.h"
#include "../math/Vector3.h"

// Shared procedural ballpark for HR Derby / pitching / batting.
//
// Play coordinate frame (matches pitching_simulator + bat_physics):
//   Mound at (0, 0, 0)
//   Home plate at (0, 0, plateZ) with plateZ = 60.5 ft / feetPerUnit
//   Pitcher throws toward +Z; batter faces −Z (toward mound / CF)
//   Center field is −Z from home (past the mound)
//   +X = first-base side when facing CF from home
//
// Default park = Rogers Centre (Toronto) closed dome:
//   LF/RF 328 · alleys 375 · CF 400 · ~10' walls · retractable roof closed
//   so fly balls hit the roof/shell instead of traveling forever.
//
// 1 world unit ≈ feetPerUnit feet (default 2).

namespace Stadium3D {

// Shared palette — FieldTurf + indoor dome lighting (Rogers Centre vibe).
inline sf::Color grassColor() { return sf::Color(42, 128, 58); }
inline sf::Color grassDarkColor() { return sf::Color(34, 108, 48); }
inline sf::Color dirtColor() { return sf::Color(168, 120, 70); }
inline sf::Color warningTrackColor() { return sf::Color(150, 120, 70); }
// Indoor dome wash (soft cool white-blue) — matches closed-roof interior.
inline sf::Color skyColor() { return sf::Color(168, 188, 210); }
inline sf::Color skyZenithColor() { return sf::Color(120, 150, 185); }
inline sf::Color groundClearColor() { return grassColor(); }
// Structural roof panel colors (white membrane + steel ribs).
inline sf::Color domePanelColor() { return sf::Color(210, 220, 232); }
inline sf::Color domeRibColor() { return sf::Color(70, 85, 105); }

struct Layout {
    float feetPerUnit = 2.0f;
    float pitchingDistanceFeet = 60.5f;
    // Fallback nominal CF (overridden by wallFeetAtAngle for the fence).
    float wallDistanceFeet = 400.0f;
    float wallHeightFeet = 10.0f;
    float foulAngleDegrees = 45.0f;
    float infieldRadiusFeet = 95.0f;
    float basePathFeet = 90.0f;
    // Closed retractable roof (Rogers Centre). Balls bounce off the shell.
    bool closedDome = true;
    // Peak roof height above field (ft). High enough for normal HRs into seats;
    // still catches moonshots / lasers that would leave the universe.
    float roofPeakFeet = 205.0f;
    // Dome base radius = max wall + this padding (ft) past the OF fence.
    float roofShellPaddingFeet = 95.0f;

    float plateZ() const { return pitchingDistanceFeet / feetPerUnit; }
    float wallR() const { return wallDistanceFeet / feetPerUnit; }
    float wallH() const { return wallHeightFeet / feetPerUnit; }
    float foulAngleRad() const;
    float infieldR() const { return infieldRadiusFeet / feetPerUnit; }
    float basePath() const { return basePathFeet / feetPerUnit; }
    float moundZ() const { return 0.0f; }
    float roofPeakY() const { return roofPeakFeet / feetPerUnit; }
    float roofShellR() const {
        return maxWallR() + roofShellPaddingFeet / feetPerUnit;
    }
    // Roof underside Y at a horizontal radius from home (world units).
    float domeRoofYAtRadius(float radiusFromHome) const;

    // OF fence distance (feet) vs spray angle from CF.
    // angleRad = 0 is CF (−Z); +angle toward +X (RF / 1B).
    float wallFeetAtAngle(float angleRad) const;
    float wallRAtAngle(float angleRad) const {
        return wallFeetAtAngle(angleRad) / feetPerUnit;
    }
    // Rogers-style relatively uniform wall height.
    float wallHeightAtAngle(float angleRad) const;

    // Point on foul/outfield arc measured from home toward CF (−Z).
    Vector3 fromHome(float radius, float angleRad, float y = 0.0f) const;
    Vector3 wallPoint(float angleRad, float yFraction = 0.0f) const;

    Vector3 home() const { return Vector3(0.0f, 0.0f, plateZ()); }
    Vector3 mound() const { return Vector3(0.0f, 0.0f, moundZ()); }
    Vector3 cfWall() const { return wallPoint(0.0f, 1.0f); }
    Vector3 parkCenter() const;
    // CF scoreboard chassis center (for overlays / camera aim).
    Vector3 scoreboardCenter() const;

    // True diamond bases (1 unit ≈ feetPerUnit feet; basePath = 90 ft side).
    // Foul lines at ±foulAngle; 1B/3B sit on those lines at basePath.
    Vector3 firstBase() const { return fromHome(basePath(), foulAngleRad()); }
    Vector3 thirdBase() const { return fromHome(basePath(), -foulAngleRad()); }
    Vector3 secondBase() const {
        return fromHome(basePath() * 1.41421356f, 0.0f);
    }

    // Max fence radius (for ground size / far plane).
    float maxWallR() const;

    // Horizontal distance (world units) and spray angle from home.
    // angleRad: 0 = CF (−Z), + toward +X (RF).
    void polarFromHome(const Vector3& worldPos, float& radiusOut, float& angleRadOut) const;
    float radiusFromHome(const Vector3& worldPos) const;

    // Shared bowl footprint (field apron + stands use the same curve).
    float bowlInnerRadius(float angleRad) const;
    float bowlBaseHeight(float angleRad) const;
};

// Result of sampling a batted-ball arc against the asymmetric OF fence.
// clearsWall: ball crosses fence radius above wall top (true HR geometry).
// hitsWallFace: ball reaches fence radius below top (wall ball / double).
struct WallClearResult {
    bool fair = true;
    bool clearsWall = false;
    bool hitsWallFace = false;
    float sprayDeg = 0.0f;
    float fenceFeet = 0.0f;
    float wallTopY = 0.0f;       // world Y of wall top at impact spray
    float heightAtFence = 0.0f;  // ball Y when crossing fence radius
    float marginFeet = 0.0f;     // (heightAtFence - wallTopY) * feetPerUnit
    float landFeet = 0.0f;       // ground landing distance from home (feet)
};

// Integrate exit velocity with gravity + light quadratic drag (matches bat demo
// post-contact atmosphere closely enough for HR calls).
WallClearResult evaluateWallClear(
    const Layout& layout,
    Vector3 position,
    Vector3 velocity,
    float gravityY = -9.8f,
    float dragK = 0.012f
);

// Solid stadium collision for a sphere (fence, backstop, dugouts, board, ground, dome).
// Mutates pos/vel so the ball cannot travel through park geometry.
enum class HitSurface {
    None = 0,
    Ground,
    Fence,
    FenceTopClear, // crossed fence above wall height (not a bounce)
    Backstop,
    Dugout,
    Scoreboard,
    Stands,
    FoulPole,
    Roof,      // closed dome underside
    DomeShell  // outer vertical shell of the dome
};

struct BallCollisionHit {
    HitSurface surface = HitSurface::None;
    bool stuck = false;       // velocity zeroed (land/wall stick)
    float impactY = 0.0f;     // ball Y at contact
    float wallTopY = 0.0f;    // fence top when relevant
    float sprayDeg = 0.0f;
    float fenceFeet = 0.0f;
};

// stickOnContact: true for HR-derby landings (no bounce/roll).
BallCollisionHit collideBall(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact = true
);

// Fan sections for cheer-wave animation (draw each with a small Y bob).
constexpr int kFanSectorCount = 16;
// Flags around the park (draw with flagSwayYaw for wind).
constexpr int kFlagCount = 12;

struct Meshes {
    Mesh3D field;
    Mesh3D walls;
    Mesh3D stands;   // full bowl: seats, aisles, concourses, backstop
    Mesh3D lines;
    Mesh3D city;     // suburban skyline backdrop (full ring)
    Mesh3D scoreboardScreen; // CF board face — demos can pulse alpha/color feel
    Mesh3D skyDome;  // gradient hemisphere (draw first, far away)
    Mesh3D clouds;   // soft cloud puffs (draw with slight alpha)
    // Low-poly crowd split by angle so demos can bob sections for cheering.
    std::vector<Mesh3D> fanSectors;
    // Wind-blown flags (pivot near pole base of each mesh).
    std::vector<Mesh3D> flagMeshes;
    // World-space base of each flag for sway transform.
    std::vector<Vector3> flagBases;
};

Layout defaultPlayLayout();
Meshes build(const Layout& layout = defaultPlayLayout());

float recommendedFarPlane(const Layout& layout = defaultPlayLayout());

// Cheer bob offset for sector i at time t (seconds). boost >1 after HRs.
float fanCheerOffsetY(int sectorIndex, float timeSec, float boost = 1.0f);
// Extra lateral sway for denser "active crowd" feel (meters-ish world units).
float fanCheerOffsetX(int sectorIndex, float timeSec, float boost = 1.0f);
// Flag yaw sway (radians) for wind animation.
float flagSwayYaw(int flagIndex, float timeSec);
// Scoreboard screen emissive pulse 0..1 for HR moments.
float scoreboardPulse(float timeSec, float excitement = 0.0f);

} // namespace Stadium3D
