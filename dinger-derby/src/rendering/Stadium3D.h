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

// Rogers Centre palette (photo reference) — deep navy seats, FieldTurf, steel roof.
inline sf::Color grassColor() { return sf::Color(48, 140, 58); }
inline sf::Color grassDarkColor() { return sf::Color(36, 112, 48); }
inline sf::Color dirtColor() { return sf::Color(188, 118, 62); }
inline sf::Color warningTrackColor() { return sf::Color(160, 105, 58); }
// Indoor clear / void outside the shell.
inline sf::Color skyColor() { return sf::Color(18, 24, 34); }
inline sf::Color skyZenithColor() { return sf::Color(40, 70, 110); }
inline sf::Color groundClearColor() { return sf::Color(18, 24, 34); }
inline sf::Color domePanelColor() { return sf::Color(215, 222, 232); }
inline sf::Color domeRibColor() { return sf::Color(48, 58, 72); }
// Deep navy seating (photo: dark blue bowl, not bright royal).
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
    // Fallback nominal CF (overridden by wallFeetAtAngle for the fence).
    float wallDistanceFeet = 400.0f;
    float wallHeightFeet = 10.0f;
    float foulAngleDegrees = 45.0f;
    float infieldRadiusFeet = 95.0f;
    float basePathFeet = 90.0f;
    // Closed retractable roof (Rogers Centre / SkyDome).
    // Building is a circular plan ~700 ft diameter; roof peak ~282 ft AGL.
    bool closedDome = true;
    float roofPeakFeet = 282.0f;           // official roof high point
    float buildingRadiusFeet = 350.0f;     // 700 ft diameter / 2
    // Dome center is offset from home toward CF so the circular shell covers
    // field + stands + CF hotel (home sits near the south of the circle).
    float domeCenterOffsetFeet = 175.0f;

    float plateZ() const { return pitchingDistanceFeet / feetPerUnit; }
    float wallR() const { return wallDistanceFeet / feetPerUnit; }
    float wallH() const { return wallHeightFeet / feetPerUnit; }
    float foulAngleRad() const;
    float infieldR() const { return infieldRadiusFeet / feetPerUnit; }
    float basePath() const { return basePathFeet / feetPerUnit; }
    float moundZ() const { return 0.0f; }
    float roofPeakY() const { return roofPeakFeet / feetPerUnit; }
    // Horizontal radius of the circular building shell (world units).
    float domeHorizR() const { return buildingRadiusFeet / feetPerUnit; }
    // Legacy alias used by demos (same as circular shell radius).
    float roofShellR() const { return domeHorizR(); }
    // Geometric center of the circular dome on the field plane (y=0).
    Vector3 domeCenter() const;
    // Roof underside Y for a world XZ (same math as the drawn shell).
    float domeRoofYAtWorld(float worldX, float worldZ) const;
    // Roof underside Y at a horizontal radius measured from HOME (approx).
    float domeRoofYAtRadius(float radiusFromHome) const;
    // Max playable radius from home at this spray before hitting the shell.
    float maxRadiusFromHome(float angleRad) const;
    // Stepped seating-deck floor height past the fence (world Y), for landings.
    float seatDeckYAtRadius(float radiusFromHome, float angleRad) const;
    // Project a sphere fully inside the closed dome (hard containment).
    // Returns true if a correction was applied.
    bool containInsideDome(Vector3& position, Vector3& velocity, float radius) const;

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

// Multi-substep collision for high-speed balls (prevents tunneling through
// fence / roof / shell). Prefer this over a single collideBall call in flight.
BallCollisionHit collideBallSubsteps(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact,
    int substeps = 4
);

// Fan sections for cheer-wave animation (draw each with a small Y bob).
constexpr int kFanSectorCount = 16;
// Flags around the park (draw with flagSwayYaw for wind).
constexpr int kFlagCount = 12;

struct Meshes {
    Mesh3D field;
    Mesh3D walls;
    Mesh3D stands;   // full bowl: seats, aisles, concourses, hotel, backstop
    Mesh3D lines;
    Mesh3D city;     // empty for closed dome (no exterior)
    Mesh3D scoreboardScreen; // CF board face — demos can pulse alpha/color feel
    Mesh3D skyDome;  // closed roof shell (or open-air sky)
    Mesh3D clouds;   // empty for closed dome
    Mesh3D hotel;    // CF hotel facade (Rogers signature interior feature)
    Mesh3D structure; // roof trusses / ring beams / light banks
    // Low-poly crowd split by angle so demos can bob sections for cheering.
    std::vector<Mesh3D> fanSectors;
    // Wind-blown flags (empty for closed dome — no exterior poles).
    std::vector<Mesh3D> flagMeshes;
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
