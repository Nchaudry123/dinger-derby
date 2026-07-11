#pragma once

#include <vector>

#include "Mesh3D.h"
#include "../math/Vector3.h"

// Shared play-space frame for HR Derby / pitching / batting.
//
// Play coordinate frame:
//   Mound at (0, 0, 0)
//   Home plate at (0, 0, plateZ) with plateZ = 60.5 ft / feetPerUnit
//   Pitcher throws toward +Z; batter faces −Z (toward mound / CF)
//   Center field is −Z from home (past the mound)
//   +X = first-base side when facing CF from home
//
// Visual stadium model: intentionally empty — rebuild from scratch.
// Layout + fence math remain so bat/pitch demos still have a play space.
//
// 1 world unit ≈ feetPerUnit feet (default 2).

namespace Stadium3D {

inline sf::Color skyColor() { return sf::Color(120, 165, 210); }
inline sf::Color skyZenithColor() { return sf::Color(70, 130, 190); }
inline sf::Color groundClearColor() { return sf::Color(22, 30, 40); }
inline sf::Color concreteFloorColor() { return sf::Color(95, 98, 104); }
// Legacy palette stubs (no mesh uses these until the park is rebuilt).
inline sf::Color grassColor() { return sf::Color(48, 140, 58); }
inline sf::Color grassDarkColor() { return sf::Color(36, 112, 48); }
inline sf::Color dirtColor() { return sf::Color(188, 118, 62); }
inline sf::Color warningTrackColor() { return sf::Color(160, 105, 58); }
inline sf::Color domePanelColor() { return sf::Color(215, 222, 232); }
inline sf::Color domeRibColor() { return sf::Color(48, 58, 72); }
inline sf::Color seatBlueColor() { return sf::Color(18, 42, 98); }
inline sf::Color seatBlueAltColor() { return sf::Color(14, 34, 82); }
inline sf::Color seatMidColor() { return sf::Color(16, 38, 90); }
inline sf::Color seatUpperColor() { return sf::Color(20, 48, 108); }
inline sf::Color concourseColor() { return sf::Color(95, 98, 104); }
inline sf::Color hotelFacadeColor() { return sf::Color(232, 228, 218); }
inline sf::Color ofWallColor() { return sf::Color(18, 42, 78); }
inline sf::Color boardChassisColor() { return sf::Color(12, 18, 28); }

struct Layout {
    float feetPerUnit = 2.0f;
    float pitchingDistanceFeet = 60.5f;
    float wallDistanceFeet = 400.0f;
    float wallHeightFeet = 10.0f;
    float foulAngleDegrees = 45.0f;
    float infieldRadiusFeet = 95.0f;
    float basePathFeet = 90.0f;
    // Kept for API compatibility; no dome mesh is drawn.
    bool closedDome = false;
    float roofPeakFeet = 300.0f;
    float buildingRadiusFeet = 430.0f;
    float domeCenterOffsetFeet = 200.0f;

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

    // Stubs — no bowl geometry until the park is rebuilt.
    float bowlInnerRadius(float angleRad) const;
    float bowlBaseHeight(float angleRad) const;
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

// Ground-only collision until stadium geometry is rebuilt.
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

constexpr int kFanSectorCount = 32;
constexpr int kFlagCount = 12;

// Empty mesh bundle — rebuild from scratch.
struct Meshes {
    Mesh3D field;
    Mesh3D walls;
    Mesh3D stands;
    Mesh3D lines;
    Mesh3D city;
    Mesh3D scoreboardScreen;
    Mesh3D skyDome;
    Mesh3D clouds;
    Mesh3D hotel;
    Mesh3D structure;
    std::vector<Mesh3D> fanSectors;
    std::vector<Mesh3D> flagMeshes;
    std::vector<Vector3> flagBases;
};

Layout defaultPlayLayout();
// Returns empty meshes (no stadium model).
Meshes build(const Layout& layout = defaultPlayLayout());

float recommendedFarPlane(const Layout& layout = defaultPlayLayout());

float fanCheerOffsetY(int sectorIndex, float timeSec, float boost = 1.0f);
float fanCheerOffsetX(int sectorIndex, float timeSec, float boost = 1.0f);
float flagSwayYaw(int flagIndex, float timeSec);
float scoreboardPulse(float timeSec, float excitement = 0.0f);

} // namespace Stadium3D
