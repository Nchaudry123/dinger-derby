#include "Body2D.h"

Body2D::Body2D() {
    position = Vector2(0, 0);
    velocity = Vector2(0, 0);
    acceleration = Vector2(0, 0);

    mass = 1.0f;
    radius = 1.0f;
    restitution = 0.7f;

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

    isSleeping = false;
    sleepTimer = 0.0f;
}

void Body2D::applyForce(const Vector2& force) {
    if (isSleeping) {
        return;
    }

    acceleration += force * (1.0f / mass);
}

void Body2D::update(float dt) {
    if (isSleeping) {
        return;
    }

    velocity += acceleration * dt;

    // Simple linear drag / damping
    float dragCoefficient = 0.2f;
    velocity = velocity - velocity * dragCoefficient * dt;

    position += velocity * dt;

    acceleration = Vector2(0, 0);

    float speed = velocity.magnitude();

    if (speed < 5.0f) {
        sleepTimer += dt;

        if (sleepTimer > 1.0f) {
            velocity = Vector2(0, 0);
            acceleration = Vector2(0, 0);
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