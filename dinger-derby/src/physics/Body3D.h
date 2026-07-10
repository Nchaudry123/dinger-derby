#pragma once

#include "../math/Vector3.h"

enum class Body3DType {
    Dynamic,
    Static
};

class Body3D {
public:
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    // World-space spin (rad/s). Drives Magnus force when non-zero.
    Vector3 angularVelocity;
    // 0..1: fraction of spin that is "active" (useful for Magnus). Gyro-heavy
    // pitches like splitters sit lower; pure backspin four-seams near 1.
    float spinEfficiency = 1.0f;
    // Multiplier on Magnus force after Cl(S) (pitch-family tuning).
    float magnusScale = 1.0f;

    float mass = 1.0f;
    float radius = 1.0f;
    float restitution = 0.65f;
    float dragCoefficient = 0.47f;
    float airResistanceScale = 1.0f;
    Body3DType type = Body3DType::Dynamic;

    Body3D();
    Body3D(const Vector3& startPosition, float mass);

    bool isStatic() const;
    float inverseMass() const;
    void setMass(float newMass);
    void setRadius(float newRadius);
    void setStatic();
    void setDynamic(float newMass);
    void applyForce(const Vector3& force);
    void update(float dt);
};
