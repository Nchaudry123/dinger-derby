#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>

#include "../src/math/Vector3.h"
#include "../src/physics/AirResistance3D.h"
#include "../src/physics/Body3D.h"

namespace {

constexpr float fixedStep = 1.0f / 180.0f;
constexpr float baseballRadius = 0.2f;
constexpr float feetPerWorldUnit = 2.0f;
constexpr float plateZ = 60.5f / feetPerWorldUnit;
const Vector3 releasePoint(-0.22f, 1.72f, 0.0f);
const Vector3 aimPoint(0.0f, 1.28f, plateZ);

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
    Vector3 airVelocity;
    Vector3 breakScale;
    float dragScale = 1.0f;
    float liftOffset = 0.0f;
    float turbulencePhase = 0.0f;
    float turbulenceStrength = 0.0f;
};

std::array<PitchProfile, 5> makePitchProfiles() {
    return {{
        PitchProfile{'F', "Four-Seam", 96.1f, 2.0f, 0.08f, Vector3(0.02f, 0.45f, 0.0f), 0.33f, 0.15f, 1.0f},
        PitchProfile{'P', "Splitter", 91.5f, 1.8f, 0.06f, Vector3(0.08f, -3.2f, 0.0f), 0.42f, 0.68f, 1.16f},
        PitchProfile{'C', "Curve", 77.1f, 2.2f, 0.16f, Vector3(-0.18f, -2.15f, 0.0f), 0.46f, 0.46f, 1.24f},
        PitchProfile{'T', "Cutter", 91.4f, 1.9f, 0.05f, Vector3(1.2f, -0.05f, 0.0f), 0.36f, 0.45f, 1.05f},
        PitchProfile{'S', "Slider", 87.2f, 1.7f, 0.04f, Vector3(2.4f, -0.55f, 0.0f), 0.39f, 0.46f, 1.1f}
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

Vector3 predictPlatePosition(
    const PitchProfile& pitch,
    const PitchFlightVariation& variation,
    const Vector3& initialVelocity
) {
    Body3D simulated(releasePoint + variation.releaseOffset, 0.145f);
    simulated.setRadius(baseballRadius);
    simulated.dragCoefficient = pitch.dragCoefficient * variation.dragScale;
    simulated.airResistanceScale = pitch.airScale;
    simulated.velocity = initialVelocity;

    float airDensity = 0.18f * variation.dragScale;
    float pitchAge = 0.0f;
    Vector3 previousPosition = simulated.position;

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

        if (simulated.position.z >= plateZ) {
            float segmentLength = simulated.position.z - previousPosition.z;
            float t = segmentLength <= 0.0f
                ? 1.0f
                : (plateZ - previousPosition.z) / segmentLength;
            return previousPosition + (simulated.position - previousPosition) * std::clamp(t, 0.0f, 1.0f);
        }
    }

    return simulated.position;
}

Vector3 calculateLaunchVelocity(
    const PitchProfile& pitch,
    float pitchSpeedMph,
    const PitchFlightVariation& variation
) {
    float pitchSpeed = mphToWorldUnitsPerSecond(pitchSpeedMph);
    Vector3 actualReleasePoint = releasePoint + variation.releaseOffset;
    float distance = aimPoint.z - actualReleasePoint.z;
    float flightTime = distance / pitchSpeed;
    Vector3 velocity(
        (aimPoint.x - actualReleasePoint.x) / flightTime,
        (aimPoint.y - actualReleasePoint.y) / flightTime + pitch.liftCompensation + variation.liftOffset,
        pitchSpeed
    );

    for (int i = 0; i < 6; i++) {
        Vector3 predicted = predictPlatePosition(pitch, variation, velocity);
        Vector3 error = aimPoint - predicted;
        velocity.x += error.x / flightTime * 0.85f;
        velocity.y += error.y / flightTime * 0.85f;
    }

    return velocity;
}

void testYamamotoPitchesReachPlateTarget() {
    std::array<PitchProfile, 5> pitches = makePitchProfiles();
    std::array<PitchFlightVariation, 3> variations = {{
        PitchFlightVariation{
            Vector3(-0.045f, -0.035f, 0.0f),
            Vector3(-0.22f, -0.04f, -0.12f),
            Vector3(0.84f, 0.84f, 1.0f),
            0.94f,
            -0.08f,
            0.0f,
            0.12f
        },
        PitchFlightVariation{
            Vector3(0.045f, 0.035f, 0.0f),
            Vector3(0.22f, 0.04f, 0.08f),
            Vector3(1.16f, 1.16f, 1.0f),
            1.08f,
            0.08f,
            1.7f,
            0.52f
        },
        PitchFlightVariation{
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(),
            Vector3(1.0f, 1.0f, 1.0f),
            1.0f,
            0.0f,
            3.1f,
            0.28f
        }
    }};

    for (const PitchProfile& pitch : pitches) {
        for (const PitchFlightVariation& variation : variations) {
            Vector3 velocity = calculateLaunchVelocity(pitch, pitch.baseSpeedMph, variation);
            Vector3 platePosition = predictPlatePosition(pitch, variation, velocity);

            assert(platePosition.z >= plateZ - 0.01f);
            assert(std::abs(platePosition.x - aimPoint.x) < 0.08f);
            assert(std::abs(platePosition.y - aimPoint.y) < 0.08f);
        }
    }
}

}

int main() {
    testYamamotoPitchesReachPlateTarget();
    return 0;
}
