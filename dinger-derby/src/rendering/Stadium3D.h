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
// Dimensions inspired by Citizens Bank Park (asymmetric OF):
//   LF 329 · LCF ~374–387 · deepest ~404 · CF ~401 · RCF ~369 · RF 330
//
// 1 world unit ≈ feetPerUnit feet (default 2).

namespace Stadium3D {

struct Layout {
    float feetPerUnit = 2.0f;
    float pitchingDistanceFeet = 60.5f;
    // Fallback nominal CF (overridden by wallFeetAtAngle for the fence).
    float wallDistanceFeet = 401.0f;
    float wallHeightFeet = 11.0f;
    float foulAngleDegrees = 45.0f;
    float infieldRadiusFeet = 95.0f;
    float basePathFeet = 90.0f;

    float plateZ() const { return pitchingDistanceFeet / feetPerUnit; }
    float wallR() const { return wallDistanceFeet / feetPerUnit; }
    float wallH() const { return wallHeightFeet / feetPerUnit; }
    float foulAngleRad() const;
    float infieldR() const { return infieldRadiusFeet / feetPerUnit; }
    float basePath() const { return basePathFeet / feetPerUnit; }
    float moundZ() const { return 0.0f; }

    // Asymmetric OF fence distance (feet) vs spray angle from CF.
    // angleRad = 0 is CF (−Z); +angle toward +X (RF / 1B).
    float wallFeetAtAngle(float angleRad) const;
    float wallRAtAngle(float angleRad) const {
        return wallFeetAtAngle(angleRad) / feetPerUnit;
    }
    // Slight height variation (LF wall taller, like many parks).
    float wallHeightAtAngle(float angleRad) const;

    // Point on foul/outfield arc measured from home toward CF (−Z).
    Vector3 fromHome(float radius, float angleRad, float y = 0.0f) const;
    Vector3 wallPoint(float angleRad, float yFraction = 0.0f) const;

    Vector3 home() const { return Vector3(0.0f, 0.0f, plateZ()); }
    Vector3 mound() const { return Vector3(0.0f, 0.0f, moundZ()); }
    Vector3 cfWall() const { return wallPoint(0.0f, 1.0f); }
    Vector3 parkCenter() const;

    // Max fence radius (for ground size / far plane).
    float maxWallR() const;
};

// Fan sections for cheer-wave animation (draw each with a small Y bob).
constexpr int kFanSectorCount = 16;

struct Meshes {
    Mesh3D field;
    Mesh3D walls;
    Mesh3D stands;   // full bowl: seats, aisles, concourses, backstop
    Mesh3D lines;
    Mesh3D city;     // suburban skyline backdrop (full ring)
    // Low-poly crowd split by angle so demos can bob sections for cheering.
    std::vector<Mesh3D> fanSectors;
};

Layout defaultPlayLayout();
Meshes build(const Layout& layout = defaultPlayLayout());

float recommendedFarPlane(const Layout& layout = defaultPlayLayout());

// Cheer bob offset for sector i at time t (seconds).
float fanCheerOffsetY(int sectorIndex, float timeSec);

} // namespace Stadium3D
