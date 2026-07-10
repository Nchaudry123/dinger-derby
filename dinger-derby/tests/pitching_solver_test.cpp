#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>

#include "math/Vector3.h"
#include "physics/AirResistance3D.h"
#include "physics/Body3D.h"

namespace {

constexpr float fixedStep = 1.0f / 180.0f;
constexpr float baseballRadius = 0.2f;
constexpr float feetPerWorldUnit = 2.0f;
constexpr float pitchAirDensity = 0.075f;
constexpr float plateZ = 60.5f / feetPerWorldUnit;
const Vector3 releasePoint(-0.22f, 1.72f, 0.0f);

struct PitchProfile {
    char hotkey;
    std::string name;
    float baseSpeedMph;
    float speedVarianceMph;
    float liftCompensation;
    Vector3 breakAcceleration;
    float breakStartZ;
    float dragCoefficient;
    float airScale;
};

struct PitchFlightVariation {
    Vector3 releaseOffset;
    Vector3 commandOffset;
    Vector3 airVelocity;
    Vector3 breakScale;
    float dragScale = 1.0f;
    float liftOffset = 0.0f;
    float turbulencePhase = 0.0f;
    float turbulenceStrength = 0.0f;
};

std::array<PitchProfile, 5> makePitchProfiles() {
    return {{
        PitchProfile{'F', "Four-Seam", 96.1f, 2.0f, 0.06f, Vector3(0.02f, 0.24f, 0.0f), 0.34f, 0.12f, 0.82f},
        PitchProfile{'P', "Splitter", 91.5f, 1.8f, 0.02f, Vector3(0.10f, -1.18f, 0.0f), 0.62f, 0.34f, 0.88f},
        PitchProfile{'C', "Curve", 77.1f, 2.2f, 0.08f, Vector3(-0.14f, -1.28f, 0.0f), 0.50f, 0.30f, 0.92f},
        PitchProfile{'T', "Cutter", 91.4f, 1.9f, 0.03f, Vector3(0.72f, -0.06f, 0.0f), 0.42f, 0.28f, 0.82f},
        PitchProfile{'S', "Slider", 87.2f, 1.7f, 0.03f, Vector3(1.45f, -0.28f, 0.0f), 0.48f, 0.30f, 0.86f}
    }};
}

float mphToWorldUnitsPerSecond(float mph) {
    return mph * 5280.0f / 3600.0f / feetPerWorldUnit;
}

Vector3 movementAccelerationForPitch(
    const PitchProfile& pitch,
    const PitchFlightVariation& variation,
    const Vector3& position,
    float pitchAge
) {
    float progress = std::clamp(
        (position.z - releasePoint.z) / (plateZ - releasePoint.z),
        0.0f,
        1.0f
    );
    float breakRamp = progress <= pitch.breakStartZ
        ? 0.0f
        : (progress - pitch.breakStartZ) / (1.0f - pitch.breakStartZ);
    breakRamp = breakRamp * breakRamp * (3.0f - 2.0f * breakRamp);

    Vector3 acceleration = Vector3(
        pitch.breakAcceleration.x * variation.breakScale.x,
        pitch.breakAcceleration.y * variation.breakScale.y,
        0.0f
    ) * breakRamp;
    float turbulenceRamp = progress * progress;
    Vector3 turbulenceForce(
        std::sin(pitchAge * 18.0f + variation.turbulencePhase),
        std::sin(pitchAge * 13.0f + variation.turbulencePhase * 0.7f),
        0.0f
    );

    return acceleration + turbulenceForce * variation.turbulenceStrength * turbulenceRamp;
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
    Body3D simulated(releasePoint + variation.releaseOffset, 0.145f);
    simulated.setRadius(baseballRadius);
    simulated.dragCoefficient = pitch.dragCoefficient * variation.dragScale;
    simulated.airResistanceScale = pitch.airScale;
    simulated.velocity = initialVelocity;

    float airDensity = pitchAirDensity * variation.dragScale;
    float pitchAge = 0.0f;
    Vector3 previousPosition = simulated.position;
    float apexY = simulated.position.y;

    for (int step = 0; step < 720; step++) {
        previousPosition = simulated.position;
        Vector3 dragForce = AirResistance3D::calculateDragForce(
            simulated,
            variation.airVelocity,
            airDensity
        );
        Vector3 acceleration =
            Vector3(0.0f, -9.8f, 0.0f) +
            movementAccelerationForPitch(pitch, variation, simulated.position, pitchAge) +
            dragForce * simulated.inverseMass();

        simulated.velocity += acceleration * fixedStep;
        simulated.position += simulated.velocity * fixedStep;
        pitchAge += fixedStep;
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
    float movementInfluence = std::max(0.0f, 1.0f - pitch.breakStartZ) * 0.24f;
    float estimatedAx = pitch.breakAcceleration.x * variation.breakScale.x * movementInfluence;
    float estimatedAy = -9.8f + pitch.breakAcceleration.y * variation.breakScale.y * movementInfluence;
    float desiredVx = (aimPoint.x - actualReleasePoint.x - 0.5f * estimatedAx * flightTime * flightTime) / flightTime;
    float desiredVy =
        (aimPoint.y - actualReleasePoint.y - 0.5f * estimatedAy * flightTime * flightTime) / flightTime +
        pitch.liftCompensation +
        variation.liftOffset;

    float maxSideVelocity = pitchSpeed * 0.16f;
    float minVerticalVelocity = pitchSpeed * std::tan(-4.0f * 3.1415926535f / 180.0f);
    float maxVerticalVelocity = pitchSpeed * std::tan(6.8f * 3.1415926535f / 180.0f);
    desiredVx = std::clamp(desiredVx, -maxSideVelocity, maxSideVelocity);
    desiredVy = std::clamp(desiredVy, minVerticalVelocity, maxVerticalVelocity);

    float forwardVelocitySquared = pitchSpeed * pitchSpeed - desiredVx * desiredVx - desiredVy * desiredVy;
    float desiredVz = std::sqrt(std::max(forwardVelocitySquared, pitchSpeed * pitchSpeed * 0.82f));
    return Vector3(desiredVx, desiredVy, desiredVz);
}

void testYamamotoPitchesUseRealisticReleaseAndReachPlate() {
    std::array<PitchProfile, 5> pitches = makePitchProfiles();
    std::array<Vector3, 3> aimPoints = {{
        Vector3(0.0f, 1.28f, plateZ),
        Vector3(0.0f, 1.63f, plateZ),
        Vector3(0.0f, 2.23f, plateZ)
    }};
    std::array<PitchFlightVariation, 3> variations = {{
        PitchFlightVariation{
            Vector3(-0.045f, -0.035f, 0.0f),
            Vector3(-0.08f, -0.04f, 0.0f),
            Vector3(-0.22f, -0.04f, -0.12f),
            Vector3(0.84f, 0.84f, 1.0f),
            0.94f,
            -0.035f,
            0.0f,
            0.08f
        },
        PitchFlightVariation{
            Vector3(0.045f, 0.035f, 0.0f),
            Vector3(0.08f, 0.04f, 0.0f),
            Vector3(0.22f, 0.04f, 0.08f),
            Vector3(1.16f, 1.16f, 1.0f),
            1.08f,
            0.035f,
            1.7f,
            0.24f
        },
        PitchFlightVariation{
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(),
            Vector3(),
            Vector3(1.0f, 1.0f, 1.0f),
            1.0f,
            0.0f,
            3.1f,
            0.16f
        }
    }};

    for (const PitchProfile& pitch : pitches) {
        for (const Vector3& aimPoint : aimPoints) {
            for (const PitchFlightVariation& variation : variations) {
                Vector3 commandedAimPoint = aimPoint + variation.commandOffset;
                commandedAimPoint.z = plateZ;
                Vector3 velocity = calculateLaunchVelocity(pitch, commandedAimPoint, pitch.baseSpeedMph, variation);
                FlightResult result = simulatePitch(pitch, variation, velocity);
                float horizontalSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
                float launchAngleDegrees = std::atan2(velocity.y, horizontalSpeed) * 180.0f / 3.1415926535f;

                assert(result.crossedPlate);
                assert(result.platePosition.z >= plateZ - 0.01f);
                assert(result.platePosition.y > std::max(0.45f, commandedAimPoint.y - 0.65f));
                assert(result.platePosition.y < commandedAimPoint.y + 0.55f);
                assert(std::abs(result.platePosition.x - commandedAimPoint.x) < 1.05f);
                assert(std::abs(result.platePosition.x - aimPoint.x) < 1.15f);
                assert(launchAngleDegrees > -4.2f);
                assert(launchAngleDegrees < 7.0f);
                assert(result.apexY < std::max(releasePoint.y + 1.25f, aimPoint.y + 0.55f));
            }
        }
    }
}

}

int main() {
    testYamamotoPitchesUseRealisticReleaseAndReachPlate();
    return 0;
}
