#pragma once

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
// 1 world unit ≈ feetPerUnit feet (default 2).

namespace Stadium3D {

struct Layout {
    float feetPerUnit = 2.0f;
    float pitchingDistanceFeet = 60.5f;
    float wallDistanceFeet = 380.0f;
    float wallHeightFeet = 12.0f;
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

    // Point on foul/outfield arc measured from home toward CF (−Z).
    // angleRad = 0 is CF; +angle is toward +X (1B).
    Vector3 fromHome(float radius, float angleRad, float y = 0.0f) const;

    Vector3 home() const { return Vector3(0.0f, 0.0f, plateZ()); }
    Vector3 mound() const { return Vector3(0.0f, 0.0f, moundZ()); }
    Vector3 cfWall() const { return fromHome(wallR(), 0.0f, wallH()); }
    Vector3 parkCenter() const;
};

struct Meshes {
    Mesh3D field;
    Mesh3D walls;
    Mesh3D stands;   // outfield + behind-home seating / backstop
    Mesh3D lines;
    Mesh3D city;     // suburban skyline backdrop (full ring)
};

Layout defaultPlayLayout();
Meshes build(const Layout& layout = defaultPlayLayout());

// Suggested camera far plane for stadium-scale scenes.
float recommendedFarPlane(const Layout& layout = defaultPlayLayout());

} // namespace Stadium3D
