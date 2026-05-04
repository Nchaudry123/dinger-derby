#pragma once
#include "../math/Vector2.h"

class Body2D {
public:
    Vector2 position;
    Vector2 velocity;
    Vector2 acceleration;

    float mass;
    float radius;
    float restitution;

    bool isSleeping;
    float sleepTimer;
    void wakeUp();

    Body2D();
    Body2D(Vector2 startPosition, float mass);

    void applyForce(const Vector2& force);
    void update(float dt);
};