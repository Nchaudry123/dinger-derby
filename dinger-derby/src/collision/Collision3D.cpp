#include "Collision3D.h"

#include <algorithm>

CollisionManifold3D findSphereSphereCollision(const Body3D& a, const Body3D& b) {
    Vector3 difference = b.position - a.position;
    float distance = difference.magnitude();
    float radiusSum = a.radius + b.radius;

    if (distance > radiusSum) {
        return CollisionManifold3D{};
    }

    CollisionManifold3D manifold;
    manifold.colliding = true;
    manifold.penetration = radiusSum - distance;

    if (distance == 0.0f) {
        manifold.normal = Vector3(1.0f, 0.0f, 0.0f);
    } else {
        manifold.normal = difference / distance;
    }

    return manifold;
}

bool sphereSphereCollision(const Body3D& a, const Body3D& b) {
    return findSphereSphereCollision(a, b).colliding;
}

void resolveSphereCollision(
    Body3D& a,
    Body3D& b,
    const CollisionManifold3D& manifold
) {
    if (!manifold.colliding) {
        return;
    }

    float inverseMassA = a.inverseMass();
    float inverseMassB = b.inverseMass();
    float inverseMassSum = inverseMassA + inverseMassB;

    if (inverseMassSum == 0.0f) {
        return;
    }

    Vector3 relativeVelocity = b.velocity - a.velocity;
    float velocityAlongNormal = relativeVelocity.dot(manifold.normal);

    if (velocityAlongNormal < 0.0f) {
        float restitution = std::min(a.restitution, b.restitution);
        float impulseStrength = -(1.0f + restitution) * velocityAlongNormal;
        impulseStrength /= inverseMassSum;

        Vector3 impulse = manifold.normal * impulseStrength;
        a.velocity -= impulse * inverseMassA;
        b.velocity += impulse * inverseMassB;
    }

    const float slop = 0.001f;
    const float correctionPercent = 0.85f;
    float correctedPenetration = std::max(manifold.penetration - slop, 0.0f);
    Vector3 correction =
        manifold.normal * (correctedPenetration / inverseMassSum * correctionPercent);

    a.position -= correction * inverseMassA;
    b.position += correction * inverseMassB;
}

void resolveSphereCollision(Body3D& a, Body3D& b) {
    resolveSphereCollision(a, b, findSphereSphereCollision(a, b));
}
