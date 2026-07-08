#pragma once

#include <vector>

#include "../math/Vector3.h"
#include "../rendering/Mesh3D.h"

struct SoftBodyParticle3D {
    Vector3 position;
    Vector3 previousPosition;
    Vector3 restPosition;
    Vector3 velocity;
    float inverseMass = 1.0f;
};

struct SoftBodySpring3D {
    int a = 0;
    int b = 0;
    float restLength = 0.0f;
    float stiffness = 1.0f;
    float damping = 0.1f;
};

class SoftBodyMesh3D {
public:
    std::vector<SoftBodyParticle3D> particles;
    std::vector<SoftBodySpring3D> springs;
    std::vector<Triangle3D> triangles;
    std::vector<sf::Color> triangleColors;

    float shapeStiffness = 16.0f;
    float velocityDamping = 0.985f;

    static SoftBodyMesh3D cube(float size = 2.0f);

    void step(float deltaTime);
    void applyImpulse(const Vector3& center, float radius, const Vector3& impulse);
    Mesh3D toMesh() const;

private:
    void addSpring(int a, int b, float stiffness, float damping);
};
