#include "Body2D.h"

#include <algorithm>
#include <cmath>

Body2D::Body2D() {
    updateMomentOfInertia();
}

Body2D::Body2D(const Vector2& startPosition, float mass) {
    position = startPosition;
    this->mass = std::max(mass, 0.001f);
    updateMomentOfInertia();
}

bool Body2D::isStatic() const {
    return type == Body2DType::Static;
}

float Body2D::inverseMass() const {
    if (isStatic()) {
        return 0.0f;
    }

    return 1.0f / mass;
}

void Body2D::setMass(float newMass) {
    mass = std::max(newMass, 0.001f);
    updateMomentOfInertia();
}

void Body2D::setRadius(float newRadius) {
    radius = std::max(newRadius, 0.001f);
    updateMomentOfInertia();
}

void Body2D::setStatic() {
    type = Body2DType::Static;
    velocity = Vector2(0.0f, 0.0f);
    acceleration = Vector2(0.0f, 0.0f);
    angularVelocity = 0.0f;
    angularAcceleration = 0.0f;
    torque = 0.0f;
    isSleeping = false;
    sleepTimer = 0.0f;
}

void Body2D::setDynamic(float newMass) {
    type = Body2DType::Dynamic;
    setMass(newMass);
    wakeUp();
}

void Body2D::applyForce(const Vector2& force) {
    if (isSleeping || isStatic()) {
        return;
    }

    acceleration += force * inverseMass();
}

void Body2D::applyTorque(float torqueAmount) {
    if (isSleeping || isStatic()) {
        return;
    }

    torque += torqueAmount;
}

void Body2D::update(float dt) {
    if (isSleeping || isStatic()) {
        return;
    }

    velocity += acceleration * dt;

    float dragCoefficient = 0.2f;
    velocity = velocity - velocity * dragCoefficient * dt;

    position += velocity * dt;

    angularAcceleration = torque / momentOfInertia;
    angularVelocity += angularAcceleration * dt;

    float angularDamping = 0.5f;
    angularVelocity -= angularVelocity * angularDamping * dt;

    rotation += angularVelocity * dt;

    acceleration = Vector2(0.0f, 0.0f);
    torque = 0.0f;

    float speed = velocity.magnitude();

    if (speed < 5.0f && std::abs(angularVelocity) < 0.1f) {
        sleepTimer += dt;

        if (sleepTimer > 1.0f) {
            velocity = Vector2(0.0f, 0.0f);
            acceleration = Vector2(0.0f, 0.0f);
            angularVelocity = 0.0f;
            angularAcceleration = 0.0f;
            torque = 0.0f;
            isSleeping = true;
        }
    } else {
        sleepTimer = 0.0f;
    }
}

void Body2D::wakeUp() {
    isSleeping = false;
    sleepTimer = 0.0f;
}

void Body2D::updateMomentOfInertia() {
    momentOfInertia = 0.5f * mass * radius * radius;
}
