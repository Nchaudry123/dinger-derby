#pragma once

#include "../math/Vector3.h"
#include "../physics/Body3D.h"

struct CollisionManifold3D {
    bool colliding = false;
    Vector3 normal;
    float penetration = 0.0f;
};

CollisionManifold3D findSphereSphereCollision(
    const Body3D& a,
    const Body3D& b
);

bool sphereSphereCollision(
    const Body3D& a,
    const Body3D& b
);

void resolveSphereCollision(
    Body3D& a,
    Body3D& b,
    const CollisionManifold3D& manifold
);

void resolveSphereCollision(Body3D& a, Body3D& b);
