#pragma once

#include "../physics/Body2D.h"

struct CollisionManifold {
    bool colliding = false;
    Vector2 normal;
    float penetration = 0.0f;
};

CollisionManifold findCircleCircleCollision(
    const Body2D& a,
    const Body2D& b
);

// Checks if two circular bodies are colliding
bool circleCircleCollision(
    const Body2D& a,
    const Body2D& b
);

// Resolves collision (applies bounce)
void resolveCircleCollision(
    Body2D& a,
    Body2D& b,
    const CollisionManifold& manifold
);

void resolveCircleCollision(
    Body2D& a,
    Body2D& b
);
// Circle-rectangle collision
bool circleRectangleCollision(
    const Body2D& circle,
    const Vector2& rectPosition,
    float rectWidth,
    float rectHeight
);
// Resolves collision
void resolveCircleRectangleCollision(
    Body2D& circle,
    const Vector2& rectPosition,
    float rectWidth,
    float rectHeight
);
