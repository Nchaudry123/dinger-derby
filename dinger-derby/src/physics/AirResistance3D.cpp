#include "AirResistance3D.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float pi = 3.1415926535f;
constexpr float minimumDragSpeed = 0.0001f;

}

float AirResistance3D::crossSectionArea(const Body3D& body) {
    float radius = std::max(body.radius, 0.0f);
    return pi * radius * radius;
}

float AirResistance3D::effectiveDragCoefficient(const Body3D& body, float speed) {
    float baseCoefficient = std::max(body.dragCoefficient, 0.0f);
    float speedResponse = std::clamp((speed - 24.0f) / 52.0f, 0.0f, 1.0f);
    float dragCrisisReduction = 1.0f - 0.18f * speedResponse;

    return baseCoefficient * dragCrisisReduction;
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
