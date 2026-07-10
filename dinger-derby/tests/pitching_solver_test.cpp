#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "math/Vector3.h"
#include "physics/AirResistance3D.h"
#include "physics/Body3D.h"
#include "physics/PhysicsWorld3D.h"

namespace {

constexpr float fixedStep = 1.0f / 180.0f;
constexpr float baseballRadius = 0.065f;
constexpr float feetPerWorldUnit = 2.0f;
constexpr float pitchAirDensity = 0.075f;
constexpr float plateZ = 60.5f / feetPerWorldUnit;
constexpr float pi = 3.1415926535f;
const Vector3 releasePoint(-0.22f, 1.72f, 0.0f);
const Vector3 boundsMinimum(-3.2f, -40.0f, -2.0f);
const Vector3 boundsMaximum(3.2f, 3.6f, plateZ + 4.0f);

struct PitchProfile {
    char hotkey;
    std::string name;
    float baseSpeedMph;
    float spinRpm;
    Vector3 spinAxis;
    float spinEfficiency;
    float magnusScale;
    float dragCoefficient;
    float airScale;
};

struct PitchFlightVariation {
    Vector3 airVelocity;
    float spinRpmScale = 1.0f;
    float dragScale = 1.0f;
    float liftOffset = 0.0f;
};

std::array<PitchProfile, 5> makePitchProfiles() {
    return {{
        PitchProfile{'F', "Four-Seam", 96.1f, 2450.0f, Vector3(-1.0f, 0.06f, 0.05f), 0.98f, 1.15f, 0.30f, 0.88f},
        PitchProfile{'P', "Splitter", 91.5f, 1300.0f, Vector3(-0.25f, 0.20f, 0.92f), 0.40f, 0.55f, 0.42f, 1.05f},
        PitchProfile{'C', "Curve", 77.1f, 2800.0f, Vector3(0.78f, -0.55f, -0.18f), 0.92f, 1.25f, 0.36f, 0.95f},
        PitchProfile{'T', "Cutter", 91.4f, 2500.0f, Vector3(-0.55f, -0.72f, 0.20f), 0.90f, 1.05f, 0.32f, 0.90f},
        PitchProfile{'S', "Slider", 87.2f, 2650.0f, Vector3(0.30f, -0.82f, 0.40f), 0.88f, 1.20f, 0.34f, 0.92f}
    }};
}

float mphToWorldUnitsPerSecond(float mph) {
    return mph * 5280.0f / 3600.0f / feetPerWorldUnit;
}

Vector3 angularVelocityFromProfile(const PitchProfile& pitch, float rpmScale) {
    float m = pitch.spinAxis.magnitude();
    Vector3 axis = m > 1e-6f ? pitch.spinAxis * (1.0f / m) : Vector3(-1.0f, 0.0f, 0.0f);
    return axis * (pitch.spinRpm * rpmScale * (2.0f * pi / 60.0f));
}

Vector3 assembleLaunchVelocity(float pitchSpeed, float vx, float vy) {
    float maxSide = pitchSpeed * 0.22f;
    float minVy = pitchSpeed * std::tan(-12.0f * pi / 180.0f);
    float maxVy = pitchSpeed * std::tan(24.0f * pi / 180.0f);
    vx = std::clamp(vx, -maxSide, maxSide);
    vy = std::clamp(vy, minVy, maxVy);
    float lat2 = vx * vx + vy * vy;
    float maxLat = pitchSpeed * 0.55f;
    if (lat2 > maxLat * maxLat && lat2 > 1e-8f) {
        float s = maxLat / std::sqrt(lat2);
        vx *= s;
        vy *= s;
        lat2 = vx * vx + vy * vy;
    }
    float vz = std::sqrt(std::max(pitchSpeed * pitchSpeed - lat2, pitchSpeed * pitchSpeed * 0.55f));
    return Vector3(vx, vy, vz);
}

bool simulatePlateCrossing(
    const PitchProfile& pitch,
    const Vector3& start,
    const Vector3& velocity,
    const Vector3& angularVelocity,
    float airDensity,
    const Vector3& airVelocity,
    float dragScale,
    Vector3& outPlate
) {
    PhysicsWorld3D world;
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(airDensity, airVelocity);
    world.airResistanceEnabled = true;
    world.setBounds(boundsMinimum, boundsMaximum);

    Body3D ball(start, 0.145f);
    ball.setRadius(baseballRadius);
    ball.dragCoefficient = pitch.dragCoefficient * dragScale;
    ball.airResistanceScale = pitch.airScale;
    ball.spinEfficiency = pitch.spinEfficiency;
    ball.magnusScale = pitch.magnusScale;
    ball.angularVelocity = angularVelocity;
    ball.velocity = velocity;
    world.addBody(&ball);

    Vector3 previous = ball.position;
    for (int step = 0; step < 900; step++) {
        previous = ball.position;
        world.step(fixedStep);
        if (ball.position.z >= plateZ && ball.velocity.z > 0.0f) {
            float seg = ball.position.z - previous.z;
            float t = seg <= 1e-6f ? 1.0f : (plateZ - previous.z) / seg;
            t = std::clamp(t, 0.0f, 1.0f);
            outPlate = previous + (ball.position - previous) * t;
            outPlate.z = plateZ;
            return true;
        }
    }
    outPlate = ball.position;
    return false;
}

Vector3 calculateLaunchVelocity(
    const PitchProfile& pitch,
    const Vector3& aimPoint,
    float pitchSpeedMph,
    const PitchFlightVariation& variation,
    const Vector3& startPosition,
    const Vector3& releaseAngularVelocity
) {
    float pitchSpeed = mphToWorldUnitsPerSecond(pitchSpeedMph);
    float distance = std::max(1.0f, aimPoint.z - startPosition.z);
    float dragSlow = std::clamp(0.94f - pitch.dragCoefficient * pitch.airScale * 0.12f, 0.84f, 0.96f);
    float flightTime = distance / std::max(1.0f, pitchSpeed * dragSlow);

    float vx = (aimPoint.x - startPosition.x) / flightTime;
    float vy =
        (aimPoint.y - startPosition.y + 0.5f * 9.8f * flightTime * flightTime) / flightTime +
        variation.liftOffset;
    Vector3 velocity = assembleLaunchVelocity(pitchSpeed, vx, vy);
    const float airDensity = pitchAirDensity * variation.dragScale;
    const float gain = 0.92f;

    for (int iter = 0; iter < 8; iter++) {
        Vector3 plateHit;
        bool ok = simulatePlateCrossing(
            pitch, startPosition, velocity, releaseAngularVelocity,
            airDensity, variation.airVelocity, variation.dragScale, plateHit
        );
        if (!ok) {
            velocity = assembleLaunchVelocity(pitchSpeed, velocity.x, velocity.y + 1.2f);
            continue;
        }
        float errX = aimPoint.x - plateHit.x;
        float errY = aimPoint.y - plateHit.y;
        if (errX * errX + errY * errY < 0.0004f) {
            break;
        }
        float measuredT = std::clamp(distance / std::max(1.0f, velocity.z * dragSlow), 0.25f, 1.2f);
        velocity = assembleLaunchVelocity(
            pitchSpeed,
            velocity.x + errX / measuredT * gain,
            velocity.y + errY / measuredT * gain
        );
    }
    return velocity;
}

void testPitchesReachPlateNearAim() {
    auto pitches = makePitchProfiles();
    PitchFlightVariation variation;
    Vector3 aim(0.0f, 1.28f, plateZ);

    for (const PitchProfile& pitch : pitches) {
        Vector3 omega = angularVelocityFromProfile(pitch, 1.0f);
        Vector3 v0 = calculateLaunchVelocity(
            pitch, aim, pitch.baseSpeedMph, variation, releasePoint, omega
        );
        Vector3 plate;
        bool ok = simulatePlateCrossing(
            pitch, releasePoint, v0, omega,
            pitchAirDensity, Vector3(), 1.0f, plate
        );
        assert(ok);
        // Fastball must stick tightly; secondaries get a bit more room.
        float tol = pitch.hotkey == 'F' ? 0.04f : 0.10f;
        assert(std::abs(plate.x - aim.x) < tol);
        assert(std::abs(plate.y - aim.y) < tol);
    }
}

void testFourSeamMoreAccurateThanSliderAtSameNoise() {
    auto pitches = makePitchProfiles();
    const PitchProfile* four = nullptr;
    const PitchProfile* slider = nullptr;
    for (const auto& p : pitches) {
        if (p.hotkey == 'F') four = &p;
        if (p.hotkey == 'S') slider = &p;
    }
    assert(four && slider);

    PitchFlightVariation variation;
    Vector3 aim(0.15f, 1.45f, plateZ);
    Vector3 hand(-0.10f, 1.58f, 0.30f);

    auto plateErr = [&](const PitchProfile& p) {
        Vector3 omega = angularVelocityFromProfile(p, 1.0f);
        Vector3 v0 = calculateLaunchVelocity(p, aim, 92.0f, variation, hand, omega);
        Vector3 plate;
        assert(simulatePlateCrossing(p, hand, v0, omega, pitchAirDensity, {}, 1.0f, plate));
        float dx = plate.x - aim.x;
        float dy = plate.y - aim.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    assert(plateErr(*four) < 0.05f);
    assert(plateErr(*slider) < 0.10f);
}

void testFourSeamRidesRelativeToNoSpin() {
    // With identical launch velocity, backspin finishes higher than zero spin.
    auto pitches = makePitchProfiles();
    const PitchProfile& four = pitches[0];
    assert(four.hotkey == 'F');

    PitchFlightVariation variation;
    Vector3 aim(0.0f, 1.40f, plateZ);
    Vector3 omega = angularVelocityFromProfile(four, 1.0f);
    Vector3 v0 = calculateLaunchVelocity(four, aim, 95.0f, variation, releasePoint, omega);

    PitchProfile noSpin = four;
    noSpin.spinRpm = 0.0f;
    noSpin.magnusScale = 0.0f;

    Vector3 plateSpin, plateNone;
    assert(simulatePlateCrossing(four, releasePoint, v0, omega, pitchAirDensity, {}, 1.0f, plateSpin));
    assert(simulatePlateCrossing(
        noSpin, releasePoint, v0, Vector3(), pitchAirDensity, {}, 1.0f, plateNone
    ));
    assert(plateSpin.y > plateNone.y + 0.04f);
}

}

int main() {
    testPitchesReachPlateNearAim();
    testFourSeamMoreAccurateThanSliderAtSameNoise();
    testFourSeamRidesRelativeToNoSpin();
    return 0;
}
