#include "Collision2D.h"
#include <algorithm>
#include <cmath>

CollisionManifold2D findCircleCircleCollision(const Body2D& a, const Body2D& b) {
    Vector2 difference = b.position - a.position;

    float distance = difference.magnitude();
    float radiusSum = a.radius + b.radius;

    if (distance > radiusSum) {
        return CollisionManifold2D{};
    }

    CollisionManifold2D manifold;
    manifold.colliding = true;
    manifold.penetration = radiusSum - distance;

    if (distance == 0.0f) {
        manifold.normal = Vector2(1.0f, 0.0f);
    } else {
        manifold.normal = difference / distance;
    }

    return manifold;
}

bool circleCircleCollision(const Body2D& a, const Body2D& b) {
    return findCircleCircleCollision(a, b).colliding;
}

void resolveCircleCollision(
    Body2D& a,
    Body2D& b,
    const CollisionManifold2D& manifold
) {
    if (!manifold.colliding) {
        return;
    }

    Vector2 normal = manifold.normal;
    Vector2 relativeVelocity = b.velocity - a.velocity;
    float inverseMassA = a.inverseMass();
    float inverseMassB = b.inverseMass();
    float inverseMassSum = inverseMassA + inverseMassB;

    if (inverseMassSum == 0.0f) {
        return;
    }

    float velocityAlongNormal = relativeVelocity.dot(normal);

    // Only apply bounce/friction if bodies are moving toward each other
    if (velocityAlongNormal <= 0.0f) {
        a.wakeUp();
        b.wakeUp();

        // Match 3D sphere resolution: use the softer of the two materials.
        float restitution = std::min(a.restitution, b.restitution);

        float impulseStrength = -(1.0f + restitution) * velocityAlongNormal;
        impulseStrength /= inverseMassSum;

        Vector2 impulse = normal * impulseStrength;

        a.velocity -= impulse * inverseMassA;
        b.velocity += impulse * inverseMassB;

        // Static and kinetic friction
        Vector2 tangent = relativeVelocity - normal * velocityAlongNormal;

        if (tangent.magnitude() != 0.0f) {
            tangent = tangent.normalized();

            float tangentVelocity = relativeVelocity.dot(tangent);
            float tangentImpulseMagnitude = -tangentVelocity / inverseMassSum;

            float staticFriction = 0.6f;
            float kineticFriction = 0.3f;

            Vector2 frictionImpulse;

            if (std::abs(tangentImpulseMagnitude) < impulseStrength * staticFriction) {
                frictionImpulse = tangent * tangentImpulseMagnitude;
            } else {
                frictionImpulse = tangent * (-impulseStrength * kineticFriction);
            }

            a.velocity -= frictionImpulse * inverseMassA;
            b.velocity += frictionImpulse * inverseMassB;
        }
    }

    // Always correct overlap, even if bodies are moving apart.
    // Use the same positional correction factors as 3D sphere resolution.
    const float slop = 0.001f;
    const float correctionPercent = 0.85f;

    float correctedPenetration = std::max(manifold.penetration - slop, 0.0f);
    float correctionAmount = correctedPenetration / inverseMassSum * correctionPercent;

    Vector2 correction = normal * correctionAmount;

    a.position -= correction * inverseMassA;
    b.position += correction * inverseMassB;
}

void resolveCircleCollision(Body2D& a, Body2D& b) {
    resolveCircleCollision(a, b, findCircleCircleCollision(a, b));
}

bool circleRectangleCollision(
    const Body2D& circle,
    const Vector2& rectPosition,
    float rectWidth,
    float rectHeight
) {
    float left = rectPosition.x - rectWidth / 2.0f;
    float right = rectPosition.x + rectWidth / 2.0f;
    float top = rectPosition.y - rectHeight / 2.0f;
    float bottom = rectPosition.y + rectHeight / 2.0f;

    float closestX = std::clamp(circle.position.x, left, right);
    float closestY = std::clamp(circle.position.y, top, bottom);

    Vector2 closestPoint(closestX, closestY);
    Vector2 difference = circle.position - closestPoint;

    return difference.magnitude() <= circle.radius;
}

void resolveCircleRectangleCollision(
    Body2D& circle,
    const Vector2& rectPosition,
    float rectWidth,
    float rectHeight
) {
    float left = rectPosition.x - rectWidth / 2.0f;
    float right = rectPosition.x + rectWidth / 2.0f;
    float top = rectPosition.y - rectHeight / 2.0f;
    float bottom = rectPosition.y + rectHeight / 2.0f;

    float closestX = std::clamp(circle.position.x, left, right);
    float closestY = std::clamp(circle.position.y, top, bottom);

    Vector2 closestPoint(closestX, closestY);
    Vector2 difference = circle.position - closestPoint;

    float distance = difference.magnitude();

    if (distance > circle.radius) {
        return;
    }

    Vector2 normal;

    if (distance == 0.0f) {
        normal = Vector2(0.0f, -1.0f);
    } else {
        normal = difference.normalized();
    }

    float overlap = circle.radius - distance;
    circle.position += normal * overlap;

    // Reflect only the normal component with restitution, matching wall/sphere response.
    float velocityAlongNormal = circle.velocity.dot(normal);
    if (velocityAlongNormal < 0.0f) {
        circle.velocity -= normal * ((1.0f + circle.restitution) * velocityAlongNormal);
    }
}
