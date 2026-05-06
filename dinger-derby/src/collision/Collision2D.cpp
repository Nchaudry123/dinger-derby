#include "Collision2D.h"
#include <algorithm>
#include <cmath>

bool circleCircleCollision(const Body2D& a, const Body2D& b) {
    Vector2 difference = b.position - a.position;

    float distance = difference.magnitude();
    float radiusSum = a.radius + b.radius;

    return distance <= radiusSum;
}

void resolveCircleCollision(Body2D& a, Body2D& b) {
    Vector2 normal = b.position - a.position;

    float distance = normal.magnitude();

    if (distance == 0) {
        return;
    }

    normal = normal.normalized();

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