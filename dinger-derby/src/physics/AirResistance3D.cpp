#include "AirResistance3D.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float pi = 3.1415926535f;
constexpr float minimumDragSpeed = 0.0001f;
constexpr float minimumSpinRate = 1e-4f;

Vector3 safeNormalized(const Vector3& v) {
    float m = v.magnitude();
    if (m < 1e-8f) {
        return Vector3();
    }
    return v * (1.0f / m);
}

// Component of angular velocity perpendicular to velocity (active / useful spin).
Vector3 activeSpin(const Body3D& body, const Vector3& relativeVelocity) {
    float speed = relativeVelocity.magnitude();
    if (speed < minimumDragSpeed) {
        return Vector3();
    }
    Vector3 vHat = relativeVelocity * (1.0f / speed);
    Vector3 omega = body.angularVelocity;
    // Remove gyrospin (ω parallel to v) — produces no Magnus force.
    Vector3 omegaActive = omega - vHat * omega.dot(vHat);
    float efficiency = std::clamp(body.spinEfficiency, 0.0f, 1.0f);
    return omegaActive * efficiency;
}

}

float AirResistance3D::crossSectionArea(const Body3D& body) {
    float radius = std::max(body.radius, 0.0f);
    return pi * radius * radius;
}

float AirResistance3D::effectiveDragCoefficient(const Body3D& body, float speed) {
    float baseCoefficient = std::max(body.dragCoefficient, 0.0f);
    float speedResponse = std::clamp((speed - 24.0f) / 52.0f, 0.0f, 1.0f);
    float dragCrisisReduction = 1.0f - 0.18f * speedResponse;

    // Low-spin balls (splitter/knuckle-like) sit slightly higher Cd; high
    // four-seam spin stays in the smoother boundary-layer regime.
    float omega = body.angularVelocity.magnitude();
    float spinRate = omega * std::max(body.radius, 0.001f);
    float spinHelp = std::clamp(spinRate / std::max(speed, 1.0f), 0.0f, 0.35f);
    float spinDragMod = 1.0f - 0.10f * spinHelp * body.spinEfficiency
        + 0.12f * (1.0f - body.spinEfficiency);

    return baseCoefficient * dragCrisisReduction * spinDragMod;
}

float AirResistance3D::spinParameter(const Body3D& body, float relativeSpeed) {
    if (relativeSpeed < minimumDragSpeed) {
        return 0.0f;
    }
    // Approximate with full |ω| * efficiency; caller may pass active |ω|.
    float omega = body.angularVelocity.magnitude() * std::clamp(body.spinEfficiency, 0.0f, 1.0f);
    return (std::max(body.radius, 0.0f) * omega) / relativeSpeed;
}

float AirResistance3D::liftCoefficient(float spinParameter) {
    // Nathan / MLB tracking fit: Cl rises with S then saturates ~0.3–0.4.
    float s = std::max(0.0f, spinParameter);
    return 1.40f * s / (0.40f + s);
}

Vector3 AirResistance3D::calculateDragForce(
    const Body3D& body,
    const Vector3& airVelocity,
    float airDensity
) {
    if (body.isStatic() || body.airResistanceScale <= 0.0f || airDensity <= 0.0f) {
        return Vector3();
    }

    Vector3 relativeVelocity = body.velocity - airVelocity;
    float speed = relativeVelocity.magnitude();

    if (speed <= minimumDragSpeed) {
        return Vector3();
    }

    float dragMagnitude =
        0.5f *
        airDensity *
        effectiveDragCoefficient(body, speed) *
        crossSectionArea(body) *
        speed *
        speed *
        body.airResistanceScale;

    return relativeVelocity.normalized() * -dragMagnitude;
}

Vector3 AirResistance3D::calculateMagnusForce(
    const Body3D& body,
    const Vector3& airVelocity,
    float airDensity
) {
    if (body.isStatic() || airDensity <= 0.0f || body.magnusScale <= 0.0f) {
        return Vector3();
    }

    Vector3 relativeVelocity = body.velocity - airVelocity;
    float speed = relativeVelocity.magnitude();
    if (speed <= minimumDragSpeed) {
        return Vector3();
    }

    Vector3 omegaActive = activeSpin(body, relativeVelocity);
    float omegaMag = omegaActive.magnitude();
    if (omegaMag < minimumSpinRate) {
        return Vector3();
    }

    // Lift direction is ω × v (right-hand rule).
    Vector3 liftDir = omegaActive.cross(relativeVelocity);
    float liftMag = liftDir.magnitude();
    if (liftMag < 1e-8f) {
        return Vector3();
    }
    liftDir = liftDir * (1.0f / liftMag);

    float s = (std::max(body.radius, 0.0f) * omegaMag) / speed;
    float cl = liftCoefficient(s);
    float forceMag =
        0.5f *
        airDensity *
        cl *
        crossSectionArea(body) *
        speed *
        speed *
        body.magnusScale;

    return liftDir * forceMag;
}
