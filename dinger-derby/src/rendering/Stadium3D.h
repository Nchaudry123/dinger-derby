#pragma once

#include <vector>

#include "Mesh3D.h"
#include "../math/Vector3.h"

// Shared open-air ballpark for HR Derby / pitching / batting.
// Style: minor-league outdoor horseshoe (Fredericksburg-class reference) —
// striped grass, dirt diamond, blue OF wall, blue lower bowl + red club seats,
// light towers, suite facade behind home. No closed dome.
//
// Play coordinate frame:
//   Mound at (0, 0, 0)
//   Home plate at (0, 0, plateZ) with plateZ = 60.5 ft / feetPerUnit
//   Pitcher throws toward +Z; batter faces −Z (toward mound / CF)
//   Center field is −Z from home; +X = first-base / RF side
//
// 1 world unit ≈ feetPerUnit feet (default 2).

namespace Stadium3D {

// Outdoor daylight sky.
inline sf::Color skyColor() { return sf::Color(135, 175, 215); }
inline sf::Color skyZenithColor() { return sf::Color(90, 140, 195); }
inline sf::Color groundClearColor() { return sf::Color(55, 70, 55); }
inline sf::Color concreteFloorColor() { return sf::Color(145, 138, 125); }

// FieldTurf-ish green + classic dirt.
inline sf::Color grassColor() { return sf::Color(42, 130, 55); }
inline sf::Color grassDarkColor() { return sf::Color(34, 112, 48); }
inline sf::Color dirtColor() { return sf::Color(185, 125, 70); }
inline sf::Color warningTrackColor() { return sf::Color(168, 112, 62); }

// Reference: royal blue lower seats, red club / corner seats, blue OF padding.
inline sf::Color seatBlueColor() { return sf::Color(28, 70, 150); }
inline sf::Color seatBlueAltColor() { return sf::Color(22, 58, 130); }
inline sf::Color seatRedColor() { return sf::Color(185, 45, 45); }
inline sf::Color seatRedAltColor() { return sf::Color(160, 35, 38); }
inline sf::Color seatMidColor() { return sf::Color(28, 70, 150); }
inline sf::Color seatUpperColor() { return sf::Color(185, 45, 45); }
inline sf::Color concourseColor() { return sf::Color(150, 145, 135); }
inline sf::Color facadeTanColor() { return sf::Color(175, 160, 140); }
inline sf::Color facadeGrayColor() { return sf::Color(155, 155, 158); }
inline sf::Color ofWallColor() { return sf::Color(18, 55, 120); }
inline sf::Color ofWallTopColor() { return sf::Color(25, 70, 145); }
inline sf::Color boardChassisColor() { return sf::Color(240, 240, 245); }
inline sf::Color railColor() { return sf::Color(210, 215, 220); }
// Legacy aliases used by older call sites.
inline sf::Color domePanelColor() { return facadeGrayColor(); }
inline sf::Color domeRibColor() { return sf::Color(90, 95, 100); }
inline sf::Color hotelFacadeColor() { return facadeTanColor(); }

struct Layout {
    float feetPerUnit = 2.0f;
    float pitchingDistanceFeet = 60.5f;
    float wallDistanceFeet = 400.0f;
    float wallHeightFeet = 8.0f; // padded OF wall ~8 ft
    float foulAngleDegrees = 45.0f;
    float infieldRadiusFeet = 95.0f;
    float basePathFeet = 90.0f;
    bool closedDome = false; // open-air park
    float roofPeakFeet = 120.0f;
    float buildingRadiusFeet = 280.0f;
    float domeCenterOffsetFeet = 140.0f;

    float plateZ() const { return pitchingDistanceFeet / feetPerUnit; }
    float wallR() const { return wallDistanceFeet / feetPerUnit; }
    float wallH() const { return wallHeightFeet / feetPerUnit; }
    float foulAngleRad() const;
    float infieldR() const { return infieldRadiusFeet / feetPerUnit; }
    float basePath() const { return basePathFeet / feetPerUnit; }
    float moundZ() const { return 0.0f; }
    float roofPeakY() const { return roofPeakFeet / feetPerUnit; }
    float domeHorizR() const { return buildingRadiusFeet / feetPerUnit; }
    float roofShellR() const { return domeHorizR(); }
    Vector3 domeCenter() const;
    float domeRoofYAtWorld(float worldX, float worldZ) const;
    float domeRoofYAtRadius(float radiusFromHome) const;
    float maxRadiusFromHome(float angleRad) const;
    float clampRadiusInDome(float angleRad, float radius, float margin = 5.0f) const;
    bool isCfScoreboardZone(float angleRad) const;
    float seatDeckYAtRadius(float radiusFromHome, float angleRad) const;
    bool containInsideDome(Vector3& position, Vector3& velocity, float radius) const;

    float wallFeetAtAngle(float angleRad) const;
    float wallRAtAngle(float angleRad) const {
        return wallFeetAtAngle(angleRad) / feetPerUnit;
    }
    float wallHeightAtAngle(float angleRad) const;

    Vector3 fromHome(float radius, float angleRad, float y = 0.0f) const;
    Vector3 wallPoint(float angleRad, float yFraction = 0.0f) const;

    Vector3 home() const { return Vector3(0.0f, 0.0f, plateZ()); }
    Vector3 mound() const { return Vector3(0.0f, 0.0f, moundZ()); }
    Vector3 cfWall() const { return wallPoint(0.0f, 1.0f); }
    Vector3 parkCenter() const;
    Vector3 scoreboardCenter() const;

    Vector3 firstBase() const { return fromHome(basePath(), foulAngleRad()); }
    Vector3 thirdBase() const { return fromHome(basePath(), -foulAngleRad()); }
    Vector3 secondBase() const {
        return fromHome(basePath() * 1.41421356f, 0.0f);
    }

    float maxWallR() const;
    void polarFromHome(const Vector3& worldPos, float& radiusOut, float& angleRadOut) const;
    float radiusFromHome(const Vector3& worldPos) const;

    // First seat row / bowl base (horseshoe around diamond).
    float bowlInnerRadius(float angleRad) const;
    float bowlBaseHeight(float angleRad) const;
    // True if angle is in the main seating horseshoe (not open OF).
    bool isSeatingArc(float angleRad) const;
};

struct WallClearResult {
    bool fair = true;
    bool clearsWall = false;
    bool hitsWallFace = false;
    float sprayDeg = 0.0f;
    float fenceFeet = 0.0f;
    float wallTopY = 0.0f;
    float heightAtFence = 0.0f;
    float marginFeet = 0.0f;
    float landFeet = 0.0f;
};

WallClearResult evaluateWallClear(
    const Layout& layout,
    Vector3 position,
    Vector3 velocity,
    float gravityY = -9.8f,
    float dragK = 0.012f
);

enum class HitSurface {
    None = 0,
    Ground,
    Fence,
    FenceTopClear,
    Backstop,
    Dugout,
    Scoreboard,
    Stands,
    FoulPole,
    Roof,
    DomeShell
};

struct BallCollisionHit {
    HitSurface surface = HitSurface::None;
    bool stuck = false;
    float impactY = 0.0f;
    float wallTopY = 0.0f;
    float sprayDeg = 0.0f;
    float fenceFeet = 0.0f;
};

BallCollisionHit collideBall(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact = true
);

BallCollisionHit collideBallSubsteps(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact,
    int substeps = 4
);

constexpr int kFanSectorCount = 24;
constexpr int kFlagCount = 8;

struct Meshes {
    Mesh3D field;
    Mesh3D walls;
    Mesh3D stands;
    Mesh3D lines;
    Mesh3D city;              // exterior berm / grass apron
    Mesh3D scoreboardScreen;  // CF / OF boards
    Mesh3D skyDome;           // unused (open air)
    Mesh3D clouds;            // unused
    Mesh3D hotel;             // suite / club facade behind home
    Mesh3D structure;         // light towers, poles, rails
    std::vector<Mesh3D> fanSectors;
    std::vector<Mesh3D> flagMeshes;
    std::vector<Vector3> flagBases;
};

Layout defaultPlayLayout();
Meshes build(const Layout& layout = defaultPlayLayout());

float recommendedFarPlane(const Layout& layout = defaultPlayLayout());

float fanCheerOffsetY(int sectorIndex, float timeSec, float boost = 1.0f);
float fanCheerOffsetX(int sectorIndex, float timeSec, float boost = 1.0f);
float flagSwayYaw(int flagIndex, float timeSec);
float scoreboardPulse(float timeSec, float excitement = 0.0f);

} // namespace Stadium3D
