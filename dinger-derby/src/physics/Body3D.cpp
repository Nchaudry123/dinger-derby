#include "Body3D.h"

#include <algorithm>

Body3D::Body3D() = default;

Body3D::Body3D(const Vector3& startPosition, float mass) {
    position = startPosition;
    this->mass = std::max(mass, 0.001f);
}

bool Body3D::isStatic() const {
    return type == Body3DType::Static;
}

float Body3D::inverseMass() const {
    if (isStatic()) {
        return 0.0f;
    }

    return 1.0f / mass;
}

void Body3D::setMass(float newMass) {
    mass = std::max(newMass, 0.001f);
}

void Body3D::setRadius(float newRadius) {
    radius = std::max(newRadius, 0.001f);
}

void Body3D::setStatic() {
    type = Body3DType::Static;
    velocity = Vector3();
    acceleration = Vector3();
}

void Body3D::setDynamic(float newMass) {
    type = Body3DType::Dynamic;
    setMass(newMass);
}

void Body3D::applyForce(const Vector3& force) {
    if (isStatic()) {
        return;
    }

    acceleration += force * inverseMass();
}

void Body3D::update(float dt) {
    if (isStatic()) {
        return;
    }

    velocity += acceleration * dt;

    const float dragCoefficient = 0.12f;
    velocity -= velocity * dragCoefficient * dt;

    position += velocity * dt;
    acceleration = Vector3();
}
