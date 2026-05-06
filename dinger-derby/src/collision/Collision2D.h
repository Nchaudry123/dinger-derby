#pragma once

#include "../physics/Body2D.h"

// Checks if two circular bodies are colliding
bool circleCircleCollision(
    const Body2D& a,
    const Body2D& b
);

// Resolves collision (applies bounce)
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