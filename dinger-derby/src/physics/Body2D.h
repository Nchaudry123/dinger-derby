#pragma once
#include "../math/Vector2.h"

enum class BodyType {
    Dynamic,
    Static
};

class Body2D {
public:
    Vector2 position;
    Vector2 velocity;
    Vector2 acceleration;

    float mass;
    float radius;
    float restitution;

    float rotation;
    float angularVelocity;
    float angularAcceleration;
    float torque;
    float momentOfInertia;

    bool isSleeping;
    float sleepTimer;
    BodyType type;

    Body2D();
    Body2D(Vector2 startPosition, float mass);

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
