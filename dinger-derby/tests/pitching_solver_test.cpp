#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
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
    Vector3 releaseOffset;
    Vector3 commandOffset;
    Vector3 airVelocity;
    float spinRpmScale = 1.0f;
    float dragScale = 1.0f;
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

struct FlightResult {
    Vector3 platePosition;
    float apexY = releasePoint.y;
    bool crossedPlate = false;
};

FlightResult simulatePitch(
    const PitchProfile& pitch,
    const PitchFlightVariation& variation,
    const Vector3& initialVelocity
) {
    PhysicsWorld3D world;
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(pitchAirDensity * variation.dragScale, variation.airVelocity);
    world.airResistanceEnabled = true;
    world.setBounds(Vector3(-8.0f, -4.0f, -2.0f), Vector3(8.0f, 6.0f, plateZ + 6.0f));

    Body3D simulated(releasePoint + variation.releaseOffset, 0.145f);
    simulated.setRadius(baseballRadius);
    simulated.dragCoefficient = pitch.dragCoefficient * variation.dragScale;
    simulated.airResistanceScale = pitch.airScale;
    simulated.spinEfficiency = pitch.spinEfficiency;
    simulated.magnusScale = pitch.magnusScale;
    simulated.angularVelocity = angularVelocityFromProfile(pitch, variation.spinRpmScale);
    simulated.velocity = initialVelocity;
    world.addBody(&simulated);

    float apexY = simulated.position.y;
    Vector3 previousPosition = simulated.position;

    for (int step = 0; step < 720; step++) {
        previousPosition = simulated.position;
        world.step(fixedStep);
        apexY = std::max(apexY, simulated.position.y);

        if (simulated.position.z >= plateZ) {
            float segmentLength = simulated.position.z - previousPosition.z;
            float t = segmentLength <= 0.0f
                ? 1.0f
                : (plateZ - previousPosition.z) / segmentLength;
            return FlightResult{
                previousPosition + (simulated.position - previousPosition) * std::clamp(t, 0.0f, 1.0f),
                apexY,
                true
            };
        }
    }

    return FlightResult{simulated.position, apexY, false};
}

Vector3 calculateLaunchVelocity(
    const PitchProfile& pitch,
    const Vector3& aimPoint,
    float pitchSpeedMph,
    const PitchFlightVariation& variation
) {
    float pitchSpeed = mphToWorldUnitsPerSecond(pitchSpeedMph);
    Vector3 actualReleasePoint = releasePoint + variation.releaseOffset;
    float distance = aimPoint.z - actualReleasePoint.z;
    float dragSlowdownEstimate = std::clamp(0.95f - pitch.dragCoefficient * pitch.airScale * 0.10f, 0.86f, 0.95f);
    float flightTime = distance / std::max(1.0f, pitchSpeed * dragSlowdownEstimate);

    Body3D probe;
    probe.setRadius(baseballRadius);
    probe.setMass(0.145f);
    probe.velocity = Vector3(0.0f, 0.0f, pitchSpeed);
    probe.angularVelocity = angularVelocityFromProfile(pitch, variation.spinRpmScale);
    probe.spinEfficiency = pitch.spinEfficiency;
    probe.magnusScale = pitch.magnusScale;
    Vector3 magnusA = AirResistance3D::calculateMagnusForce(probe, Vector3(), pitchAirDensity)
        * probe.inverseMass();

    float estimatedAx = magnusA.x * 0.55f;
    float estimatedAy = -9.8f + magnusA.y * 0.55f;
    float t2 = flightTime * flightTime;
    float desiredVx = (aimPoint.x - actualReleasePoint.x - 0.5f * estimatedAx * t2) / flightTime;
    float desiredVy = (aimPoint.y - actualReleasePoint.y - 0.5f * estimatedAy * t2) / flightTime;

    float maxSideVelocity = pitchSpeed * 0.20f;
    float minVerticalVelocity = pitchSpeed * std::tan(-10.0f * pi / 180.0f);
    float maxVerticalVelocity = pitchSpeed * std::tan(22.0f * pi / 180.0f);
    desiredVx = std::clamp(desiredVx, -maxSideVelocity, maxSideVelocity);
    desiredVy = std::clamp(desiredVy, minVerticalVelocity, maxVerticalVelocity);

    float forwardVelocitySquared = pitchSpeed * pitchSpeed - desiredVx * desiredVx - desiredVy * desiredVy;
    float desiredVz = std::sqrt(std::max(forwardVelocitySquared, pitchSpeed * pitchSpeed * 0.55f));
    return Vector3(desiredVx, desiredVy, desiredVz);
}

void testYamamotoPitchesUseRealisticReleaseAndReachPlate() {
    std::array<PitchProfile, 5> pitches = makePitchProfiles();
    std::array<Vector3, 3> aimPoints = {{
        Vector3(0.0f, 1.28f, plateZ),
        Vector3(0.0f, 1.63f, plateZ),
        Vector3(0.0f, 2.23f, plateZ)
    }};

    PitchFlightVariation variation;
    for (const PitchProfile& pitch : pitches) {
        for (const Vector3& aim : aimPoints) {
            Vector3 v0 = calculateLaunchVelocity(pitch, aim, pitch.baseSpeedMph, variation);
            FlightResult result = simulatePitch(pitch, variation, v0);
            assert(result.crossedPlate);
            assert(result.platePosition.y > 0.2f);
            assert(result.platePosition.y < 3.5f);
            assert(std::abs(result.platePosition.x) < 2.5f);
        }
    }
}

void testFourSeamRidesHigherThanCurve() {
    // Same aim / speed band: four-seam backspin should finish higher than curve topspin.
    auto pitches = makePitchProfiles();
    const PitchProfile* four = nullptr;
    const PitchProfile* curve = nullptr;
    for (const auto& p : pitches) {
        if (p.hotkey == 'F') four = &p;
        if (p.hotkey == 'C') curve = &p;
    }
    assert(four && curve);

    PitchFlightVariation variation;
    Vector3 aim(0.0f, 1.40f, plateZ);

    // Same launch velocity so Magnus difference is isolated.
    Vector3 v0 = calculateLaunchVelocity(*four, aim, 90.0f, variation);
    FlightResult f = simulatePitch(*four, variation, v0);
    FlightResult c = simulatePitch(*curve, variation, v0);
    assert(f.crossedPlate && c.crossedPlate);
    assert(f.platePosition.y > c.platePosition.y + 0.05f);
}

void testSliderBreaksGloveSideRelativeToFourSeam() {
    auto pitches = makePitchProfiles();
    const PitchProfile* four = nullptr;
    const PitchProfile* slider = nullptr;
    for (const auto& p : pitches) {
        if (p.hotkey == 'F') four = &p;
        if (p.hotkey == 'S') slider = &p;
    }
    assert(four && slider);

    PitchFlightVariation variation;
    Vector3 aim(0.0f, 1.40f, plateZ);
    Vector3 v0 = calculateLaunchVelocity(*four, aim, 90.0f, variation);
    FlightResult f = simulatePitch(*four, variation, v0);
    FlightResult s = simulatePitch(*slider, variation, v0);
    assert(f.crossedPlate && s.crossedPlate);
    // RHP slider: glove side = −X relative to four-seam.
    assert(s.platePosition.x < f.platePosition.x - 0.03f);
}

}

int main() {
    testYamamotoPitchesUseRealisticReleaseAndReachPlate();
    testFourSeamRidesHigherThanCurve();
    testSliderBreaksGloveSideRelativeToFourSeam();
    return 0;
}
