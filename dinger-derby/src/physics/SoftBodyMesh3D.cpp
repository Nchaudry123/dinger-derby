#include "SoftBodyMesh3D.h"

#include <algorithm>

namespace {

float distanceBetween(const Vector3& a, const Vector3& b) {
    return (b - a).magnitude();
}

}

SoftBodyMesh3D SoftBodyMesh3D::cube(float size) {
    Mesh3D source = Mesh3D::cube(size);

    SoftBodyMesh3D body;
    body.particles.reserve(source.vertices.size());
    body.triangles = source.triangles;
    body.triangleColors = source.triangleColors;

    for (const Vector3& vertex : source.vertices) {
        SoftBodyParticle3D particle;
        particle.position = vertex;
        particle.previousPosition = vertex;
        particle.restPosition = vertex;
        body.particles.push_back(particle);
    }

    for (const Edge3D& edge : source.edges) {
        body.addSpring(edge.start, edge.end, 90.0f, 1.8f);
    }

    for (const Triangle3D& triangle : source.triangles) {
        body.addSpring(triangle.a, triangle.b, 65.0f, 1.2f);
        body.addSpring(triangle.b, triangle.c, 65.0f, 1.2f);
        body.addSpring(triangle.c, triangle.a, 65.0f, 1.2f);
    }

    body.addSpring(0, 6, 45.0f, 1.0f);
    body.addSpring(1, 7, 45.0f, 1.0f);
    body.addSpring(2, 4, 45.0f, 1.0f);
    body.addSpring(3, 5, 45.0f, 1.0f);

    return body;
}

void SoftBodyMesh3D::step(float deltaTime) {
    if (deltaTime <= 0.0f) {
        return;
    }

    deltaTime = std::min(deltaTime, 1.0f / 30.0f);
    std::vector<Vector3> forces(particles.size());

    for (int i = 0; i < particles.size(); i++) {
        SoftBodyParticle3D& particle = particles[i];
        forces[i] += (particle.restPosition - particle.position) * shapeStiffness;
    }

    for (const SoftBodySpring3D& spring : springs) {
        SoftBodyParticle3D& particleA = particles[spring.a];
        SoftBodyParticle3D& particleB = particles[spring.b];

        Vector3 delta = particleB.position - particleA.position;
        float length = delta.magnitude();

        if (length == 0.0f) {
            continue;
        }

        Vector3 direction = delta / length;
        float stretch = length - spring.restLength;
        float relativeVelocity = (particleB.velocity - particleA.velocity).dot(direction);
        Vector3 force = direction * (stretch * spring.stiffness + relativeVelocity * spring.damping);

        forces[spring.a] += force;
        forces[spring.b] -= force;
    }

    for (int i = 0; i < particles.size(); i++) {
        SoftBodyParticle3D& particle = particles[i];

        if (particle.inverseMass <= 0.0f) {
            continue;
        }

        particle.previousPosition = particle.position;
        particle.velocity += forces[i] * particle.inverseMass * deltaTime;
        particle.velocity = particle.velocity * velocityDamping;
        particle.position += particle.velocity * deltaTime;
    }
}

void SoftBodyMesh3D::applyImpulse(const Vector3& center, float radius, const Vector3& impulse) {
    if (radius <= 0.0f) {
        return;
    }

    for (SoftBodyParticle3D& particle : particles) {
        Vector3 offset = particle.position - center;
        float distance = offset.magnitude();

        if (distance > radius) {
            continue;
        }

        float falloff = 1.0f - distance / radius;
        particle.velocity += impulse * (falloff * particle.inverseMass);
    }
}

Mesh3D SoftBodyMesh3D::toMesh() const {
    Mesh3D mesh;
    mesh.vertices.reserve(particles.size());
    mesh.edges.reserve(springs.size());

    for (const SoftBodyParticle3D& particle : particles) {
        mesh.vertices.push_back(particle.position);
    }

    for (const SoftBodySpring3D& spring : springs) {
        mesh.edges.push_back({spring.a, spring.b});
    }

    mesh.triangles = triangles;
    mesh.triangleColors = triangleColors;

    return mesh;
}

void SoftBodyMesh3D::addSpring(int a, int b, float stiffness, float damping) {
    if (a == b) {
        return;
    }

    int start = std::min(a, b);
    int end = std::max(a, b);

    for (const SoftBodySpring3D& spring : springs) {
        if (spring.a == start && spring.b == end) {
            return;
        }
    }

    SoftBodySpring3D spring;
    spring.a = start;
    spring.b = end;
    spring.restLength = distanceBetween(particles[start].position, particles[end].position);
    spring.stiffness = stiffness;
    spring.damping = damping;
    springs.push_back(spring);
}
