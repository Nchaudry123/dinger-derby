#include "Stadium3D.h"

#include <algorithm>
#include <cmath>

// Stadium visual model removed — blank play space only.
// Rebuild field / stands / roof from scratch when ready.

namespace Stadium3D {
namespace {

constexpr float pi = 3.1415926535f;

} // namespace

float Layout::foulAngleRad() const {
    return foulAngleDegrees * (pi / 180.0f);
}

// Rogers Centre OF fence (feet) vs angle from CF — keep for HR distance math.
float Layout::wallFeetAtAngle(float angleRad) const {
    float aDeg = angleRad * (180.0f / pi);
    aDeg = std::clamp(aDeg, -foulAngleDegrees, foulAngleDegrees);

    static const float samples[][2] = {
        {-45.0f, 328.0f},
        {-32.0f, 368.0f},
        {-22.0f, 381.0f},
        {-8.0f, 395.0f},
        {0.0f, 400.0f},
        {10.0f, 385.0f},
        {20.0f, 372.0f},
        {32.0f, 359.0f},
        {45.0f, 328.0f},
    };
    constexpr int n = static_cast<int>(sizeof(samples) / sizeof(samples[0]));
    if (aDeg <= samples[0][0]) {
        return samples[0][1];
    }
    if (aDeg >= samples[n - 1][0]) {
        return samples[n - 1][1];
    }
    for (int i = 0; i < n - 1; i++) {
        if (aDeg >= samples[i][0] && aDeg <= samples[i + 1][0]) {
            float u = (aDeg - samples[i][0]) / (samples[i + 1][0] - samples[i][0]);
            u = u * u * (3.0f - 2.0f * u);
            return samples[i][1] + (samples[i + 1][1] - samples[i][1]) * u;
        }
    }
    return wallDistanceFeet;
}

float Layout::wallHeightAtAngle(float /*angleRad*/) const {
    return wallHeightFeet / feetPerUnit;
}

Vector3 Layout::domeCenter() const {
    return Vector3(0.0f, 0.0f, plateZ() - domeCenterOffsetFeet / feetPerUnit);
}

float Layout::domeRoofYAtWorld(float /*worldX*/, float /*worldZ*/) const {
    return roofPeakY();
}

float Layout::domeRoofYAtRadius(float /*radiusFromHome*/) const {
    return roofPeakY();
}

float Layout::maxRadiusFromHome(float angleRad) const {
    return wallRAtAngle(angleRad) + 80.0f;
}

float Layout::clampRadiusInDome(float /*angleRad*/, float radius, float /*margin*/) const {
    return std::max(0.0f, radius);
}

bool Layout::isCfScoreboardZone(float /*angleRad*/) const {
    return false;
}

float Layout::seatDeckYAtRadius(float /*radiusFromHome*/, float /*angleRad*/) const {
    return 0.0f;
}

bool Layout::containInsideDome(Vector3& /*position*/, Vector3& /*velocity*/, float /*radius*/) const {
    return false;
}

Vector3 Layout::fromHome(float radius, float angleRad, float y) const {
    return Vector3(
        std::sin(angleRad) * radius,
        y,
        plateZ() - std::cos(angleRad) * radius
    );
}

Vector3 Layout::wallPoint(float angleRad, float yFraction) const {
    float r = wallRAtAngle(angleRad);
    float h = wallHeightAtAngle(angleRad) * std::clamp(yFraction, 0.0f, 1.0f);
    return fromHome(r, angleRad, h);
}

float Layout::maxWallR() const {
    float mx = 0.0f;
    for (int i = 0; i <= 32; i++) {
        float a = -foulAngleRad() + (2.0f * foulAngleRad()) * (static_cast<float>(i) / 32.0f);
        mx = std::max(mx, wallRAtAngle(a));
    }
    return mx;
}

Vector3 Layout::parkCenter() const {
    return Vector3(0.0f, 4.0f, plateZ() - maxWallR() * 0.42f);
}

Vector3 Layout::scoreboardCenter() const {
    float cfR = wallRAtAngle(0.0f);
    float cfH = wallHeightAtAngle(0.0f);
    return fromHome(cfR + 9.0f, 0.0f, cfH + 13.5f);
}

void Layout::polarFromHome(const Vector3& worldPos, float& radiusOut, float& angleRadOut) const {
    float dx = worldPos.x;
    float dz = worldPos.z - plateZ();
    radiusOut = std::sqrt(dx * dx + dz * dz);
    angleRadOut = std::atan2(dx, -dz);
}

float Layout::radiusFromHome(const Vector3& worldPos) const {
    float r = 0.0f;
    float a = 0.0f;
    polarFromHome(worldPos, r, a);
    (void)a;
    return r;
}

float Layout::bowlInnerRadius(float ang) const {
    // No stands — report just past the fence so landings stay sane.
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    float fa = foulAngleRad();
    if (std::abs(ang) <= fa + 0.02f) {
        return wallRAtAngle(ang) + 2.0f;
    }
    return 20.0f;
}

float Layout::bowlBaseHeight(float /*ang*/) const {
    return 0.0f;
}

BallCollisionHit collideBall(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact
) {
    BallCollisionHit hit;
    const float groundY = radius + 0.01f;

    float r = 0.0f;
    float ang = 0.0f;
    layout.polarFromHome(position, r, ang);
    hit.sprayDeg = ang * (180.0f / pi);
    hit.fenceFeet = layout.wallFeetAtAngle(ang);
    hit.wallTopY = layout.wallHeightAtAngle(ang);

    // Ground only — no fence / stands / roof until park is rebuilt.
    if (position.y < groundY) {
        position.y = groundY;
        hit.surface = HitSurface::Ground;
        hit.impactY = groundY;
        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * 0.38f;
            velocity.x *= 0.86f;
            velocity.z *= 0.86f;
        }
        if ((stickOnContact && velocity.magnitude() < 3.8f) || velocity.magnitude() < 2.6f) {
            velocity = Vector3();
            hit.stuck = true;
        }
    } else if (position.y < groundY + 0.12f && velocity.y <= 0.15f) {
        if (velocity.magnitude() < 3.2f || (stickOnContact && velocity.magnitude() < 4.5f)) {
            position.y = groundY;
            velocity = Vector3();
            hit.surface = HitSurface::Ground;
            hit.impactY = groundY;
            hit.stuck = true;
        }
    }

    // Track fence-clear geometrically (no solid wall yet).
    if (std::abs(ang) <= layout.foulAngleRad() + 0.02f && r > layout.wallRAtAngle(ang)) {
        if (position.y > layout.wallHeightAtAngle(ang) + radius) {
            if (hit.surface == HitSurface::None) {
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
            }
        }
    }

    return hit;
}

BallCollisionHit collideBallSubsteps(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact,
    int substeps
) {
    BallCollisionHit last;
    substeps = std::max(1, substeps);
    Vector3 start = position;
    Vector3 end = position; // caller already integrated; just re-check
    (void)start;
    (void)end;
    for (int i = 0; i < substeps; i++) {
        last = collideBall(layout, position, velocity, radius, stickOnContact);
        if (last.stuck) {
            break;
        }
    }
    return last;
}

WallClearResult evaluateWallClear(
    const Layout& layout,
    Vector3 position,
    Vector3 velocity,
    float gravityY,
    float dragK
) {
    WallClearResult out;
    const float dt = 1.0f / 120.0f;
    const float maxT = 12.0f;
    float prevR = layout.radiusFromHome(position);
    bool crossed = false;

    for (float t = 0.0f; t < maxT; t += dt) {
        float sp = velocity.magnitude();
        if (sp > 1e-4f) {
            Vector3 drag = velocity * (-dragK * sp);
            velocity = velocity + (Vector3(0.0f, gravityY, 0.0f) + drag) * dt;
        } else {
            velocity.y += gravityY * dt;
        }
        position = position + velocity * dt;

        float r = 0.0f;
        float ang = 0.0f;
        layout.polarFromHome(position, r, ang);
        bool stepFair = std::abs(ang * (180.0f / pi)) <= layout.foulAngleDegrees + 0.5f;
        float wallR = stepFair ? layout.wallRAtAngle(ang) : layout.maxWallR() * 1.5f;

        if (stepFair && prevR < wallR && r >= wallR) {
            float u = (wallR - prevR) / std::max(r - prevR, 1e-5f);
            float yAt = (position.y - velocity.y * dt) + velocity.y * dt * u;
            // Approximate height at fence from linear segment.
            yAt = position.y; // close enough after step
            out.sprayDeg = ang * (180.0f / pi);
            out.fenceFeet = layout.wallFeetAtAngle(ang);
            out.wallTopY = layout.wallHeightAtAngle(ang);
            out.heightAtFence = yAt;
            out.marginFeet = (yAt - out.wallTopY) * layout.feetPerUnit;
            out.fair = true;
            out.clearsWall = yAt > out.wallTopY;
            out.hitsWallFace = !out.clearsWall;
            crossed = true;
            break;
        }
        prevR = r;

        if (position.y < 0.05f && velocity.y <= 0.0f) {
            break;
        }
    }

    float endR = 0.0f;
    float endA = 0.0f;
    layout.polarFromHome(position, endR, endA);
    out.landFeet = std::max(0.0f, endR * layout.feetPerUnit);
    if (!crossed) {
        out.sprayDeg = endA * (180.0f / pi);
        out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
        out.fenceFeet = layout.wallFeetAtAngle(endA);
        out.wallTopY = layout.wallHeightAtAngle(endA);
    }
    return out;
}

Layout defaultPlayLayout() {
    Layout L;
    L.wallDistanceFeet = 400.0f;
    L.wallHeightFeet = 10.0f;
    L.closedDome = false; // no dome model
    L.roofPeakFeet = 300.0f;
    L.buildingRadiusFeet = 430.0f;
    L.domeCenterOffsetFeet = 200.0f;
    return L;
}

Meshes build(const Layout& /*layout*/) {
    // Blank park — no field, walls, stands, roof, fans, or structure.
    Meshes out;
    out.fanSectors.assign(kFanSectorCount, Mesh3D{});
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    return std::max(1200.0f, layout.maxWallR() * 6.0f + 200.0f);
}

float fanCheerOffsetY(int /*sectorIndex*/, float /*timeSec*/, float /*boost*/) {
    return 0.0f;
}

float fanCheerOffsetX(int /*sectorIndex*/, float /*timeSec*/, float /*boost*/) {
    return 0.0f;
}

float flagSwayYaw(int /*flagIndex*/, float /*timeSec*/) {
    return 0.0f;
}

float scoreboardPulse(float /*timeSec*/, float /*excitement*/) {
    return 0.0f;
}

} // namespace Stadium3D
