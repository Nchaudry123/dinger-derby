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

    float mass = 1.0f;
    float radius = 1.0f;
    float restitution = 0.65f;
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
