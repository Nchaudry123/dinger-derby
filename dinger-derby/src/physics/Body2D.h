#pragma once

#include "../math/Vector2.h"

enum class Body2DType {
    Dynamic,
    Static
};

class Body2D {
public:
    Vector2 position;
    Vector2 velocity;
    Vector2 acceleration;

    float mass = 1.0f;
    float radius = 1.0f;
    float restitution = 0.7f;

    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float angularAcceleration = 0.0f;
    float torque = 0.0f;
    float momentOfInertia = 0.5f;

    bool isSleeping = false;
    float sleepTimer = 0.0f;
    Body2DType type = Body2DType::Dynamic;

    Body2D();
    Body2D(const Vector2& startPosition, float mass);

    bool isStatic() const;
    float inverseMass() const;
    void setMass(float newMass);
    void setRadius(float newRadius);
    void setStatic();
    void setDynamic(float newMass);
    void applyForce(const Vector2& force);
    void applyTorque(float torque);
    void update(float dt);
    void wakeUp();

private:
    void updateMomentOfInertia();
};
