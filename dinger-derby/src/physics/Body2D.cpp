#include "Body2D.h"
#include <cmath>
Body2D::Body2D() {
    position = Vector2(0, 0);
    velocity = Vector2(0, 0);
    acceleration = Vector2(0, 0);

    mass = 1.0f;
    radius = 1.0f;
    restitution = 0.7f;

    rotation = 0.0f;
    angularVelocity = 0.0f;
    angularAcceleration = 0.0f;
    torque = 0.0f;

    momentOfInertia = 0.5f * mass * radius * radius;

    isSleeping = false;
    sleepTimer = 0.0f;
}

Body2D::Body2D(Vector2 startPosition, float mass) {
    position = startPosition;
    velocity = Vector2(0, 0);
    acceleration = Vector2(0, 0);

    this->mass = mass;
    radius = 1.0f;
    restitution = 0.7f;

    rotation = 0.0f;
    angularVelocity = 0.0f;
    angularAcceleration = 0.0f;
    torque = 0.0f;

    momentOfInertia = 0.5f * mass * radius * radius;

    isSleeping = false;
    sleepTimer = 0.0f;
}

void Body2D::applyForce(const Vector2& force) {
    if (isSleeping) {
        return;
    }

    acceleration += force * (1.0f / mass);
}

void Body2D::applyTorque(float torqueAmount) {
    if (isSleeping) {
        return;
    }

    torque += torqueAmount;
}

void Body2D::update(float dt) {
    if (isSleeping) {
        return;
    }

    // Keep rotational inertia in sync with any runtime radius changes.
    momentOfInertia = 0.5f * mass * radius * radius;

    velocity += acceleration * dt;

    float dragCoefficient = 0.2f;
    velocity = velocity - velocity * dragCoefficient * dt;

    position += velocity * dt;

    angularAcceleration = torque / momentOfInertia;
    angularVelocity += angularAcceleration * dt;

    float angularDamping = 0.5f;
    angularVelocity -= angularVelocity * angularDamping * dt;

    rotation += angularVelocity * dt;

    acceleration = Vector2(0, 0);
    torque = 0.0f;

    float speed = velocity.magnitude();

    if (speed < 5.0f && std::abs(angularVelocity) < 0.1f) {
        sleepTimer += dt;

        if (sleepTimer > 1.0f) {
            velocity = Vector2(0, 0);
            acceleration = Vector2(0, 0);
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
