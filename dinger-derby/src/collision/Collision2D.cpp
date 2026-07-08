#include "Collision2D.h"
#include <algorithm>
#include <cmath>

CollisionManifold findCircleCircleCollision(const Body2D& a, const Body2D& b) {
    Vector2 difference = b.position - a.position;

    float distance = difference.magnitude();
    float radiusSum = a.radius + b.radius;

    if (distance > radiusSum) {
        return CollisionManifold{};
    }

    CollisionManifold manifold;
    manifold.colliding = true;
    manifold.penetration = radiusSum - distance;

    if (distance == 0.0f) {
        manifold.normal = Vector2(1.0f, 0.0f);
    } else {
        manifold.normal = difference * (1.0f / distance);
    }

    return manifold;
}

bool circleCircleCollision(const Body2D& a, const Body2D& b) {
    return findCircleCircleCollision(a, b).colliding;
}

void resolveCircleCollision(Body2D& a, Body2D& b) {
    Vector2 normal = b.position - a.position;

    float distance = normal.magnitude();

    if (distance == 0) {
        // Pick a stable fallback normal so perfectly overlapping bodies can separate.
        normal = Vector2(1.0f, 0.0f);
        distance = 0.0001f;
    } else {
        normal = normal.normalized();
    }

    Vector2 relativeVelocity = b.velocity - a.velocity;

    float velocityAlongNormal =
        relativeVelocity.x * normal.x + relativeVelocity.y * normal.y;

    bool movingApart = velocityAlongNormal > 0;

    // Only apply bounce/friction if bodies are moving toward each other
    if (!movingApart) {
        a.wakeUp();
        b.wakeUp();

        // Normal impulse
        float restitution = 0.6f;

        float impulseStrength = -(1 + restitution) * velocityAlongNormal;
        impulseStrength /= (1 / a.mass) + (1 / b.mass);

        Vector2 impulse = normal * impulseStrength;

        a.velocity = a.velocity - impulse * (1 / a.mass);
        b.velocity = b.velocity + impulse * (1 / b.mass);

        // Static and kinetic friction
        Vector2 tangent = relativeVelocity - normal * velocityAlongNormal;

        if (tangent.magnitude() != 0) {
            tangent = tangent.normalized();

            float tangentVelocity =
                relativeVelocity.x * tangent.x + relativeVelocity.y * tangent.y;

            float tangentImpulseMagnitude =
                -tangentVelocity / ((1 / a.mass) + (1 / b.mass));

            float staticFriction = 0.6f;
            float kineticFriction = 0.3f;

            Vector2 frictionImpulse;

            if (std::abs(tangentImpulseMagnitude) < impulseStrength * staticFriction) {
                frictionImpulse = tangent * tangentImpulseMagnitude;
            } else {
                frictionImpulse = tangent * (-impulseStrength * kineticFriction);
            }

            a.velocity = a.velocity - frictionImpulse * (1 / a.mass);
            b.velocity = b.velocity + frictionImpulse * (1 / b.mass);
        }
    }

    // Always correct overlap, even if bodies are moving apart
    float overlap = (a.radius + b.radius) - distance;

    float slop = 0.001f;
    float percent = 1.0f;

    float correctionAmount =
        (overlap - slop > 0.0f ? overlap - slop : 0.0f)
        / ((1 / a.mass) + (1 / b.mass)) * percent;

    Vector2 correction = normal * correctionAmount;

    a.position = a.position - correction * (1 / a.mass);
    b.position = b.position + correction * (1 / b.mass);
}

float clamp(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
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

    float closestX = clamp(circle.position.x, left, right);
    float closestY = clamp(circle.position.y, top, bottom);

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

    float closestX = clamp(circle.position.x, left, right);
    float closestY = clamp(circle.position.y, top, bottom);

    Vector2 closestPoint(closestX, closestY);
    Vector2 difference = circle.position - closestPoint;

    float distance = difference.magnitude();

    if (distance > circle.radius) {
        return;
    }

    Vector2 normal;

    if (distance == 0) {
        normal = Vector2(0, -1);
    } else {
        normal = difference.normalized();
    }

    float overlap = circle.radius - distance;

    circle.position += normal * overlap;

    float velocityAlongNormal =
        circle.velocity.x * normal.x +
        circle.velocity.y * normal.y;

    if (velocityAlongNormal < 0) {
        circle.velocity =
            circle.velocity -
            normal * (2.0f * velocityAlongNormal);

        circle.velocity =
            circle.velocity * circle.restitution;
    }
}
