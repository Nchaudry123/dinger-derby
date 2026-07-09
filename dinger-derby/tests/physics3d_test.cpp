#include <cassert>
#include <cmath>

#include "../src/collision/Collision3D.h"
#include "../src/physics/PhysicsWorld3D.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void testBody3DIntegratesForces() {
    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 2.0f);
    body.applyForce(Vector3(4.0f, 0.0f, 0.0f));
    body.update(1.0f);

    assert(body.position.x > 1.5f);
    assert(body.velocity.x > 1.5f);
}

void testSphereSphereCollisionFindsNormalAndPenetration() {
    Body3D a(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    Body3D b(Vector3(1.5f, 0.0f, 0.0f), 1.0f);
    a.radius = 1.0f;
    b.radius = 1.0f;

    CollisionManifold3D manifold = findSphereSphereCollision(a, b);

    assert(manifold.colliding);
    assert(nearlyEqual(manifold.normal.x, 1.0f));
    assert(nearlyEqual(manifold.penetration, 0.5f));
}

void testSphereResolutionSeparatesBodies() {
    Body3D a(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    Body3D b(Vector3(1.5f, 0.0f, 0.0f), 1.0f);
    a.radius = 1.0f;
    b.radius = 1.0f;

    resolveSphereCollision(a, b);

    assert((b.position - a.position).magnitude() > 1.9f);
}

void testPhysicsWorld3DGravityAndBounds() {
    PhysicsWorld3D world;
    world.setBounds(Vector3(-2.0f, -1.0f, -2.0f), Vector3(2.0f, 3.0f, 2.0f));

    Body3D body(Vector3(0.0f, -0.8f, 0.0f), 1.0f);
    body.radius = 0.5f;
    body.velocity = Vector3(0.0f, -5.0f, 0.0f);
    world.addBody(&body);
    world.step(0.5f);

    assert(body.position.y >= -0.5f);
    assert(body.velocity.y >= 0.0f);
}

}

int main() {
    testBody3DIntegratesForces();
    testSphereSphereCollisionFindsNormalAndPenetration();
    testSphereResolutionSeparatesBodies();
    testPhysicsWorld3DGravityAndBounds();

    return 0;
}
